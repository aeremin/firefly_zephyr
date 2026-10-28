"""Access to the boards of the test rack through their Black Magic Probes.

Each probe exposes two serial ports: interface 00 is its GDB server (used to
flash the board), interface 02 is the board's UART (used to read its logs).
"""

from __future__ import annotations

import re
import subprocess
import threading
import time
from dataclasses import dataclass
from pathlib import Path

import serial

# Zephyr's log backend colors its output.
_ANSI_ESCAPE = re.compile(r'\x1b\[[0-9;]*m')

FAULT_PATTERNS = (
    re.compile(r'ZEPHYR FATAL ERROR'),
    re.compile(r'ASSERTION FAIL'),
)


class UartLog:
    """Captures a serial port line by line in a background thread."""

    def __init__(self, port: str, baudrate: int = 115200) -> None:
        self._serial = serial.Serial(port, baudrate, timeout=0.1)
        self._lines: list[str] = []
        self._condition = threading.Condition()
        self._stop = threading.Event()
        self._thread = threading.Thread(target=self._read, daemon=True)
        self._thread.start()

    def _read(self) -> None:
        pending = b''
        while not self._stop.is_set():
            pending += self._serial.read(4096)
            *complete, pending = pending.split(b'\n')
            if complete:
                with self._condition:
                    for raw in complete:
                        line = raw.decode('utf-8', errors='replace')
                        self._lines.append(_ANSI_ESCAPE.sub('', line).rstrip('\r'))
                    self._condition.notify_all()

    def close(self) -> None:
        self._stop.set()
        self._thread.join()
        self._serial.close()

    def mark(self) -> int:
        """Returns a position to search from, e.g. right before flashing."""
        with self._condition:
            return len(self._lines)

    def lines(self, since: int = 0) -> list[str]:
        with self._condition:
            return self._lines[since:]

    def wait_for(self, pattern: str, since: int, timeout: float) -> re.Match[str]:
        """Waits for a line (logged after `since`) matching the regex `pattern`."""
        regex = re.compile(pattern)
        deadline = time.monotonic() + timeout
        position = since
        with self._condition:
            while True:
                for line in self._lines[position:]:
                    match = regex.search(line)
                    if match:
                        return match
                position = len(self._lines)
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    raise TimeoutError(
                        f'No line matching {pattern!r} within {timeout} s. Log:\n'
                        + '\n'.join(self._lines[since:])
                    )
                self._condition.wait(remaining)

    def faults(self, since: int) -> list[str]:
        """Lines reporting a Zephyr fatal error or failed assertion."""
        return [
            line
            for line in self.lines(since)
            if any(p.search(line) for p in FAULT_PATTERNS)
        ]


class Board:
    """A test board connected to a Black Magic Probe."""

    def __init__(self, name: str, probe: str, gdb: str) -> None:
        self.name = name
        self.probe = probe
        self._gdb = gdb
        self.uart = UartLog(f'{probe}-if02')

    def close(self) -> None:
        self.uart.close()

    def flash(self, elf: Path) -> int:
        """Flashes and starts `elf`. Returns the UART log position before flashing."""
        if not elf.is_file():
            raise FileNotFoundError(f'{elf} not found')
        since = self.uart.mark()
        result = subprocess.run(
            [
                self._gdb, '-nx', '--batch',
                '-ex', f'target extended-remote {self.probe}-if00',
                '-ex', 'monitor swdp_scan',
                '-ex', 'attach 1',
                '-ex', 'load',
                '-ex', 'compare-sections',
                '-ex', 'kill',
                str(elf),
            ],
            capture_output=True,
            text=True,
            timeout=180,
        )
        output = result.stdout + result.stderr
        if result.returncode != 0 or 'MIS-MATCHED' in output:
            raise RuntimeError(f'Flashing {elf.name} to {self.name} failed:\n{output}')
        return since

    def run(self, elf: Path, boot_line: str, timeout: float = 15) -> RunningImage:
        """Flashes `elf` and waits until it logs `boot_line` (a regex)."""
        image = RunningImage(self, self.flash(elf))
        image.wait_for(boot_line, timeout)
        return image


@dataclass
class RunningImage:
    """An image flashed to a board, with its part of the UART log."""

    board: Board
    since: int  # UART log position right before flashing

    def wait_for(self, pattern: str, timeout: float) -> re.Match[str]:
        return self.board.uart.wait_for(pattern, self.since, timeout)

    def wait_for_next(self, pattern: str, timeout: float, after: int) -> re.Match[str]:
        """Like wait_for(), but only lines logged after the position `after`."""
        return self.board.uart.wait_for(pattern, after, timeout)

    def faults(self) -> list[str]:
        return self.board.uart.faults(self.since)

    def ble_address(self, timeout: float = 15) -> str:
        """The Bluetooth identity address, as logged by Zephyr's Bluetooth host."""
        match = self.wait_for(r'Identity(?:\[\d+\])?: ([0-9A-F]{2}(?::[0-9A-F]{2}){5})', timeout)
        return match.group(1)

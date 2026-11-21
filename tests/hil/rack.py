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

# Zephyr's boot banner, printed once per boot.
BOOT_BANNER = re.compile(r'\*\*\* Booting ')
# Logged at boot by images that report why they were reset (e.g. firefly).
RESET_CAUSE = re.compile(r'Reset cause: .*')


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


class UsbPort:
    """The USB hub port a device is plugged into, switched with uhubctl."""

    def __init__(self, device: Path) -> None:
        # The device's sysfs directory, named after its path through the hubs:
        # e.g. 1-1.3.2 is on port 2 of hub 1-1.3, whose directory is the parent.
        self.device = device
        self.hub = device.parent
        self.port = int(re.split(r'[.-]', device.name)[-1])

    @classmethod
    def of_tty(cls, tty: str) -> UsbPort:
        """The port of the USB device providing the serial port `tty`, e.g. a /dev/serial/by-id path."""
        name = Path(tty).resolve(strict=True).name  # e.g. ttyACM0
        interface = Path('/sys/class/tty', name, 'device').resolve(strict=True)
        return cls(interface.parent)

    @property
    def location(self) -> str:
        """The hub's location as uhubctl names it, e.g. 1-1.3 (or 1 for the root hub usb1)."""
        return self.hub.name.removeprefix('usb')

    def hub_id(self) -> str:
        """The hub's vendor:product ID, e.g. 05e3:0608."""
        return ':'.join((self.hub / name).read_text().strip() for name in ('idVendor', 'idProduct'))

    def device_number(self) -> int | None:
        """The device's number on its bus, which changes when it reconnects. None while disconnected."""
        try:
            return int((self.device / 'devnum').read_text())
        except FileNotFoundError:
            return None

    def powered(self) -> bool:
        """Whether the hub reports the port as powered."""
        output = self._uhubctl()
        match = re.search(rf'^\s*Port {self.port}: [0-9a-f]+ (\w+)', output, re.MULTILINE)
        if not match:
            raise RuntimeError(f'No status for port {self.port} in uhubctl output:\n{output}')
        return match.group(1) != 'off'

    def set_power(self, on: bool) -> None:
        self._uhubctl('-a', 'on' if on else 'off')

    def _uhubctl(self, *args: str) -> str:
        result = subprocess.run(
            ['uhubctl', '-l', self.location, '-p', str(self.port), *args],
            capture_output=True,
            text=True,
            timeout=30,
        )
        output = result.stdout + result.stderr
        if result.returncode != 0:
            raise RuntimeError(f'uhubctl failed for port {self.port} of hub {self.location}:\n{output}')
        return output


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

    def usb_port(self) -> UsbPort:
        """The USB hub port the probe is plugged into."""
        return UsbPort.of_tty(f'{self.probe}-if00')

    def power_cycle(self, off_time: float = 3, timeout: float = 15) -> None:
        """Switches the probe's USB port off and on again with uhubctl, e.g. to recover a hung probe.

        Also restarts the board if the probe powers it.
        """
        port = self.usb_port()
        number = port.device_number()
        self.uart.close()
        try:
            port.set_power(False)
        except Exception:
            self.uart = UartLog(f'{self.probe}-if02')
            raise
        try:
            # Long enough to notice the port being switched on again behind uhubctl's back.
            time.sleep(off_time)
            stayed_off = not port.powered()
        finally:
            port.set_power(True)

        deadline = time.monotonic() + timeout
        while not (
            port.device_number() not in (None, number)
            and all(Path(f'{self.probe}-{interface}').exists() for interface in ('if00', 'if02'))
        ):
            if time.monotonic() > deadline:
                raise TimeoutError(f'The {self.name} probe did not reconnect within {timeout} s')
            time.sleep(0.1)
        self.uart = UartLog(f'{self.probe}-if02')
        if not stayed_off:
            raise RuntimeError(f'Port {port.port} of USB hub {port.location} was powered again while switched off')


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
        """Fatal errors and failed assertions, and resets after the boot that followed flashing.

        A reset without a logged fault (e.g. a brownout) only shows up as another boot banner.
        """
        lines = self.board.uart.lines(self.since)
        boots = [i for i, line in enumerate(lines) if BOOT_BANNER.search(line)]
        resets = []
        for i in boots[1:]:
            causes = (RESET_CAUSE.search(line) for line in lines[i : i + 20])
            cause = next((c.group(0) for c in causes if c), 'Reset cause not logged')
            resets.append(f'Unexpected reset ({cause}), before: {lines[i - 1]!r}')
        return self.board.uart.faults(self.since) + resets

    def ble_address(self, timeout: float = 15) -> str:
        """The Bluetooth identity address, as logged by Zephyr's Bluetooth host."""
        match = self.wait_for(r'Identity(?:\[\d+\])?: ([0-9A-F]{2}(?::[0-9A-F]{2}){5})', timeout)
        return match.group(1)

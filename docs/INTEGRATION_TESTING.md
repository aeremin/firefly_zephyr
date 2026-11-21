# Integration testing

How the firmware is (and could be) tested on real hardware, beyond the
on-target unit test images `smoke_test` and `lock_test`.

The test rack (`hardware/hitl_rack`, attached to the self-hosted Raspberry Pi CI
runner) has two Black Magic Probes, each connected to one test board
(`firefly_v1`, `lock_v01`) over SWD and UART. The probes are plugged into a
D-Link DUB-H7 USB hub (hardware revision D, USB ID `05e3:0608`), which can
switch the power of each port, so the Pi can power-cycle a probe with
[uhubctl](https://github.com/mvp/uhubctl). Everything below builds on that.

## Phase 1: real firmware on the existing rack

Status: implemented in `tests/hil/` and run by the `hardware_test` CI job,
including the Bluetooth LE tests (the runner is a Raspberry Pi 3, using its
onboard Bluetooth adapter through BlueZ).

### Running the tests

The tests need the boards' probes, configured with environment variables
holding the probe's `/dev/serial/by-id/...` path without the interface suffix
(see the `hardware_test` job in `.github/workflows/ci.yml` for the rack's
values), and the images as `<image>.elf` in one folder (as the CI build job
names them):

```bash
python3 -m venv .venv
.venv/bin/pip install -r tests/hil/requirements.txt
export FIREFLY_PROBE=/dev/serial/by-id/usb-Black_Magic_Debug_..._365938773034
export LOCK_PROBE=/dev/serial/by-id/usb-Black_Magic_Debug_..._3951376E3233
.venv/bin/pytest tests/hil --elf-dir <folder with .elf files> --ble -v
```

Options:

* `--ble`: also run the Bluetooth LE tests.
* `--usb-hub VID:PID`: also test the USB hub the probes are plugged into (CI
  uses `05e3:0608`): check that each probe is on it, and power-cycle each probe
  with uhubctl.
* `--require-all-boards`: fail instead of skipping tests for boards without a
  probe variable set (CI uses this, so a missing board can't go unnoticed).
* `--gdb`: GDB executable used to flash (default `gdb-multiarch`).

### Runner setup

One-time setup of the Raspberry Pi running the self-hosted CI runner:

* `gdb-multiarch`, `python3-venv` and `uhubctl` installed. uhubctl must be 2.5 or
  later (Raspberry Pi OS Bookworm or later): older versions can leave a
  switched-off probe's device files around, or have the port switched on again
  a few seconds later.
* The runner's user can switch the hub's ports without `sudo`: the rules from
  uhubctl's README in `/etc/udev/rules.d/52-usb.rules`, for the hub's vendor ID
  `05e3`:

  ```
  SUBSYSTEM=="usb", DRIVER=="hub|usb", MODE="0666", ATTR{idVendor}=="05e3"
  # Linux 6.0 or later (its ok to have this block present for older Linux kernels):
  SUBSYSTEM=="usb", DRIVER=="hub|usb", \
    RUN="/bin/sh -c \"chmod -f 666 $sys$devpath/*port*/disable || true\""
  ```

  then `sudo udevadm trigger --attr-match=subsystem=usb`. `uhubctl` (without
  `sudo`) lists the hub's two internal hubs (its 7 ports are two chained 4-port
  hubs), each with `ppps` (per-port power switching).
* The runner's user can access the probes' serial ports (`dialout` group) and
  BlueZ (`bluetooth` group): `sudo usermod -aG dialout,bluetooth <runner user>`,
  then restart the runner service.
* Bluetooth not blocked and powered on at boot:
  * `rfkill list bluetooth` shows `Soft blocked: no`; otherwise
    `sudo rfkill unblock bluetooth` (on Raspberry Pi OS, setting the Wi-Fi
    country with `raspi-config` also lifts the block).
  * `AutoEnable=true` in the `[Policy]` section of `/etc/bluetooth/main.conf`,
    then `sudo systemctl restart bluetooth`.
  * `bluetoothctl show` reports `Powered: yes`.

The `hardware_test` job also runs `bluetoothctl power on` before the tests and
prints the adapter state, so a powered-off adapter is visible in the log.

### What it does

Hardware tests are a pytest suite running on the Pi. For each board it:

* flashes an image through the probe's GDB server,
* captures the board's UART through the probe's second serial port,
* talks to the device over Bluetooth LE with [bleak](https://github.com/hbldh/bleak)
  (needs a Bluetooth adapter on the Pi: the onboard one on Pi 3 and later, or a
  USB dongle),
* with `--usb-hub`, power-cycles the probe through the hub's port.

Checks:

| Image                | Checks                                                               |
|----------------------|----------------------------------------------------------------------|
| `smoke_test_firefly` | All `pw_unit_test` tests pass (per-test results in the report).       |
| `lock_test`          | Same.                                                                |
| `firefly`            | Boots (startup log line, no fault or assert in the log); advertises, and again after a connection; battery level readable and in range; writing Beep / Blink produces "Beep!" / "Blink!" in the log. |

USB hub checks (`test_usb_hub.py`, with `--usb-hub`), for each probe:

* The probe is plugged into the hub (not into the Pi or another hub).
* Power cycling: uhubctl switches the probe's port off, the port is still off 3 s
  later, and after switching it on again the probe reconnects (as a new USB
  device) and can flash its board (`smoke_test_firefly` / `lock_test`, up to the
  boot banner). The power cycle is refused for a probe that isn't on the hub,
  since switching one of the Pi 3's own ports also cuts its Ethernet.

This checks what the hub reports, not that the port's power really goes off. To
check that once by hand, plug something with an LED into a port and switch it
off with `uhubctl -l <hub> -p <port> -a off` (the hub's internal hub and port
numbers, not the order of the sockets, as `uhubctl` lists them).

The real `lock` image isn't tested: it needs an MFRC522 NFC reader on the lock
board's expansion header (and stops with an assert without one), and the rack's
lock board has none.

Compared to the previous shell steps, waiting is based on log lines with
timeouts instead of fixed sleeps, and results are reported per test (JUnit
report).

The BLE device under test is identified by the address it logs at startup
("Identity: ..."), not by its name, so other devices nearby (e.g. a real
firefly on the desk) don't interfere.

## Phase 2: extend the rack (hardware work)

* **Second `firefly_v1` board** for an end-to-end radio test: `activator` on one,
  `firefly` on the other. Configure the activator over BLE, check that the
  firefly picks up the color. The color is only in a debug log line today, so
  this needs either a small test configuration raising the log level (via
  `EXTRA_CONF_FILE`, keeping the image otherwise identical), or reading the LED.
* **Stimulus / observation lines** to the boards' headers (Pi GPIOs or a small
  helper MCU): press buttons, drive the lock's keypad matrix and reed switch,
  check buzzer PWM and LED state. `lock_test` already checks the headers for
  shorts, so the wiring exists on the board side.
* **The power meter as a fixture**: put a `power_meter` board (INA226) in the
  supply of a test board to measure current, e.g. firefly's low power mode.
* **Power-on resets of the boards**: the probes can already be power-cycled
  through the hub (see above), which also resets a board powered by its probe.
  A board with its own supply needs a switched one.
* **`cordyceps`**: route its UART1 TX to a rack UART, so a test can write
  "Lock is closed" over BLE and check the `C` / `O` bytes sent to the lock.

## Phase 3: BLE tests in CI without hardware

Zephyr can run nRF52 firmware in simulation: the `nrf52_bsim` board with
[BabbleSim](https://babblesim.github.io/). A second simulated device acts as the
BLE peer, so the GATT services of the real applications (`rothult_cordyceps`,
`power_meter`, the firefly service) could be tested on every push, independent
of the Pi runner.

Cost:

* devicetree overlays standing in for the custom boards' peripherals,
* adding the BabbleSim projects (`babblesim_*`, `nrf_hw_models`) to the
  manifest allowlist,
* Linux only (fine for CI, not for local development on Windows).

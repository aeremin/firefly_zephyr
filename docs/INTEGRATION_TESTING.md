# Integration testing

How the firmware is (and could be) tested on real hardware, beyond the
on-target unit test images `smoke_test` and `lock_test`.

The test rack (`hardware/hitl_rack`, attached to the self-hosted Raspberry Pi CI
runner) has two Black Magic Probes, each connected to one test board
(`firefly_v1`, `lock_v01`) over SWD and UART. Everything below builds on that.

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
* `--require-all-boards`: fail instead of skipping tests for boards without a
  probe variable set (CI uses this, so a missing board can't go unnoticed).
* `--gdb`: GDB executable used to flash (default `gdb-multiarch`).

### Runner setup

One-time setup of the Raspberry Pi running the self-hosted CI runner:

* `gdb-multiarch` and `python3-venv` installed.
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
  USB dongle).

Checks:

| Image                | Checks                                                               |
|----------------------|----------------------------------------------------------------------|
| `smoke_test_firefly` | All `pw_unit_test` tests pass (per-test results in the report).       |
| `lock_test`          | Same.                                                                |
| `firefly`            | Boots (startup log line, no fault or assert in the log); advertises; battery level readable and in range; writing Beep / Blink produces "Beep!" / "Blink!" in the log. |
| `lock`               | Same boot and BLE checks (Beep / Blink).                              |

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
* **USB power switching** (e.g. `uhubctl`) for clean power-on resets.
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

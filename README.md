# Firefly
[![CI](https://github.com/aeremin/firefly_zephyr/actions/workflows/ci.yml/badge.svg)](https://github.com/aeremin/firefly_zephyr/actions/workflows/ci.yml)

Firmware (Zephyr RTOS / nRF Connect SDK, with [Pigweed](https://pigweed.dev/) libraries),
custom boards and KiCad hardware projects for a few small nRF52-based devices.

# Products

| Product         | PCB (`hardware/`)  | Board (`boards/arm/`) | Firmware (`firmware/`)               |
|-----------------|--------------------|-----------------------|--------------------------------------|
| Firefly         | —                  | `firefly_v1`          | `firefly`, `activator`, `smoke_test` |
| Lock            | `lock` (EasyEDA)   | `lock_v01`            | `lock`, `lock_test`                  |
| Power meter     | `usb_power_meter`  | `power_meter`         | `power_meter`                        |
| Cordyceps       | `cordyceps`        | `cordyceps`           | `rothult_cordyceps`                  |

Other hardware projects without firmware in this repository: `blackmagic`,
`esphome-nfc`, `hitl_rack`, `rothult`. Shared KiCad libraries are in `hardware/libs`.

# Repository layout

* `firmware/` — one folder per firmware image, plus shared code in `firmware/common`
  and `firmware/testing`.
* `boards/` — custom Zephyr board definitions.
* `manifest/` — west manifest (nRF Connect SDK and Pigweed revisions). The repository
  is also a Zephyr module (`manifest/zephyr/module.yml`), which makes the boards and
  shared code available to every build.
* `hardware/` — KiCad projects, with fabrication outputs in each project's `production/`.
* `docs/` — development setup and notes.

# Building and flashing

See [docs/Developing.md](docs/Developing.md) for the one-time setup (west workspace,
Pigweed environment, Python dependencies).

Then, from the repository root:
```bash
west build firmware/<image> --board <board> --pristine
```
(`--pristine` means "clean, non-incremental build" and can be omitted when rebuilding
the same image).

To flash it, use
```bash
west flash
```
Flashing with a Black Magic Probe is described in [docs/Developing.md](docs/Developing.md#black-magic-probe).

CI builds all images on every push and runs `smoke_test` and `lock_test` on real
hardware.

# Firmware images

## Firefly (`firefly`, board `firefly_v1`)
This is the "main" image.
Behaviours:
* Radio: will continously listen for the radio packets (see packet structure
  [here](firmware/common/magic_path_packet.h)).
  This is compatible with Locket firmware from [this branch](https://github.com/aeremin/Locket_fw/tree/7Colors) and
  with the Activator firmware described below. LED color will change according to packets received.
* Bluetooth: presents itself as connectable BLE device. Provides 2 BLE services:
  * Standard battery service, containing battery level (0-100) characteristic.
  * Custom service (UUID `8ec87060-8865-4eca-82e0-2ea8e45e8221`) exposing following characteristics:
    * "Beep" (UUID `8ec87062-8865-4eca-82e0-2ea8e45e8221`). Write-only characteristic, writing a byte to it will trigger short beep,
      byte written controls the volume.
    * "Blink" (UUID `8ec87063-8865-4eca-82e0-2ea8e45e8221`). Write-only characteristic, writing anything will trigger short series
      of blinks.

  Both of the services can be used by the [Ostranna configurator](https://github.com/aeremin/ostranna_configurator) Android app.
* Battery measurement. Will continously monitor battery voltage and
  * Report it via bluetooth - see above.
  * Will disable radio and LED (will keep Bluetooth enabled) if battery level is low.

## Activator (`activator`, board `firefly_v1`)
This image can be used to trigger Firefly one or to provide some settings for it.
Behaviours:
* Radio: will continously transmit radio packets (see packet structure
  [here](firmware/common/magic_path_packet.h)).
* LED: will have a color corresponding to foreground color of packet being transmitted.
* Bluetooth: presents itself as connectable BLE device. Provides 2 BLE services:
  * Standard battery service, containing battery level (0-100) characteristic.
  * Custom service (UUID `8ec87060-8865-4eca-82e0-2ea8e45e8221`) exposing readable and writable characteristics corresponding to the
    fields of radio packet being transmitted. This allows to control the packet from e.g. phone.

  Both of the services can be used by the [Ostranna configurator](https://github.com/aeremin/ostranna_configurator) Android app.

## Lock (`lock`, board `lock_v01`)
* Bluetooth: same custom service as Firefly, with "Beep" and "Blink" characteristics.
* Reads NFC tag UIDs with an MFRC522 reader and logs them.
* Handles a matrix keyboard, a reed switch and two buttons.

## Power meter (`power_meter`, board `power_meter`)
* Measures voltage, current and power with an INA226 and logs them.
* Bluetooth: custom service (UUID `a0c272ef-1280-4a64-8c5b-2833b948fc96`) with a
  readable and writable "Power On" characteristic that switches the output.

## Cordyceps (`rothult_cordyceps`, board `cordyceps`)
* Bluetooth: custom service (UUID `ab12c725-0a0e-4c06-9968-2b9fff64b504`) with
  * "Power On" characteristic (readable and writable) that switches the power output.
  * "Lock is closed" characteristic (readable and writable). Writing it sends a
    close/open command to the lock over UART.

## Tests (`smoke_test`, `lock_test`)
On-target test images (Pigweed `pw_unit_test`), run by CI on real hardware. They
print the test log to the UART/RTT and end with "All tests passed!" on success.
* `smoke_test` (board `firefly_v1`): CC1101 radio, RGB LED, timers.
* `lock_test` (board `lock_v01`): RGB LED, timers, EEPROM, expansion headers, threads,
  Pigweed RPC.

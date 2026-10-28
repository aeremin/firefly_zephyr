# Repository structure review

Review of the repository layout, build setup, CI and docs, with suggested
improvements. Items are grouped by area and roughly ordered by priority within
each group; each has an ID (e.g. `C1`) so it can be referenced. Items that are
done have been removed; IDs of the remaining ones are kept as they were.

* **Partially done** — some of it is fixed; the remaining part is described.
* **Open** — still valid as written.

| ID | Item                                           | Status         |
|----|------------------------------------------------|----------------|
| B5 | Consistent default boards                      | Open           |
| D2 | Shared Kconfig fragment                        | Open           |
| L1 | Split `firmware/common` by purpose             | Open           |
| L2 | Fix ODR violation in `eeprom.h`                | Open           |
| C1 | Fix the release step                           | Partially done |
| C2 | Replace copy-pasted CI steps                   | Open           |
| C5 | Deduplicate the hardware test job              | Open           |
| C6 | Make CI install Python deps like local setup   | Open           |
| T1 | Host-side unit tests                           | Open           |
| H5 | Consolidate symbol libraries                   | Open           |
| H6 | Merge the IDE launcher scripts                 | Open           |
| N1 | Align product names across trees               | Open           |
| N2 | Refresh `README.md`                            | Open           |
| N3 | Update `RELEASE.md`                            | Open           |

Suggested next steps: **C1/C2** (fix the release step, deduplicate CI) and
**L2** (ODR violation in `eeprom.h`).

## B. Build and dependency structure

### B5. Consistent default boards — open

`firefly` and `activator` hardcode `set(BOARD firefly_v1)`; the other apps
require `--board`. CI passes `--board` for every app, so the hardcoded
defaults only matter locally. Pick one convention (a default per app is the
friendlier option).

## D. Duplication

### D2. Shared Kconfig fragment — open

Every `prj.conf` repeats the same C++ / newlib / debug block. Move it to a
shared fragment (e.g. `firmware/common/cpp.conf`) included with
`list(APPEND EXTRA_CONF_FILE ...)` before `find_package(Zephyr)`, or have an
option in the module's Kconfig (`firmware/Kconfig`) `select` those settings.

## L. `firmware/common` layout

### L1. Split `firmware/common` by purpose — open

It is a flat mix of unrelated code. Suggested split:

| Folder         | Contents                                                                                                      |
|----------------|---------------------------------------------------------------------------------------------------------------|
| `drivers/`     | `cc1101*`, `st25r3911b*`, `generic_device.h`, `eeprom.h` (candidates for real Zephyr drivers with DT bindings) |
| `lib/`         | `timer`, `thread`, `scoped_mutex_lock.h`, `bitmap_enum.h`, `color`                                            |
| `peripherals/` | `rgb_led`, `buzzer`, `keyboard`, `battery`, `sequences.h`                                                     |
| `protocols/`   | `bluetooth`, `magic_path_packet.h`, `nfc*`, `rpc/` proto                                                      |

### L2. Fix ODR violation in `eeprom.h` — open

`firmware/common/eeprom.h` defines a non-`inline` global (`kEepromDevice`) and
a non-`inline` function (`EnablePower`) in a header. As soon as two
translation units in one image include it, linking fails. Mark them `inline`
or move them into a `.cpp`.

## C. CI (`.github/workflows/ci.yml`)

### C1. Fix the release step — partially done

Everything is `.elf` now, and the release step lists
`firefly.elf, activator.elf, smoke_test.elf, smoke_test_firefly.elf`.
Remaining:

* `smoke_test.elf` is never produced (the only smoke test artifact is
  `smoke_test_firefly.elf`).
* `lock.elf`, `lock_test.elf`, `power_meter.elf` and `rothult_cordyceps.elf`
  are built but not attached to releases.
* `RELEASE.md`, used as the release body, still describes flashing `.hex`
  files (see N3).

### C2. Replace copy-pasted CI steps — open

The `build` job has seven near-identical build/upload steps, and the
`store_artifacts` job repeats a download/upload pair for each of the same seven
images. Use a job matrix of `{app, board}` (build and store artifacts per
matrix entry), or add `sample.yaml` / `testcase.yaml` to each app and run
`west twister -T firmware`, which also gives a single local "build everything"
command.

For twister, every board needs a `<board>.yaml` with test metadata:
`firefly_v1` and `lock_v01` have one, `cordyceps` and `power_meter` don't, and
the existing two list an `xtools` toolchain that isn't used here.

### C5. Deduplicate the hardware test job — open

Black Magic Probe serial paths are hardcoded twice. Move them into job `env`
variables, or into a `scripts/flash_and_check.sh` usable both from CI and
locally.

### C6. Make CI install Python deps like the local setup — open

Each `run:` step in GitHub Actions is a fresh shell, so the environment from
`source third_party/pigweed/bootstrap.sh` (and `zephyr-env.sh`) doesn't carry
over to the following steps. The `pip3 install -r ...` steps therefore install
the Zephyr / NCS requirements outside the Pigweed venv, unlike the documented
local setup in `docs/Developing.md`. That's likely why CI doesn't hit the
`protobuf` downgrade that breaks `lock_test` locally (the NCS requirements pin
`protobuf` 4.x, Pigweed needs ~6.33), and it means CI and local builds use
different Python environments. Run bootstrap/activate and the `pip install`
steps in one step (or export the environment via `$GITHUB_ENV` /
`$GITHUB_PATH`), and pin `protobuf` the same way as locally.

## T. Tests

### T1. Host-side unit tests — open

`smoke_test` and `lock_test` only run on real hardware. Pure-logic code
(`color`, `nfc_definitions`, `bitmap_enum`, `sequences`) could get host tests on
`native_sim` with `pw_unit_test`, running in every CI build without hardware.
Put them under a top-level `tests/` folder, separate from on-target images.

## H. Repository hygiene

### H5. Consolidate symbol libraries — open

`hardware/Ostranna.kicad_sym` and `hardware/libs/Common.kicad_sym` live in
different places; put both under `hardware/libs/`.

### H6. Merge the IDE launcher scripts — open

`develop_in_clion.sh` and `develop_in_cursor.sh` duplicate the Pigweed
bootstrap and toolchain environment setup, and have already drifted
(`ZEPHYR_BASE` is only set in the Cursor one). Move the shared part into one
script (e.g. `scripts/env.sh`) that both source, or into a single script that
takes the editor as an argument.

## N. Naming and docs

### N1. Align product names across trees — open

The same product has different names in `hardware/`, `boards/` and
`firmware/`, e.g.:

* `hardware/cordyceps` → `boards/arm/cordyceps` → `firmware/rothult_cordyceps`
* `hardware/usb_power_meter` → `boards/arm/power_meter` → `firmware/power_meter`

Either align the names or add a product table to the README mapping
product → PCB → board → app → default board.

### N2. Refresh `README.md` — open

* Says Zephyr 3.4.0 and `west init --mr v3.4.0`, contradicting the
  manifest-based flow in `docs/Developing.md` (the manifest uses NCS 3.2.4).
* Links to `common/magic_path_packet.h`, which has moved to `firmware/common/`.
* Doesn't mention lock, power_meter or cordyceps.

Suggestion: trim the README to an overview + product table and point to
`docs/Developing.md` for setup.

### N3. Update `RELEASE.md` — open

Used as the GitHub release body. Describes flashing `.hex` files via nRF
Programmer with pca10040/pca10056 devkits, while releases now ship `.elf` files
and the main workflow is Black Magic Probe / J-Link.

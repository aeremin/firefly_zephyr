# Repository structure review

Review of the repository layout, build setup, CI and docs, with suggested
improvements. Items are grouped by area and roughly ordered by priority within
each group; each has an ID (e.g. `B1`) so it can be referenced.

Suggested starting point: **B1** (Zephyr module), **B2** (pin Pigweed) and
**C1/C2** (fix CI release step, matrix build). Together they remove most of the
CMake boilerplate, stop surprise nightly breakages and fix tagged releases.

## B. Build and dependency structure

### B1. Make the repository a Zephyr module

Today `firmware/common/basics.cmake` sets `BOARD_ROOT` via a relative path and
every app does `add_subdirectory(../common common EXCLUDE_FROM_ALL)`.

Instead, add `zephyr/module.yml` at the repo root:

```yaml
build:
  cmake: firmware/common
  kconfig: firmware/common/Kconfig
  settings:
    board_root: .
    dts_root: .
```

The manifest repository is part of the west workspace, so it will be picked up
as a module automatically. Then:

* Apps reduce to `find_package(Zephyr)` + `target_link_libraries(app ...)`.
* Each common library gets a Kconfig switch (`CONFIG_FIREFLY_CC1101=y`, ...)
  instead of being listed by hand in each app's `CMakeLists.txt`.
* The `BOARD_ROOT` hack in `basics.cmake` goes away.

### B2. Pin Pigweed

`manifest/west.yml` uses `revision: main` for Pigweed. Combined with the daily
cron CI run, builds can break overnight with no change in this repo. Pin it to a
specific commit hash and bump it deliberately.

### B3. Bring the local workspace in line with the manifest

The manifest requests sdk-nrf v2.7.0 (Zephyr ~3.6), but the local
`third_party/zephyr/VERSION` says 3.4.99 and the local Pigweed checkout is from
November 2023. A `west update` is due and may surface breakage.

### B4. Migrate boards to hardware model v2

Boards use the legacy `boards/arm/<board>/` layout with `Kconfig.board`. Newer
NCS/Zephyr versions use `boards/<vendor>/<board>/board.yml`, and the old layout
is deprecated. Migrating all five boards together is cheap.

### B5. Consistent default boards

`firefly` and `activator` hardcode `set(BOARD firefly_v1)`; the other apps
require `--board`. Pick one convention (a default per app is the friendlier
option).

## D. Duplication

### D1. Single definition of `custom_library()`

The helper is copy-pasted in `firmware/common/CMakeLists.txt` and
`firmware/testing/CMakeLists.txt`. Define it once (e.g. in `basics.cmake`), or
make it unnecessary via B1.

### D2. Shared Kconfig fragment

Every `prj.conf` repeats the same C++ / newlib / debug block. Move it to a
shared fragment (e.g. `firmware/common/cpp.conf`) included with
`list(APPEND EXTRA_CONF_FILE ...)` before `find_package(Zephyr)`, or have a
module Kconfig option `select` those settings (with B1).

### D3. Consistent library target names

Some targets are `common.keyboard`, `common.nfc`; others are plain `timer`,
`rgb_led`, `cc1101`. Pick one prefix.

## L. `firmware/common` layout

### L1. Split `firmware/common` by purpose

It is currently a flat mix of unrelated code. Suggested split:

| Folder         | Contents                                                                                                      |
|----------------|---------------------------------------------------------------------------------------------------------------|
| `drivers/`     | `cc1101*`, `st25r3911b*`, `generic_device.h`, `eeprom.h` (candidates for real Zephyr drivers with DT bindings) |
| `lib/`         | `timer`, `thread`, `scoped_mutex_lock.h`, `bitmap_enum.h`, `color`                                            |
| `peripherals/` | `rgb_led`, `buzzer`, `keyboard`, `battery`, `sequences.h`                                                     |
| `protocols/`   | `bluetooth`, `magic_path_packet.h`, `nfc*`, `rpc/` proto                                                      |

### L2. Fix ODR violation in `eeprom.h`

`firmware/common/eeprom.h` defines a non-`inline` global (`kEepromDevice`) and
non-`inline` functions (`EnablePower`) in a header. As soon as two translation
units in one image include it, linking fails. Mark them `inline` or move them
into a `.cpp`.

### L3. Remove committed log files from `firmware/common/rpc/`

`pw_console-logs.txt` (38 KB) and the empty `pw_console-device-logs.txt` are
console logs. Delete them and add the pattern to `.gitignore`.

## C. CI (`.github/workflows/ci.yml`)

### C1. Fix the release step

It publishes `firefly.hex, activator.hex, smoke_test.hex,
smoke_test_firefly.hex`, but the build job only produces `.elf` files (plus
`lock.hex`). Tagged releases currently publish nothing useful.

### C2. Replace copy-pasted build steps

Seven near-identical build/upload steps. Use a job matrix of `{app, board}`, or
better, add `sample.yaml` / `testcase.yaml` to each app and run
`west twister -T firmware`, which also gives a single local "build everything"
command.

### C3. Cache dependencies

Every run does a full `west update` and Pigweed bootstrap. Cache `third_party/`
and the Pigweed environment, keyed on `manifest/west.yml`.

### C4. Update actions

`actions/checkout@v1` is very old; move to `@v4`.

### C5. Deduplicate the hardware test job

Black Magic Probe serial paths are hardcoded twice. Move them into job `env`
variables, or into a `scripts/flash_and_check.sh` usable both from CI and
locally.

## T. Tests

### T1. Host-side unit tests

`smoke_test` and `lock_test` only run on real hardware. Pure-logic code
(`color`, `nfc_definitions`, `bitmap_enum`, `sequences`) could get host tests on
`native_sim` with `pw_unit_test`, running in every CI build without hardware.
Put them under a top-level `tests/` folder, separate from on-target images.

## H. Repository hygiene

### H1. Untrack files that shouldn't be in git

* `.vscode/.cortex-debug.*.state.json` (per-user debugger state)
* `research/voltcraft/.idea/`
* `hardware/libs/Common.bak`

### H2. Clean local leftovers

`firmware/microkeyboard/` contains only a `build/` directory;
`firmware/smoke_test/build/` is stale.

### H3. Dead or incomplete entries

* The `locket` board isn't used by any app or CI job: delete it or add an app.
* `.vscode/settings.json` (`nrf-connect.applications`) lists six apps and is
  missing `rothult_cordyceps`.
* `rothult_cordyceps` and `power_meter` don't use `common` at all. That's fine,
  but worth stating so it doesn't look like an omission.

### H4. Consistent policy for KiCad production outputs

Most boards commit `production/`, while `hitl_rack` and `usb_power_meter`
ignore it. Choose one policy (e.g. commit only for released revisions, or
attach to GitHub releases).

### H5. Consolidate symbol libraries

`hardware/Ostranna.kicad_sym` and `hardware/libs/Common.kicad_sym` live in
different places; put both under `hardware/libs/`.

## N. Naming and docs

### N1. Align product names across trees

The same product has different names in `hardware/`, `boards/` and
`firmware/`, e.g.:

* `hardware/cordyceps` → `boards/arm/cordyceps` → `firmware/rothult_cordyceps`
* `hardware/usb_power_meter` → `boards/arm/power_meter` → `firmware/power_meter`

Either align the names or add a product table to the README mapping
product → PCB → board → app → default board.

### N2. Refresh `README.md`

* Says Zephyr 3.4.0 and `west init --mr v3.4.0`, contradicting the
  manifest-based flow in `docs/Developing.md`.
* Links to `common/magic_path_packet.h`, which has moved to `firmware/common/`.
* Doesn't mention lock, power_meter or cordyceps.

Suggestion: trim the README to an overview + product table and point to
`docs/Developing.md` for setup.

### N3. Update `RELEASE.md`

Used as the GitHub release body. Describes flashing via nRF Programmer with
pca10040/pca10056 devkits, which no longer matches the main workflow
(Black Magic Probe / J-Link).

# Repository structure review

Review of the repository layout, build setup, CI and docs, with suggested
improvements. Items are grouped by area and roughly ordered by priority within
each group; each has an ID (e.g. `B1`) so it can be referenced.

Status is relative to `origin/master` at `51d7885` ("Remove the voltcraft
stuff"):

* **Done** — fixed upstream, kept here for the record.
* **Partially done** — some of it fixed upstream; the remaining part is
  described.
* **Open** — still valid as written.

| ID | Item                                           | Status         |
|----|------------------------------------------------|----------------|
| B1 | Make the repository a Zephyr module            | Partially done |
| B2 | Pin Pigweed                                    | Done           |
| B3 | Bring local workspace in line with manifest    | Done           |
| B4 | Migrate boards to hardware model v2            | Done           |
| B5 | Consistent default boards                      | Open           |
| D1 | Single definition of `custom_library()`        | Open           |
| D2 | Shared Kconfig fragment                        | Open           |
| D3 | Consistent library target names                | Open           |
| L1 | Split `firmware/common` by purpose             | Open           |
| L2 | Fix ODR violation in `eeprom.h`                | Open           |
| L3 | Remove committed log files from `rpc/`         | Done           |
| C1 | Fix the release step                           | Partially done |
| C2 | Replace copy-pasted CI steps                   | Open (worse)   |
| C3 | Cache dependencies                             | Done           |
| C4 | Update actions                                 | Partially done |
| C5 | Deduplicate the hardware test job              | Open           |
| C6 | Make CI install Python deps like local setup   | Open (new)     |
| T1 | Host-side unit tests                           | Open           |
| H1 | Untrack files that shouldn't be in git         | Partially done |
| H2 | Clean local leftovers                          | Open           |
| H3 | Dead or incomplete entries                     | Open           |
| H4 | Consistent policy for KiCad production outputs | Open           |
| H5 | Consolidate symbol libraries                   | Open           |
| H6 | Merge the IDE launcher scripts                 | Open (new)     |
| N1 | Align product names across trees               | Open           |
| N2 | Refresh `README.md`                            | Open           |
| N3 | Update `RELEASE.md`                            | Open           |

Suggested starting point: **B1** (finish the Zephyr module, which also resolves
D1 and D3) and **C1/C2** (fix the release step,
deduplicate CI).

## B. Build and dependency structure

### B1. Make the repository a Zephyr module — partially done

Done upstream: `manifest/zephyr/module.yml` declares the repository as a Zephyr
module with `board_root: ..`, so west itself (and sysbuild, twister, IDEs) sees
the custom boards. It lives in `manifest/` because that's the west manifest
project path (`west init -l manifest`), and west only discovers modules at
project roots.

Remaining:

* `firmware/common/basics.cmake` still does
  `list(APPEND BOARD_ROOT ${CMAKE_CURRENT_LIST_DIR}/../..)`, which is now
  redundant with the module's `board_root`. Remove it.
* Shared code is still pulled in by every app with
  `add_subdirectory(../common common EXCLUDE_FROM_ALL)` and listed by hand in
  `target_link_libraries`. Add `cmake:` / `kconfig:` entries to `module.yml`
  pointing at `../firmware`, build `firmware/common` and `firmware/testing` as
  Zephyr libraries, and let apps enable each piece with a
  `CONFIG_FIREFLY_*` option in `prj.conf` (each option `select`s the Pigweed
  libraries it needs). App `CMakeLists.txt` files then only list `main.cpp`
  and Pigweed-specific targets (`pw_unit_test.light`, RPC protos, ...).
* Pigweed's Kconfig symbols changed in the pinned revision
  (e.g. `PIGWEED_ASSERT` → `PIGWEED_ASSERT_ZEPHYR`), so check the names of any
  `PIGWEED_*` symbols selected from the new Kconfig against `ccbb96c`.

### B2. Pin Pigweed — done

`manifest/west.yml` pins Pigweed instead of following `main`, now with the
full 40-character hash (`ccbb96c4dcd8...`). The short hash used before only
resolved because west fetched all branches; it can't be fetched directly,
which shallow updates (C3) need.

### B3. Bring the local workspace in line with the manifest — done

The local workspace was on NCS 2.5 (Zephyr 3.4.99) with a November 2023
Pigweed, so `lock`, `lock_test` and `smoke_test` didn't build locally. It's now
on sdk-nrf v3.2.4 (Zephyr 4.2.99) and Pigweed `ccbb96c`, with Pigweed
re-bootstrapped (Python 3.10 venv) and Zephyr / NCS / MCUboot requirements
installed. All seven CI images build locally.

One trap found on the way: the NCS requirements downgrade `protobuf` to 4.x,
which breaks Pigweed's protobuf code generation in `lock_test`. Reinstalling
`protobuf~=6.33.4` (the version Pigweed declares) fixes it.
`docs/Developing.md` now covers this, plus how to update the workspace after a
manifest bump. See also C6.

### B4. Migrate boards to hardware model v2 — done

Done upstream: `cordyceps`, `firefly_v1`, `lock_v01` and `power_meter` have
`board.yml` and `Kconfig.<board>` and build on NCS 3.2.4.

The `locket` board, which was still in the legacy format and had no
firmware using it, has been deleted.

Left as is on purpose: the boards declare `vendor: arm` and live in
`boards/arm/`. `arm` is the vendor prefix for Arm Ltd, so board listings
attribute them to Arm, but it has no effect on builds. A custom vendor
(e.g. `boards/ostranna/<board>/` with a `dts/bindings/vendor-prefixes.txt`
entry in the module) would fix that if it ever matters.

### B5. Consistent default boards — open

`firefly` and `activator` hardcode `set(BOARD firefly_v1)`; the other apps
require `--board`. CI now passes `--board` for every app, so the hardcoded
defaults only matter locally. Pick one convention (a default per app is the
friendlier option).

## D. Duplication

### D1. Single definition of `custom_library()` — open

The helper is copy-pasted in `firmware/common/CMakeLists.txt` and
`firmware/testing/CMakeLists.txt`. Finishing B1 makes it unnecessary
(plain `zephyr_library_*` calls); otherwise define it once in `basics.cmake`.

### D2. Shared Kconfig fragment — open

Every `prj.conf` repeats the same C++ / newlib / debug block. Move it to a
shared fragment (e.g. `firmware/common/cpp.conf`) included with
`list(APPEND EXTRA_CONF_FILE ...)` before `find_package(Zephyr)`, or have a
module Kconfig option `select` those settings (with B1).

### D3. Consistent library target names — open

Some targets are `common.keyboard`, `common.nfc`; others are plain `timer`,
`rgb_led`, `cc1101`. Finishing B1 reduces them to two libraries
(`firefly_common`, `firefly_testing`) with `CONFIG_FIREFLY_*` options.

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

Upstream changed the body of `EnablePower()` (now guarded by
`DT_NODE_EXISTS(DT_ALIAS(eeprom_en))`), but `firmware/common/eeprom.h` still
defines a non-`inline` global (`kEepromDevice`) and a non-`inline` function
(`EnablePower`) in a header. As soon as two translation units in one image
include it, linking fails. Mark them `inline` or move them into a `.cpp`.

### L3. Remove committed log files from `firmware/common/rpc/` — done

`pw_console-logs.txt` (38 KB) and the empty `pw_console-device-logs.txt` were
`pw console` logs. They're deleted, and `.gitignore` now has
`pw_console-*logs.txt`.

## C. CI (`.github/workflows/ci.yml`)

### C1. Fix the release step — partially done

Upstream switched everything to `.elf`, and the release step now lists
`firefly.elf, activator.elf, smoke_test.elf, smoke_test_firefly.elf`.
Remaining:

* `smoke_test.elf` is never produced (the only smoke test artifact is
  `smoke_test_firefly.elf`).
* `lock.elf`, `lock_test.elf`, `power_meter.elf` and `rothult_cordyceps.elf`
  are built but not attached to releases.
* `RELEASE.md`, used as the release body, still describes flashing `.hex`
  files (see N3).

### C2. Replace copy-pasted CI steps — open (worse)

The `build` job has seven near-identical build/upload steps, and the new
`store_artifacts` job repeats a download/upload pair for each of the same seven
images. Use a job matrix of `{app, board}` (build and store artifacts per
matrix entry), or add `sample.yaml` / `testcase.yaml` to each app and run
`west twister -T firmware`, which also gives a single local "build everything"
command.

For twister, every board needs a `<board>.yaml` with test metadata:
`firefly_v1` and `lock_v01` have one, `cordyceps` and `power_meter` don't, and
the existing two list an `xtools` toolchain that isn't used here.

### C3. Cache dependencies — done

Every run used to spend ~230 s on `west update` and ~200 s on the Pigweed
bootstrap, out of ~12.5 min for the `build` job. Caching everything as is
wasn't an option: the active west projects with full history are ~6.5 GB and
the Pigweed environment several GB more, against GitHub's 10 GB cache limit
per repository.

Now the `build` job:

* Takes only the sdk-nrf projects the images need (`name-allowlist` in
  `manifest/west.yml`: 7 of ~50 imported projects; dropping the rest doesn't
  change any image by a single byte).
* Fetches west projects shallow (`west update --narrow -o=--depth=1`): only
  the pinned revisions, no history. Together with the allowlist that's
  ~1.2 GB on disk, ~0.5 GB compressed (measured in a dry run of the same
  command; without the allowlist it was 2.6 GB / 1.2 GB).
* Caches `third_party/` without the Pigweed environment, keyed on
  `manifest/west.yml`; on a hit `west update` is skipped.
* Caches `third_party/pigweed/environment` separately, keyed on
  `manifest/west.yml` with a fallback to the newest older environment; on an
  exact hit the bootstrap is skipped (`activate.sh` used by the build steps
  works on a restored environment), otherwise bootstrap only downloads what
  changed.

Any revision bump in the manifest invalidates both caches. The Pigweed
environment is the bigger of the two, so the 10 GB limit leaves room for
roughly two generations; GitHub evicts the least recently used entries.

Not cached: APT packages (~35 s) and pip installs (~30 s).

### C4. Update actions — partially done

`upload-artifact` and `download-artifact` were bumped (v7/v8). All three jobs
still use `actions/checkout@v1`; move to `@v4` or later.

### C5. Deduplicate the hardware test job — open

Black Magic Probe serial paths are hardcoded twice. Move them into job `env`
variables, or into a `scripts/flash_and_check.sh` usable both from CI and
locally.

### C6. Make CI install Python deps like the local setup — open (new)

Each `run:` step in GitHub Actions is a fresh shell, so the environment from
`source third_party/pigweed/bootstrap.sh` (and `zephyr-env.sh`) doesn't carry
over to the following steps. The `pip3 install -r ...` steps therefore install
the Zephyr / NCS / MCUboot requirements outside the Pigweed venv, unlike the
documented local setup in `docs/Developing.md`. That's likely why CI doesn't hit
the `protobuf` downgrade from B3, and it means CI and local builds use
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

### H1. Untrack files that shouldn't be in git — partially done

`research/voltcraft/` (including its `.idea/`) was removed upstream.
Still tracked:

* `.vscode/.cortex-debug.*.state.json` (per-user debugger state)
* `hardware/libs/Common.bak`

### H2. Clean local leftovers — open

Not in git, but present in the working copy: `firmware/microkeyboard/`
(contains only `build/`), and `firmware/smoke_test/build/` plus
`firmware/smoke_test/cmake-build-debug/`.

### H3. Dead or incomplete entries — open

* ~~The `locket` board isn't used by any app or CI job~~ — deleted (B4).
* `.vscode/settings.json` (`nrf-connect.applications`) lists six apps and is
  missing `rothult_cordyceps`.
* `rothult_cordyceps` and `power_meter` don't use `common` at all. That's fine,
  but worth stating so it doesn't look like an omission.

### H4. Consistent policy for KiCad production outputs — open

`blackmagic`, `cordyceps`, `esphome-nfc` and `rothult` commit `production/`,
while `hitl_rack` and `usb_power_meter` ignore it. Choose one policy (e.g.
commit only for released revisions, or attach to GitHub releases).

### H5. Consolidate symbol libraries — open

`hardware/Ostranna.kicad_sym` and `hardware/libs/Common.kicad_sym` live in
different places; put both under `hardware/libs/`.

### H6. Merge the IDE launcher scripts — open (new)

`develop_in_clion.sh` and the new `develop_in_cursor.sh` duplicate the Pigweed
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
  manifest-based flow in `docs/Developing.md` (and the manifest now uses
  NCS 3.2.4).
* Links to `common/magic_path_packet.h`, which has moved to `firmware/common/`.
* Doesn't mention lock, power_meter or cordyceps.

Suggestion: trim the README to an overview + product table and point to
`docs/Developing.md` for setup.

### N3. Update `RELEASE.md` — open

Used as the GitHub release body. Describes flashing `.hex` files via nRF
Programmer with pca10040/pca10056 devkits, while releases now ship `.elf` files
and the main workflow is Black Magic Probe / J-Link.

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
| B6 | Bump NCS to 3.4 (blocked on Pigweed)           | Blocked        |
| D2 | Shared Kconfig fragment                        | Open           |
| L1 | Split `firmware/common` by purpose             | Open           |
| T1 | Host-side unit tests                           | Open           |
| T2 | `lock_test` never starts its RPC server        | Open           |
| H7 | Missing `Common.pretty` footprint library      | Open           |
| N1 | Align product names across trees               | Partially done |

Suggested next step: **B6** once Pigweed fixes its Zephyr 4.4 integration.

## B. Build and dependency structure

### B6. Bump NCS to 3.4 — blocked on Pigweed

NCS v3.4.1 (LTS, Zephyr 4.4.2) needs Python 3.12+, which the project's
Pigweed bootstrap config now provides (Python 3.13). With it, six of seven
images build; `lock_test` doesn't. Already in place: `full_name` in every
`board.yml`, and the Python / Pigweed setup. Also needed for 3.4, not applied
yet because 3.2.6 doesn't need it: `lock_test/prj.conf` must stop enabling
`CONFIG_PIGWEED_RPC_SERVER` (and enable `CONFIG_PIGWEED_SYNC_MUTEX` instead).
That option links the `pw_rpc` libraries into `zephyr_interface`; as they depend
on the `pw_log` backend, CMake reports a dependency cycle with Zephyr 4.4's
generated headers. `lock_test` links the `pw_rpc` libraries in its
`CMakeLists.txt` anyway.

The blocker: Zephyr 4.4's `kernel.h` includes the generated
`zephyr/heap_constants.h`. Pigweed libraries enabled with `CONFIG_PIGWEED_*`
are linked into `zephyr_interface`, which the object library generating that
header links too, so CMake always builds those Pigweed libraries before the
header exists. Libraries that include `kernel.h` fail to compile: in
`lock_test`, Pigweed's thread, sync and chrono backends. A file-level ordering
dependency doesn't help (Ninja reports a cycle).

Pigweed fixed the same problem for `pw_log_zephyr` / `pw_assert_zephyr` in
[79c2915087](https://pigweed.googlesource.com/pigweed/pigweed/+/79c2915087b58c0d34f02df2ae4018d69add2b79)
([review](https://pigweed-review.googlesource.com/c/pigweed/pigweed/+/462885)):
only facades go into `zephyr_interface`, static backends are linked into the
application with a dependency on the generated headers. `pw_thread_zephyr`,
`pw_sync_zephyr` and `pw_chrono_zephyr` need the same treatment, upstream or
as a workaround in this repository's module.

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

## T. Tests

### T1. Host-side unit tests — open

`smoke_test` and `lock_test` only run on real hardware. Pure-logic code
(`color`, `nfc_definitions`, `bitmap_enum`, `sequences`) could get host tests on
`native_sim` with `pw_unit_test`, running in every CI build without hardware.
Put them under a top-level `tests/` folder, separate from on-target images.
Good candidates beyond those: firefly's `PacketsLog` color mixing / timeout
(needs an injectable clock instead of `k_uptime_get()`) and the battery voltage
to level formula, both currently inside `firmware/firefly/main.cpp`.

### T2. `lock_test` never starts its RPC server — open

In `firmware/lock_test/main.cpp`, `main()` runs the tests and then loops
forever (`while (true) sleep_for(1s)`); the RPC service registration and the
RPC dispatch thread come after that loop, so they're unreachable.
`rpc_example.py` and the `pw_log` RPC service can't work against this image.
Start the RPC thread before the loop (and decide whether the hardware tests
should exercise it, see `docs/INTEGRATION_TESTING.md`).

## H. Repository hygiene

### H7. Missing `Common.pretty` footprint library — open

The `fp-lib-table` of `cordyceps` and `esphome-nfc` points at
`hardware/libs/Common.pretty`, which isn't in the repository (and never was).
The boards still open, since KiCad embeds footprints in the `.kicad_pcb`, but
updating footprints from the library or reusing them in a new board won't work.
Commit the library, or drop the table entry if those footprints aren't needed.

## N. Naming and docs

### N1. Align product names across trees — partially done

The same product has different names in `hardware/`, `boards/` and
`firmware/`, e.g.:

* `hardware/cordyceps` → `boards/arm/cordyceps` → `firmware/rothult_cordyceps`
* `hardware/usb_power_meter` → `boards/arm/power_meter` → `firmware/power_meter`

The README now has a product table mapping product → PCB → board → firmware,
so the names are discoverable. Remaining (optional): align the names
themselves.

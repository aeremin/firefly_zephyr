# Set up

## First time set up

### Overview

Setup is somewhat convoluted as we want to force multiple things to work
together nicely:
* Nordic's version of Zephyr RTOS. We can use mainstream Zephyr as well,
  but it lacks some Nordic-specific stuff (e.g. DFU-related things).
* nRF Connect for VS Code. Nice to have it working for the application and board
  management in VS Code, properly working flashing & debugging, etc.
* [Pigweed](https://pigweed.dev/). We use many of the libraries, some of them are
  pretty unique (e.g. RPC-via-serial support). For better or worse, Pigweed comes
  with it's own toolchain and python virtual environment. If we want Pigweed tooling
  and python scripts to work - we **must** use this virtual environment, in
  particular we need to force nRF Connect for VS Code to also use it.

### Steps

* Install [nRF Connect for Desktop](https://www.nordicsemi.com/Products/Development-tools/nrf-connect-for-desktop).
* Install nRF Connect for VS Code. It can also be done later from the VS Code itself -
  just install recommended extensions defined in `.vscode/extensions.json` - VS Code will propose to do that.
* Install `west` globally: `pip install west --break-system-packages` (need to be run with administrator priviliges).
* Run `west init -l manifest && west update && west zephyr-export` (from the repository root).
  This will create and populate `third_party` folder with all dependencies.
* Do the next steps in the same console, as further step rely on environment
  variables set by earlier steps.
* Bootstrap Pigweed environment (`source third_party/pigweed/bootstrap.sh` or
  `third_party\pigweed\bootstrap.bat` depending on OS). This will install Pigweed toolchain
  (including python), initialize and populate python virtual environment, set up
  some environment variables.
* Install Zephyr RTOS and nRF Connect SDK python dependencies (again, to the Pigweed's python's venv!):
  ```
  pip install -r third_party/zephyr/scripts/requirements.txt
  pip install -r third_party/nrf/scripts/requirements.txt
  pip install "protobuf~=6.33.4"
  ```
  The last line is needed because nRF Connect SDK requirements downgrade `protobuf`
  to 4.x, which breaks Pigweed's protobuf code generation (`lock_test` fails with
  `cannot import name 'runtime_version' from 'google.protobuf'`). Use the version
  Pigweed declares (`pip show pigweed` lists it under "Requires").
* Start VS Code (from the same console!): `"C:\Program Files\Microsoft VS Code\Code.exe" .` (or just `code .` if you have it in PATH).
* Add the following to your VS Code `settings.json`
  (Global one, not the one in repo):
  ```json
  "nrf-connect.west.env": {
    "GNUARMEMB_TOOLCHAIN_PATH": "????/third_party/pigweed/environment/cipd/packages/arm"
  }
  ```
  (replace ???? with the path to this repository location).
* Try to build and flash something!
* When running commands from console - make sure to use `nRF Connect` one. It should
  be default one because of the `settings.json`. It's important to use it and not
  some other one as it has environment variables pointing to Zephyr installation
  we use (Nordic's one).

## Subsequent set up

* Do the next stays in the same console, as further step rely on environment
  variables set by earlier steps.
* Activate Pigweed environment (`source third_party/pigweed/activate.sh` or
  `third_party\pigweed\activate.bat` depending on OS).
* Start VS Code (from the same console!): `"C:\Program Files\Microsoft VS Code\Code.exe" .` (or just `code .` if you have it in PATH).

## After `manifest/west.yml` changes

When sdk-nrf or Pigweed revision is bumped, bring the local workspace in line:
* `west update` (from the repository root). If it complains about local changes
  in some `third_party` project, stash or drop them first.
* `manifest/west.yml` only takes the sdk-nrf projects listed in `name-allowlist`.
  If a build needs another Zephyr module (e.g. MCUboot, picolibc, littlefs),
  add its project name there. `west update` doesn't delete projects that were
  dropped from the manifest; their folders under `third_party` can be removed
  by hand.
* Re-bootstrap Pigweed (`source third_party/pigweed/bootstrap.sh` or
  `third_party\pigweed\bootstrap.bat`), as a new revision can bring a new toolchain
  and python version.
* Re-install python dependencies, same as during the first time set up (including
  the `protobuf` step).
* `west zephyr-export`.

# Flashing

## Black Magic Probe

Doesn't use OpenOCD, has an internal GDB server to connect to directly.

To use via `west flash`,

* `board.cmake` needs to have `include(${ZEPHYR_BASE}/boards/common/blackmagicprobe.board.cmake)`
* Run `west flash --runner blackmagicprobe --gdb-serial \.\\COM11` (note weird format of COM-port name when it has 2 digits)

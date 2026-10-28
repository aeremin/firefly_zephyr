"""Hardware-in-the-loop tests, run against the boards of the test rack.

Each board is reached through a Black Magic Probe, configured with an
environment variable holding the probe's /dev/serial/by-id path without the
interface suffix (e.g. ...-if00):

    FIREFLY_PROBE  firefly_v1 board
    LOCK_PROBE     lock_v01 board

Tests for boards without a probe configured are skipped, unless
--require-all-boards is given. Images are taken from --elf-dir as
<image>.elf, as produced by the CI build job.
"""

from __future__ import annotations

import os
from pathlib import Path

import pytest

from rack import Board

BOARD_PROBE_VARIABLES = {
    'firefly_v1': 'FIREFLY_PROBE',
    'lock_v01': 'LOCK_PROBE',
}


def pytest_addoption(parser: pytest.Parser) -> None:
    parser.addoption('--elf-dir', default='.', help='directory with <image>.elf files')
    parser.addoption('--gdb', default='gdb-multiarch', help='GDB executable used to flash')
    parser.addoption(
        '--require-all-boards',
        action='store_true',
        help='fail instead of skipping tests for boards without a probe configured',
    )
    parser.addoption('--ble', action='store_true', help='run Bluetooth LE tests')


def pytest_collection_modifyitems(config: pytest.Config, items: list[pytest.Item]) -> None:
    if config.getoption('--ble'):
        return
    skip = pytest.mark.skip(reason='Bluetooth LE tests are disabled (use --ble)')
    for item in items:
        if 'ble' in item.keywords:
            item.add_marker(skip)


@pytest.fixture(scope='session')
def elf_dir(request: pytest.FixtureRequest) -> Path:
    return Path(request.config.getoption('--elf-dir'))


def _board(request: pytest.FixtureRequest, name: str):
    variable = BOARD_PROBE_VARIABLES[name]
    probe = os.environ.get(variable)
    if not probe:
        message = f'{variable} is not set, no {name} board to test on'
        if request.config.getoption('--require-all-boards'):
            pytest.fail(message)
        pytest.skip(message)
    board = Board(name, probe, request.config.getoption('--gdb'))
    yield board
    board.close()


@pytest.fixture(scope='session')
def firefly_board(request: pytest.FixtureRequest):
    yield from _board(request, 'firefly_v1')


@pytest.fixture(scope='session')
def lock_board(request: pytest.FixtureRequest):
    yield from _board(request, 'lock_v01')

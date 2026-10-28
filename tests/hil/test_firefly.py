"""The firefly image on the firefly_v1 board, see firmware/firefly."""

from __future__ import annotations

import pytest

import ble

LOG_TIMEOUT_S = 5


@pytest.fixture(scope='module')
def firefly(firefly_board, elf_dir):
    return firefly_board.run(elf_dir / 'firefly.elf', r'Hello! Application started successfully\.')


@pytest.fixture(scope='module')
def address(firefly):
    return firefly.ble_address()


def test_logs_ble_identity(address):
    assert address


@pytest.mark.ble
def test_advertises_its_name(address):
    assert ble.advertised_name(address) == 'Firefly v.1'


@pytest.mark.ble
def test_advertises_again_after_a_connection(firefly, address):
    before = firefly.board.uart.mark()
    ble.connect(address)
    firefly.wait_for_next(r'Advertising successfully started', LOG_TIMEOUT_S, after=before)
    assert ble.advertised_name(address) == 'Firefly v.1'


@pytest.mark.ble
def test_battery_level_is_readable(address, record_property):
    level = ble.read(address, ble.BATTERY_LEVEL_UUID)
    # In the report: a low battery can make the board reset (e.g. when the buzzer starts).
    record_property('battery_level', list(level))
    assert len(level) == 1 and 0 <= level[0] <= 100


@pytest.mark.ble
def test_beep(firefly, address):
    before = firefly.board.uart.mark()
    ble.write(address, ble.BEEP_UUID, bytes([ble.BEEP_VOLUME]))
    firefly.wait_for_next(r'Beep!', LOG_TIMEOUT_S, after=before)


@pytest.mark.ble
def test_blink(firefly, address):
    before = firefly.board.uart.mark()
    ble.write(address, ble.BLINK_UUID, bytes([1]))
    firefly.wait_for_next(r'Blink!', LOG_TIMEOUT_S, after=before)


def test_no_fault_logged(firefly):
    # Runs last in this module, so it covers everything the tests above did.
    assert not firefly.faults()

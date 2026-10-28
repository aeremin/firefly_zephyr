"""The lock image on the lock_v01 board, see firmware/lock."""

from __future__ import annotations

import pytest

import ble

LOG_TIMEOUT_S = 5


@pytest.fixture(scope='module')
def lock(lock_board, elf_dir):
    return lock_board.run(elf_dir / 'lock.elf', r'Hello! Application started successfully\.')


@pytest.fixture(scope='module')
def address(lock):
    return lock.ble_address()


def test_logs_ble_identity(address):
    assert address


@pytest.mark.ble
def test_advertises_its_name(address):
    assert ble.advertised_name(address) == 'Lock v0.1'


@pytest.mark.ble
def test_beep(lock, address):
    before = lock.board.uart.mark()
    ble.write(address, ble.BEEP_UUID, bytes([ble.BEEP_VOLUME]))
    lock.wait_for_next(r'Beep!', LOG_TIMEOUT_S, after=before)


@pytest.mark.ble
def test_blink(lock, address):
    before = lock.board.uart.mark()
    ble.write(address, ble.BLINK_UUID, bytes([1]))
    lock.wait_for_next(r'Blink!', LOG_TIMEOUT_S, after=before)


def test_no_fault_logged(lock):
    # Runs last in this module, so it covers everything the tests above did.
    assert not lock.faults()

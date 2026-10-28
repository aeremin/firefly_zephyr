"""Synchronous helpers around bleak for the tests."""

from __future__ import annotations

import asyncio

from bleak import BleakClient, BleakScanner

BATTERY_LEVEL_UUID = '00002a19-0000-1000-8000-00805f9b34fb'

# The common Ostranna service, see firmware/common/bluetooth.h.
BEEP_UUID = '8ec87062-8865-4eca-82e0-2ea8e45e8221'
BLINK_UUID = '8ec87063-8865-4eca-82e0-2ea8e45e8221'

# Same as smoke_test's "all tests passed" beep.
BEEP_VOLUME = 100

SCAN_TIMEOUT_S = 20.0


async def _find(address: str):
    device = await BleakScanner.find_device_by_address(address, timeout=SCAN_TIMEOUT_S)
    if device is None:
        raise AssertionError(f'{address} not seen advertising within {SCAN_TIMEOUT_S} s')
    return device


def advertised_name(address: str) -> str | None:
    """Scans for the device and returns its advertised name."""

    async def run():
        return (await _find(address)).name

    return asyncio.run(run())


def connect(address: str) -> None:
    """Connects to the device and disconnects again."""

    async def run():
        async with BleakClient(await _find(address)):
            pass

    asyncio.run(run())


def read(address: str, uuid: str) -> bytes:
    async def run():
        async with BleakClient(await _find(address)) as client:
            return bytes(await client.read_gatt_char(uuid))

    return asyncio.run(run())


def write(address: str, uuid: str, data: bytes) -> None:
    async def run():
        async with BleakClient(await _find(address)) as client:
            await client.write_gatt_char(uuid, data, response=True)

    asyncio.run(run())

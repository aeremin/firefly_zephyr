"""The USB hub the probes are plugged into, see docs/INTEGRATION_TESTING.md."""

from __future__ import annotations

import pytest

from rack import BOOT_BANNER, Board

# Board fixture -> image flashed to check that the probe works after power cycling.
IMAGES = {
    'firefly_board': 'smoke_test_firefly',
    'lock_board': 'lock_test',
}


@pytest.fixture(scope='session')
def usb_hub(request: pytest.FixtureRequest) -> str:
    hub = request.config.getoption('--usb-hub')
    if not hub:
        pytest.skip('No USB hub to test (use --usb-hub)')
    return hub.lower()


def check_on_hub(board: Board, usb_hub: str) -> None:
    port = board.usb_port()
    assert port.hub_id() == usb_hub, (
        f'The {board.name} probe is on port {port.port} of hub {port.location} ({port.hub_id()})'
    )


@pytest.mark.parametrize('board_fixture', IMAGES)
def test_probe_is_on_the_hub(request, board_fixture, usb_hub):
    check_on_hub(request.getfixturevalue(board_fixture), usb_hub)


@pytest.mark.parametrize('board_fixture', IMAGES)
def test_power_cycle_probe(request, board_fixture, usb_hub, elf_dir):
    board = request.getfixturevalue(board_fixture)
    # Never switch another hub's port: on a Raspberry Pi 3 that would also cut its Ethernet.
    check_on_hub(board, usb_hub)
    board.power_cycle()
    board.run(elf_dir / f'{IMAGES[board_fixture]}.elf', BOOT_BANNER.pattern)

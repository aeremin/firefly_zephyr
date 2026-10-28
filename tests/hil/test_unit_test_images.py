"""On-target unit test images (pw_unit_test), see firmware/smoke_test and firmware/lock_test."""

from __future__ import annotations

import re

from rack import Board

# googletest-style output of PrintkEventHandler, see firmware/testing.
TEST_RESULT = re.compile(r'\[ +(OK|FAILED) +\] (\w+\.\w+)$')
ALL_TESTS_DONE = r'\[=+\] Done running all tests\.'
TIMEOUT_S = 60


def check_unit_tests(board: Board, elf, record_property) -> None:
    since = board.flash(elf)
    board.uart.wait_for(ALL_TESTS_DONE, since, TIMEOUT_S)

    results = {}
    for line in board.uart.lines(since):
        match = TEST_RESULT.search(line)
        if match:
            results[match.group(2)] = match.group(1)
    for test, result in results.items():
        record_property(test, result)

    log = '\n'.join(board.uart.lines(since))
    assert results, f'No test results in the log:\n{log}'
    failed = [test for test, result in results.items() if result == 'FAILED']
    assert not failed, f'Failed: {", ".join(failed)}. Log:\n{log}'
    assert not board.uart.faults(since), f'Fault logged:\n{log}'


def test_smoke_test_firefly(firefly_board, elf_dir, record_property):
    check_unit_tests(firefly_board, elf_dir / 'smoke_test_firefly.elf', record_property)


def test_lock_test(lock_board, elf_dir, record_property):
    check_unit_tests(lock_board, elf_dir / 'lock_test.elf', record_property)

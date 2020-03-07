# 'reset_config none separate' is required as there is no reset pin on the locket.
board_runner_args(openocd "--cmd-pre-init=reset_config none separate")
include(${ZEPHYR_BASE}/boards/common/openocd.board.cmake)


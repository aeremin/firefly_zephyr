cmake_minimum_required(VERSION 3.13.1)

# Re-direct the directory where the 'boards' directory is found from
# $ZEPHYR_BASE to this directory.
# set(BOARD_ROOT ${CMAKE_CURRENT_LIST_DIR}..)
set(BOARD_ROOT ${CMAKE_CURRENT_LIST_DIR}/..)

# This sample is only compatible with nrf52840_pca10065 because it has
# re-directed the 'boards' directory to a directory that only contains
# this board.
set(BOARD firefly_v1)

include($ENV{ZEPHYR_BASE}/cmake/app/boilerplate.cmake NO_POLICY_SCOPE)

project(activator)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(../common common)

target_sources(app PRIVATE main.cpp)
target_link_libraries(app PRIVATE
  cc1101
  color
  timer
  battery
  bluetooth
  rgb_led
)

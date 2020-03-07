cmake_minimum_required(VERSION 3.13.1)

# Re-direct the directory where the 'boards' directory is found from
# $ZEPHYR_BASE to this directory.
# set(BOARD_ROOT ${CMAKE_CURRENT_LIST_DIR}..)
set(BOARD_ROOT ${CMAKE_CURRENT_LIST_DIR}/..)

include($ENV{ZEPHYR_BASE}/cmake/app/boilerplate.cmake NO_POLICY_SCOPE)

project(smoke_test)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(../common common)

target_sources(app PRIVATE main.cpp)
target_link_libraries(app PRIVATE
  color
  timer
  #cc1101
  #rgb_led
)

cmake_minimum_required(VERSION 3.13.1)

# We create a pw::Function capturing 3 4-byte arguments.
add_definitions(-DPW_FUNCTION_INLINE_CALLABLE_SIZE=12)

# Only one board is supported
set(BOARD firefly_v1)

include(../common/basics.cmake)

project(firefly)

target_sources(app PRIVATE main.cpp)

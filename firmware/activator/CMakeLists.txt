cmake_minimum_required(VERSION 3.13.1)

# Only one board is supported
set(BOARD firefly_v1)

include(../common/basics.cmake)

project(activator)

target_sources(app PRIVATE main.cpp)

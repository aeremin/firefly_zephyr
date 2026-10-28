cmake_minimum_required(VERSION 3.20.0)

include(../common/basics.cmake)

project(power_meter)

# Self-contained: doesn't use the shared code from firmware/common.
target_sources(app PRIVATE main.cpp)

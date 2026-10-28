cmake_minimum_required(VERSION 3.13.1)

include(../common/basics.cmake)

project(smoke_test)

target_sources(app PRIVATE main.cpp)
target_link_libraries(app PRIVATE
  pw_unit_test.light
)

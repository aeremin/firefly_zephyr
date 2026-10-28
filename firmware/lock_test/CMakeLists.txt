cmake_minimum_required(VERSION 3.13.1)

include($ENV{PW_ROOT}/pw_system/backend.cmake)
pw_set_backend(pw_system.io pw_system.sys_io_target_io)

# We create a pw::Function capturing 3 4-byte arguments.
add_definitions(-DPW_FUNCTION_INLINE_CALLABLE_SIZE=12)

include(../common/basics.cmake)

project(lock_test)

include($ENV{PW_ROOT}/pw_build/pigweed.cmake)
include($ENV{PW_ROOT}/pw_protobuf_compiler/proto.cmake)

add_subdirectory(../common/rpc common_rpc EXCLUDE_FROM_ALL)

target_sources(app PRIVATE main.cpp)
target_link_libraries(app PRIVATE
  pw_system.rpc_server
  rpc.test_proto.pwpb
  rpc.test_proto.pwpb_rpc
  pw_unit_test.light
  pw_log.protos.raw_rpc
  pw_log
  pw_thread.thread
)


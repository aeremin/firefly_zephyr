if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-psabi -Wno-pointer-arith")
endif()

# Re-direct the directory where the 'boards' directory is found from
# $ZEPHYR_BASE to this directory.
# Note: this needs to happen **before** the `find_package(Zephyr)`, as
# board lookup (based on BOARD root and --board argument passed to build)
# happens inside `find_package(Zephyr)`.
list(APPEND BOARD_ROOT ${CMAKE_CURRENT_LIST_DIR}/../..)

find_package(Zephyr)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Pigweed used 16k by default which is A LOT.
add_definitions(-DPW_UNIT_TEST_CONFIG_MEMORY_POOL_SIZE=512)

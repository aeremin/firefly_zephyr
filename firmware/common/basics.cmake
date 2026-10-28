if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-psabi -Wno-pointer-arith")
endif()

# Custom boards and shared code (firmware/common, firmware/testing) come from
# this repository being a Zephyr module, see manifest/zephyr/module.yml.
find_package(Zephyr)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Pigweed used 16k by default which is A LOT.
add_definitions(-DPW_UNIT_TEST_CONFIG_MEMORY_POOL_SIZE=512)

zephyr_include_directories(.)

zephyr_library_named(firefly_testing)
# Images that don't use any of the testing helpers end up with an empty library.
zephyr_library_property(ALLOW_EMPTY TRUE)

if(CONFIG_FIREFLY_PRINTK_EVENT_HANDLER)
  zephyr_library_sources(printk_event_handler.cpp)
  zephyr_library_link_libraries(pw_unit_test.googletest_style_event_handler)
endif()

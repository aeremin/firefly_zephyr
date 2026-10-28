zephyr_include_directories(.)

zephyr_library_named(firefly_common)
# Images that don't use any of the shared code end up with an empty library.
zephyr_library_property(ALLOW_EMPTY TRUE)

zephyr_library_sources_ifdef(CONFIG_FIREFLY_BATTERY battery.cpp)
zephyr_library_sources_ifdef(CONFIG_FIREFLY_BLUETOOTH bluetooth.cpp)
zephyr_library_sources_ifdef(CONFIG_FIREFLY_BUZZER buzzer.cpp)
zephyr_library_sources_ifdef(CONFIG_FIREFLY_CC1101 cc1101.cpp)
zephyr_library_sources_ifdef(CONFIG_FIREFLY_COLOR color.cpp)
zephyr_library_sources_ifdef(CONFIG_FIREFLY_KEYBOARD keyboard.cpp)
zephyr_library_sources_ifdef(CONFIG_FIREFLY_NFC nfc.cpp nfc_definitions.cpp)
zephyr_library_sources_ifdef(CONFIG_FIREFLY_RGB_LED rgb_led.cpp)
zephyr_library_sources_ifdef(CONFIG_FIREFLY_ST25R3911B st25r3911b.cpp)
zephyr_library_sources_ifdef(CONFIG_FIREFLY_THREAD thread.cpp)
zephyr_library_sources_ifdef(CONFIG_FIREFLY_TIMER timer.cpp)

# These use PW_LOG_*. Pigweed only adds the pw_log facade to zephyr_interface,
# the backend has to be linked explicitly.
if(CONFIG_FIREFLY_KEYBOARD OR CONFIG_FIREFLY_NFC OR CONFIG_FIREFLY_ST25R3911B)
  zephyr_library_link_libraries(pw_log)
endif()

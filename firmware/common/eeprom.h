#pragma once

#include <cstdint>

#include <zephyr/device.h>
#include <zephyr/drivers/eeprom.h>
#include <zephyr/drivers/gpio.h>

namespace eeprom {
namespace internal {
const device* kEepromDevice = DEVICE_DT_GET(DT_ALIAS(eeprom));
}

// Enables eeprom power. Without that reads/writes won't work.
void EnablePower() {
  #if DT_NODE_EXISTS(DT_ALIAS(eeprom_en))
    const gpio_dt_spec gpio_spec = GPIO_DT_SPEC_GET(DT_ALIAS(eeprom_en), gpios);
    gpio_pin_configure_dt(&gpio_spec, GPIO_OUTPUT_ACTIVE);
  #endif
}

template<typename T> T Read(uint32_t offset) {
  T result;
  eeprom_read(internal::kEepromDevice, offset, &result, sizeof(result));
  return result;
}

template<typename T> void Write(const T& value, uint32_t offset) {
  eeprom_write(internal::kEepromDevice, offset, &value, sizeof(value));
}

}
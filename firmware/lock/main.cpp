#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/random/random.h>

#include "bluetooth.h"
#include "buzzer.h"
#include "eeprom.h"
#include "keyboard.h"
#include "nfc.h"
#include "pw_log/log.h"
#include "rgb_led.h"
#include "sequences.h"

constexpr gpio_dt_spec reed_switch = GPIO_DT_SPEC_GET(DT_NODELABEL(reed_switch), gpios);
constexpr gpio_dt_spec sw1 = GPIO_DT_SPEC_GET(DT_NODELABEL(button_sw1), gpios);
constexpr gpio_dt_spec sw2 = GPIO_DT_SPEC_GET(DT_NODELABEL(button_sw2), gpios);

const gpio_dt_spec gnd_gpio_device_spec = GPIO_DT_SPEC_GET(DT_NODELABEL(nfc_gnd), gpios);
const gpio_dt_spec rst_gpio_device_spec = GPIO_DT_SPEC_GET(DT_NODELABEL(nfc_rst), gpios);

RgbLed led;
RgbLedSequencer led_sequencer(led);
Buzzer buzzer;

using namespace std;

/* Beep Characteristic, UUID 8ec87062-8865-4eca-82e0-2ea8e45e8221 */
struct bt_uuid_128 beep_characteristic_uuid =
    BT_UUID_INIT_128(0x21, 0x82, 0x5e, 0xe4, 0xa8, 0x2e, 0xe0, 0x82, 0xca, 0x4e, 0x65, 0x88, 0x62, 0x70, 0xc8, 0x8e);

/* Blink Characteristic, UUID 8ec87063-8865-4eca-82e0-2ea8e45e8221 */
struct bt_uuid_128 blink_characteristic_uuid =
    BT_UUID_INIT_128(0x21, 0x82, 0x5e, 0xe4, 0xa8, 0x2e, 0xe0, 0x82, 0xca, 0x4e, 0x65, 0x88, 0x63, 0x70, 0xc8, 0x8e);

ssize_t write_beep(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf, uint16_t len,
                   uint16_t offset, uint8_t flags) {
  if (len <= 0) {
    return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
  }
  uint8_t volume = *reinterpret_cast<const uint8_t *>(buf);
  PW_LOG_INFO("Beep!");
  buzzer.Beep(volume, 600, 300);
  return len;
}

ssize_t write_blink(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf, uint16_t len,
                    uint16_t offset, uint8_t flags) {
  PW_LOG_INFO("Blink!");
  led_sequencer.StartOrRestart(lsqFastBlink);
  return len;
}

BT_GATT_SERVICE_DEFINE(firefly_service, BT_GATT_PRIMARY_SERVICE(&firefly_service_uuid),
                       BT_GATT_CHARACTERISTIC(&beep_characteristic_uuid.uuid, BT_GATT_CHRC_WRITE, BT_GATT_PERM_WRITE,
                                              nullptr, write_beep, nullptr),
                       BT_GATT_CUD("Beep", BT_GATT_PERM_READ),
                       BT_GATT_CHARACTERISTIC(&blink_characteristic_uuid.uuid, BT_GATT_CHRC_WRITE, BT_GATT_PERM_WRITE,
                                              nullptr, write_blink, nullptr),
                       BT_GATT_CUD("Blink", BT_GATT_PERM_READ), );

int main() {
  PW_LOG_INFO("Hello! Application started successfully.");
  InitBleAdvertising(ConnectableFastAdvertisingParams());

  for (auto &spec : {reed_switch, sw1, sw2}) {
    gpio_pin_configure_dt(&spec, GPIO_INPUT);
  }

  Keyboard keyboard([&](char c) {
    PW_LOG_INFO("Pressed %c", c);
    if (c == '*') {
      buzzer.Beep(200, 700, 300);
    }
  });

  led_sequencer.StartOrRestart(lsqStart);

  buzzer.Beep(100, 600, 100);

  gpio_pin_configure_dt(&gnd_gpio_device_spec, GPIO_OUTPUT_INACTIVE);
  gpio_pin_configure_dt(&rst_gpio_device_spec, GPIO_OUTPUT_ACTIVE);
  // The MFRC522 is only usable once its oscillator is running after the power-up / reset
  // (MFRC522 datasheet, section 8.8.2). Common drivers wait 50 ms.
  k_sleep(K_MSEC(50));

  Nfc nfc;
  nfc.Init();
  nfc.ReadUIDContinuously(400, [](const pw::Vector<uint8_t> &uid) {
    if (uid.size() == 4) {
      PW_LOG_INFO("4 byte UID: %02x %02x %02x %02x", uid[0], uid[1], uid[2], uid[3]);
    } else if (uid.size() == 7) {
      PW_LOG_INFO("7 byte UID: %02x %02x %02x %02x %02x %02x %02x", uid[0], uid[1], uid[2], uid[3], uid[4], uid[5],
                  uid[6]);
    } else {
      PW_LOG_WARN("Unsupported UID length: %d", uid.size());
    }
  });

  initializer_list<pair<gpio_dt_spec, const char *>> buttons{{reed_switch, "Reed"}, {sw1, "Sw1"}, {sw2, "Sw2"}};

  while (true) {
    for (auto &[spec, name] : buttons) {
      if (gpio_pin_get_dt(&spec)) {
        LOG_INF("%s pressed", name);
      }
    }
    k_sleep(K_MSEC(100));
  }
}

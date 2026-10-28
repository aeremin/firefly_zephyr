#include <array>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/uuid.h>

#include "battery.h"
#include "buzzer.h"
#include "cc1101.h"
#include "timer.h"
#include "color.h"
#include "rgb_led.h"
#include "sequences.h"
#include "bluetooth.h"
#include "magic_path_packet.h"

LOG_MODULE_DECLARE();


namespace {
Buzzer buzzer;
RgbLed led;
RgbLedSequencer led_sequencer(led);
}

struct ColorAndTimestamp {
  ColorAndTimestamp(): timestamp(0), color(0, 0, 0) {}
  int32_t timestamp;
  Color color;
};

class PacketsLog {
public:
  void ProcessRadioPacket(const MagicPathRadioPacket& p) {
    if (p.id >= colors_.size()) {
      LOG_WRN("Unexpected radio packet with ID = %d", p.id);
      return;
    }

    if (p.configure_mode) {
      background_color_ = p.background_color;
    }

    colors_[p.id].timestamp = k_uptime_get();
    colors_[p.id].color = p.color;
  }

  Color GetColor() const {
    const auto current_t = k_uptime_get();
    uint16_t r = 0, g = 0, b = 0;
    bool see_something = false;
    for (const auto& entry: colors_) {
      if (entry.timestamp != 0 && current_t - entry.timestamp < 3000) {
        r += entry.color.r;
        g += entry.color.g;
        b += entry.color.b;
        see_something = true;
      }
    }

    if (!see_something) {
      return background_color_;
    }

    return Color(std::min<uint16_t>(255, r), std::min<uint16_t>(255, g), std::min<uint16_t>(255, b));
  }

private:
  Color background_color_ = {0, 0, 0};
  std::array<ColorAndTimestamp, 20> colors_;
};

/* Beep Characteristic, UUID 8ec87062-8865-4eca-82e0-2ea8e45e8221 */
struct bt_uuid_128 beep_characteristic_uuid = BT_UUID_INIT_128(
    0x21, 0x82, 0x5e, 0xe4, 0xa8, 0x2e, 0xe0, 0x82,
    0xca, 0x4e, 0x65, 0x88, 0x62, 0x70, 0xc8, 0x8e);

/* Blink Characteristic, UUID 8ec87063-8865-4eca-82e0-2ea8e45e8221 */
struct bt_uuid_128 blink_characteristic_uuid = BT_UUID_INIT_128(
    0x21, 0x82, 0x5e, 0xe4, 0xa8, 0x2e, 0xe0, 0x82,
    0xca, 0x4e, 0x65, 0x88, 0x63, 0x70, 0xc8, 0x8e);

ssize_t write_beep(struct bt_conn *conn,
                           const struct bt_gatt_attr *attr,
                           const void *buf, uint16_t len, uint16_t offset,
                           uint8_t flags) {
  if (len <= 0) {
    return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
  }
  uint8_t volume = *reinterpret_cast<const uint8_t*>(buf);
  LOG_INF("Beep!");
  buzzer.Beep(volume, 600, 300);
  return len;
}



ssize_t write_blink(struct bt_conn *conn,
                           const struct bt_gatt_attr *attr,
                           const void *buf, uint16_t len, uint16_t offset,
                           uint8_t flags) {
  LOG_INF("Blink!");
  led.EnablePowerStabilizer();
  led_sequencer.StartOrRestart(lsqFastBlink);
  return len;
}

BT_GATT_SERVICE_DEFINE(firefly_service,
                       BT_GATT_PRIMARY_SERVICE(&firefly_service_uuid),
                       BT_GATT_CHARACTERISTIC(&beep_characteristic_uuid.uuid,
                                              BT_GATT_CHRC_WRITE,
                                              BT_GATT_PERM_WRITE,
                                              nullptr, write_beep, nullptr),
                       BT_GATT_CUD("Beep", BT_GATT_PERM_READ),
                       BT_GATT_CHARACTERISTIC(&blink_characteristic_uuid.uuid,
                                              BT_GATT_CHRC_WRITE,
                                              BT_GATT_PERM_WRITE,
                                              nullptr, write_blink, nullptr),
                       BT_GATT_CUD("Blink", BT_GATT_PERM_READ),
);


void LogResetCause() {
  uint32_t cause = 0;
  if (hwinfo_get_reset_cause(&cause) != 0) {
    return;
  }
  // The flags accumulate across resets until cleared.
  hwinfo_clear_reset_cause();
  // nRF52 sets no flag for a power-on reset or a brownout.
  LOG_WRN("Reset cause: 0x%x%s%s%s%s%s%s", cause,
          cause == 0 ? " power-on/brownout" : "",
          (cause & RESET_PIN) ? " pin" : "",
          (cause & RESET_SOFTWARE) ? " software" : "",
          (cause & RESET_WATCHDOG) ? " watchdog" : "",
          (cause & RESET_CPU_LOCKUP) ? " lockup" : "",
          (cause & RESET_DEBUG) ? " debug" : "");
}

int main(void) {
  LOG_WRN("Hello! Application started successfully.");
  LogResetCause();
  InitBleAdvertising(ConnectableSlowAdvertisingParams());

  Cc1101 cc1101;
  cc1101.Init();
  cc1101.SetChannel(1);

  led.EnablePowerStabilizer();
  PacketsLog log;

  atomic_t low_power_mode = 0;

  auto t1 = RunEvery([&log](){
    auto c = log.GetColor();
    LOG_DBG("New color is %d %d %d", c.r, c.g, c.b);
    led.SetColorSmooth(c, 1000);
  }, 1000);

  auto t2 = RunEvery([&cc1101, &low_power_mode, &t1](){
    auto v = Battery::GetInstance().GetVoltage();
    if (v == 0) return; // Workaround for the first measurement
    LOG_INF("Adc result: %d", v);
    const uint8_t level = std::clamp(v / 3 - 790, 0, 100);
    SetBatteryLevel(level);
    if (level < 10) {
      LOG_WRN("Entering low power mode");
      atomic_set(&low_power_mode, 1);
      t1.Cancel();
      led.SetColor({0, 0, 0});
      led.DisablePowerStabilizer();
      cc1101.EnterPwrDown();
    }
  }, 5000);

  led_sequencer.StartOrRestart(lsqStart);

  while (true) {
    MagicPathRadioPacket pkt;
    if (atomic_get(&low_power_mode)) k_sleep(K_FOREVER);

    for (int ch = 0; ch < 4; ++ch) {
      cc1101.SetChannel(ch);
      if (cc1101.Receive(63, &pkt)) {
        LOG_DBG("Got packet! ID=%d, R=%d, G=%d, B=%d", pkt.id, pkt.color.r, pkt.color.g, pkt.color.b);
        log.ProcessRadioPacket(pkt);
      }
    }
    k_sleep(K_MSEC(810));
  }
}

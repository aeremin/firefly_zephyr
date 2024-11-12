#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

LOG_MODULE_REGISTER(main);

constexpr gpio_dt_spec power_en = GPIO_DT_SPEC_GET(DT_NODELABEL(power_en), gpios);
constexpr gpio_dt_spec led1 = GPIO_DT_SPEC_GET(DT_NODELABEL(led1), gpios);
constexpr gpio_dt_spec button_sw1 = GPIO_DT_SPEC_GET(DT_NODELABEL(button_sw1), gpios);

bool power_enabled = false;
gpio_callback button_callback_data;

void ActuatePowerEnabled() {
  gpio_pin_set_dt(&power_en, power_enabled);
  gpio_pin_set_dt(&led1, power_enabled);
}

// Cordyceps (Rothult) service, UUID  ab12c725-0a0e-4c06-9968-2b9fff64b504
bt_uuid_128 cordyceps_service_uuid =
    BT_UUID_INIT_128(BT_UUID_128_ENCODE(0xab12c725, 0x0a0e, 0x4c06, 0x9968, 0x2b9fff64b504));

static const bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_UUID128_SOME, cordyceps_service_uuid.val, sizeof(cordyceps_service_uuid.val))};

bt_le_adv_param ConnectableFastAdvertisingParams() {
  return {
      .id = 0,
      .options = BT_LE_ADV_OPT_CONNECTABLE | BT_LE_ADV_OPT_USE_NAME,
      .interval_min = BT_GAP_ADV_FAST_INT_MIN_2,
      .interval_max = BT_GAP_ADV_FAST_INT_MAX_2,
  };
}

// Power on characteristic, UUID 0eb094be-5997-492e-a257-9242c57b1586
struct bt_uuid_128 power_on_characteristic_uuid =
    BT_UUID_INIT_128(BT_UUID_128_ENCODE(0x0eb094be, 0x5997, 0x492e, 0xa257, 0x9242c57b1586));

ssize_t ReadPowerOn(struct bt_conn* conn, const struct bt_gatt_attr* attr, void* buf, uint16_t len, uint16_t offset) {
  uint8_t as_uint8 = power_enabled ? 1 : 0;
  return bt_gatt_attr_read(conn, attr, buf, len, offset, &as_uint8, sizeof(as_uint8));
}

ssize_t WritePowerOn(struct bt_conn* conn, const struct bt_gatt_attr* attr, const void* buf, uint16_t len,
                     uint16_t offset, uint8_t flags) {
  uint8_t enabled = *reinterpret_cast<const uint8_t*>(buf);
  power_enabled = enabled != 0;
  ActuatePowerEnabled();

  return len;
}

BT_GATT_SERVICE_DEFINE(power_meter_service, BT_GATT_PRIMARY_SERVICE(&cordyceps_service_uuid),
                       BT_GATT_CHARACTERISTIC(&power_on_characteristic_uuid.uuid,
                                              BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
                                              BT_GATT_PERM_READ | BT_GATT_PERM_WRITE, ReadPowerOn, WritePowerOn,
                                              &power_enabled),
                       BT_GATT_CUD("Power On", BT_GATT_PERM_READ), );

void InitBleAdvertising(const bt_le_adv_param& params) {
  auto err = bt_enable(nullptr);
  if (err) {
    LOG_ERR("Bluetooth init failed (err %d)", err);
    return;
  }

  LOG_INF("Bluetooth initialized");

  err = bt_le_adv_start(&params, ad, ARRAY_SIZE(ad), nullptr, 0);
  if (err) {
    LOG_ERR("Advertising failed to start (err %d)", err);
    return;
  }

  LOG_INF("Advertising successfully started");
}

void OnButtonPress(const device* gpio, gpio_callback* cb, uint32_t pins) {
  power_enabled = !power_enabled;
  ActuatePowerEnabled();
}

int main() {
  InitBleAdvertising(ConnectableFastAdvertisingParams());

  gpio_pin_configure_dt(&power_en, GPIO_OUTPUT);
  gpio_pin_configure_dt(&led1, GPIO_OUTPUT);
  gpio_pin_configure_dt(&button_sw1, GPIO_INPUT);

  gpio_init_callback(&button_callback_data, OnButtonPress, BIT(button_sw1.pin));
  gpio_add_callback(button_sw1.port, &button_callback_data);
  gpio_pin_interrupt_configure_dt(&button_sw1, GPIO_INT_EDGE_TO_ACTIVE);

  ActuatePowerEnabled();

  while (true) {
    k_sleep(K_MSEC(500));
    LOG_INF("Avaiting command...");
  }

  return 0;
}

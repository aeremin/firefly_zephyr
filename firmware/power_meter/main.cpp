#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/bluetooth.h>
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

// Power meter service, UUID  a0c272ef-1280-4a64-8c5b-2833b948fc96
bt_uuid_128 power_meter_service_uuid =
    BT_UUID_INIT_128(BT_UUID_128_ENCODE(0xa0c272ef, 0x1280, 0x4a64, 0x8c5b, 0x2833b948fc96));

static const bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_UUID128_SOME, power_meter_service_uuid.val, sizeof(power_meter_service_uuid.val))};

static const bt_data sd[] = {
    BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1)};

bt_le_adv_param ConnectableFastAdvertisingParams() {
  return {
      .id = 0,
      .options = BT_LE_ADV_OPT_CONN,
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

BT_GATT_SERVICE_DEFINE(power_meter_service, BT_GATT_PRIMARY_SERVICE(&power_meter_service_uuid),
                       BT_GATT_CHARACTERISTIC(&power_on_characteristic_uuid.uuid,
                                              BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
                                              BT_GATT_PERM_READ | BT_GATT_PERM_WRITE, ReadPowerOn, WritePowerOn,
                                              &power_enabled),
                       BT_GATT_CUD("Power On", BT_GATT_PERM_READ), );

namespace ina226 {
// Datasheet: https://www.ti.com/lit/ds/symlink/ina226.pdf

constexpr i2c_dt_spec bus = {I2C_DT_SPEC_GET_ON_I2C(DT_NODELABEL(ina226))};

// According to equation 2) in chapter 7.5, it should be enough to measure
// currents up to 2^15 mA ~= 32 A, which is more than enough for our purposes.
// We could have used smaller value to get a better precision,
// but then conversion will be slightly more convoluted.
constexpr uint16_t kCurrentLsbMa = 1;
constexpr uint16_t kShuntResistanceMohm = 20;

// See equation 1) in chapter 7.1.
// 0.00512 becomes 5120 after taking both "millis" into account (from current and resistance).
constexpr uint16_t kCalibrationValue = 5120 / (kCurrentLsbMa * kShuntResistanceMohm);

// See chapter 7.6.4:
// "Power Register LSB is internally programmed to equal 25 times the programmed value of the Current_LSB"
constexpr uint16_t kPowerLsbMw = 25 * kCurrentLsbMa;

// See chapter 7.6.
enum class Register : uint8_t {
  Configuration = 0x00,
  ShuntVoltage = 0x01,
  BusVoltage = 0x02,
  Power = 0x03,
  Current = 0x04,
  Calibration = 0x05,
  MaskEnable = 0x06,
  AlertLimit = 0x07,
  ManufacturerId = 0xfe,
  DieId = 0xff,
};

struct [[gnu::packed]] [[gnu::aligned(2)]] ConfigurationRegister {
  enum class OperatingMode : uint8_t {
    PowerDown = 0b000,
    ShuntVoltageTriggered = 0b001,
    BusVoltageTriggered = 0b010,
    ShuntAndBusTriggered = 0b011,
    ShuntVoltageContinuous = 0b101,
    BusVoltageContinuous = 0b110,
    ShuntAndBusContinuous = 0b111,
  };

  enum class ConversionTime : uint8_t {
    us140 = 0b000,
    us204 = 0b001,
    us332 = 0b010,
    us588 = 0b011,
    us1100 = 0b100,
    us2116 = 0b101,
    us4156 = 0b110,
    us8244 = 0b111,
  };

  enum class AveragingMode : uint8_t {
    Samples1 = 0b000,
    Samples4 = 0b001,
    Samples16 = 0b010,
    Samples64 = 0b011,
    Samples128 = 0b100,
    Samples256 = 0b101,
    Samples512 = 0b110,
    Samples1024 = 0b111,
  };

  OperatingMode mode : 3;
  ConversionTime shunt_voltage_conversion_time : 3;
  ConversionTime bus_voltage_conversion_time : 3;
  AveragingMode averaging_mode : 3;
  uint8_t : 3;
  bool reset : 1;
};

static_assert(sizeof(ConfigurationRegister) == 2);

int ReadRegister(Register reg, int16_t* value) {
  uint8_t rx_buf[2];
  int rc = i2c_write_read_dt(&bus, &reg, sizeof(reg), rx_buf, sizeof(rx_buf));
  *value = sys_get_be16(rx_buf);

  return rc;
}

int WriteRegister(Register reg, uint16_t value) {
  uint8_t tx_buf[3] = {uint8_t(reg), 0, 0};
  sys_put_be16(value, &tx_buf[1]);

  return i2c_write_dt(&bus, tx_buf, sizeof(tx_buf));
}

void Init() {
  WriteRegister(Register::Calibration, kCalibrationValue);

  ConfigurationRegister config = {
    .mode = ConfigurationRegister::OperatingMode::ShuntAndBusContinuous,
    .shunt_voltage_conversion_time = ConfigurationRegister::ConversionTime::us8244,
    .bus_voltage_conversion_time = ConfigurationRegister::ConversionTime::us8244,
    .averaging_mode = ConfigurationRegister::AveragingMode::Samples64,
    .reset = false,
  };

  WriteRegister(Register::Configuration, *reinterpret_cast<int16_t*>(&config));
}

struct Stats {
  int16_t voltage_mv;
  int16_t current_ma;
  int16_t power_mw;
};

Stats ReadStats() {
  Stats result;

  int16_t val;
  ReadRegister(Register::BusVoltage, &val);
  // See chapter 7.6.3.
  result.voltage_mv = 1.25 * val;

  ReadRegister(Register::Current, &val);
  result.current_ma = kCurrentLsbMa * val;

  ReadRegister(Register::Power, &val);
  result.power_mw = kPowerLsbMw * val;

  return result;
}

}  // namespace ina226


void InitBleAdvertising(const bt_le_adv_param& params) {
  auto err = bt_enable(nullptr);
  if (err) {
    LOG_ERR("Bluetooth init failed (err %d)", err);
    return;
  }

  LOG_INF("Bluetooth initialized");

  err = bt_le_adv_start(&params, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
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

  ina226::Init();

  while (true) {
    k_sleep(K_MSEC(500));

    auto stats = ina226::ReadStats();
    LOG_INF("V %d mV, I %d mA, P %d mW", stats.voltage_mv, stats.current_ma, stats.power_mw);
  }

  return 0;
}

#include "cc1101.h"

LOG_MODULE_DECLARE();

device* Cc1101::spi_ = device_get_binding(DT_ALIAS_CC1101_SPI_LABEL);

device* gpio_device = device_get_binding(DT_ALIAS_CC1101_GDO0_GPIOS_CONTROLLER);
gpio_pin_t gd_pin = DT_ALIAS_CC1101_GDO0_GPIOS_PIN;

k_sem Cc1101::gd_ready_;
gpio_callback Cc1101::gdo0_callback_data_;

spi_cs_control Cc1101::spi_cs_cfg_ = {
    .gpio_dev = device_get_binding(DT_ALIAS_CC1101_SPI_CS_GPIOS_CONTROLLER),
    .gpio_pin = DT_ALIAS_CC1101_SPI_CS_GPIOS_PIN,
    .delay = 0};

spi_config Cc1101::spi_config_ = {
    .frequency = 0x400000UL, // 4 MHz
    .operation = SPI_OP_MODE_MASTER | SPI_TRANSFER_MSB | SPI_WORD_SET(8) | SPI_LINES_SINGLE,
    .slave = 0,
    .cs = &Cc1101::spi_cs_cfg_};

void Cc1101::Init() {
  k_sem_init(&gd_ready_, 0, 1);

  Reset();

  // TODO(aeremin) This is a hack. Instead we should wait for
  // CC1101 to became ready by listening for MISO to become low.
  // This actually should be done every time we initiate a transfer (i.e. pull CS low),
  // but there is no direct support for that in nRF SPI library. From the datasheet it
  // seems that it's actually only required when leaving SLEEP or XOFF states
  // (see p. 29, "4-wire Serial Configuration and Data Interface").
  k_sleep(40);

  RfConfig();
  FlushRxFIFO();

  SetTxPower(CC_PwrMinus30dBm);
  SetChannel(0);

	//auto ret = gpio_pin_configure(gpio_device, gd_pin, DT_ALIAS_CC1101_GDO0_GPIOS_FLAGS | GPIO_INPUT);
	//if (ret != 0) LOG_ERR("Failed to configure button pin: %d", ret);

	//ret = gpio_pin_interrupt_configure(gpio_device, gd_pin, GPIO_INT_EDGE_FALLING);
	//if (ret != 0) LOG_ERR("Failed to configure interrupt on pin: %d", ret);

	//gpio_init_callback(&gdo0_callback_data_, Gdo0Callback, BIT(gd_pin));
	//gpio_add_callback(gpio_device, &gdo0_callback_data_);
}

void Cc1101::Gdo0Callback(struct device *dev, struct gpio_callback *cb, u32_t pins)
{
   k_sem_give(&Cc1101::gd_ready_);
}

void Cc1101::WriteStrobe(uint8_t instruction, uint8_t* status /* = nullptr*/) {
  spi_buf tx_buf = {
      .buf = &instruction,
      .len = 1};

  spi_buf_set tx_bufs = {
      .buffers = &tx_buf,
      .count = 1};

  spi_buf rx_buf = {
      .buf = status,
      .len = (status ? 1u : 0u)};

  spi_buf_set rx_bufs = {
      .buffers = &rx_buf,
      .count = 1};

  auto r = spi_transceive(spi_, &spi_config_, &tx_bufs, &rx_bufs);
  if (r != 0) {
    LOG_ERR("WriteStrobe fail: %d", r);
  }
}

void Cc1101::WriteConfigurationRegister(uint8_t reg, uint8_t value, uint8_t* statuses /* = nullptr*/) {
  uint8_t tx[] = {reg, value};
  spi_buf tx_buf = {
      .buf = tx,
      .len = 2};

  spi_buf_set tx_bufs = {
      .buffers = &tx_buf,
      .count = 1};

  spi_buf rx_buf = {
      .buf = statuses,
      .len = (statuses ? 2u : 0u)};

  spi_buf_set rx_bufs = {
      .buffers = &rx_buf,
      .count = 1};

  auto r = spi_transceive(spi_, &spi_config_, &tx_bufs, &rx_bufs);
  if (r != 0) {
    LOG_ERR("WriteConfigurationRegister fail: %d", r);
  }
}

uint8_t Cc1101::ReadRegister(uint8_t reg, uint8_t* status /* = nullptr*/) {
  uint8_t tx = reg | CC_READ_FLAG;
  uint8_t rx[] = {0, 0};

  spi_buf tx_buf = {
      .buf = &tx,
      .len = 1};

  spi_buf_set tx_bufs = {
      .buffers = &tx_buf,
      .count = 1};

  spi_buf rx_buf = {
      .buf = rx,
      .len = 2};

  spi_buf_set rx_bufs = {
      .buffers = &rx_buf,
      .count = 1};

  auto r = spi_transceive(spi_, &spi_config_, &tx_bufs, &rx_bufs);
  if (r != 0) {
    LOG_ERR("ReadRegister fail: %d", r);
  }

  if (status) *status = rx[0];
  return rx[1];
}

void Cc1101::Recalibrate() {
  EnterIdle();
  WriteStrobe(CC_SCAL);
}

void Cc1101::RfConfig() {
  WriteConfigurationRegister(CC_FSCTRL1, CC_FSCTRL1_VALUE);   // Frequency synthesizer control.
  WriteConfigurationRegister(CC_FSCTRL0, CC_FSCTRL0_VALUE);   // Frequency synthesizer control.
  WriteConfigurationRegister(CC_FREQ2, CC_FREQ2_VALUE);       // Frequency control word, high byte.
  WriteConfigurationRegister(CC_FREQ1, CC_FREQ1_VALUE);       // Frequency control word, middle byte.
  WriteConfigurationRegister(CC_FREQ0, CC_FREQ0_VALUE);       // Frequency control word, low byte.
  WriteConfigurationRegister(CC_MDMCFG4, CC_MDMCFG4_VALUE);   // Modem configuration.
  WriteConfigurationRegister(CC_MDMCFG3, CC_MDMCFG3_VALUE);   // Modem configuration.
  WriteConfigurationRegister(CC_MDMCFG2, CC_MDMCFG2_VALUE);   // Modem configuration.
  WriteConfigurationRegister(CC_MDMCFG1, CC_MDMCFG1_VALUE);   // Modem configuration.
  WriteConfigurationRegister(CC_MDMCFG0, CC_MDMCFG0_VALUE);   // Modem configuration.
  WriteConfigurationRegister(CC_CHANNR, CC_CHANNR_VALUE);     // Channel number.
  WriteConfigurationRegister(CC_DEVIATN, CC_DEVIATN_VALUE);   // Modem deviation setting (when FSK modulation is enabled).
  WriteConfigurationRegister(CC_FREND1, CC_FREND1_VALUE);     // Front end RX configuration.
  WriteConfigurationRegister(CC_FREND0, CC_FREND0_VALUE);     // Front end RX configuration.
  WriteConfigurationRegister(CC_MCSM0, CC_MCSM0_VALUE);       // Main Radio Control State Machine configuration.
  WriteConfigurationRegister(CC_FOCCFG, CC_FOCCFG_VALUE);     // Frequency Offset Compensation Configuration.
  WriteConfigurationRegister(CC_BSCFG, CC_BSCFG_VALUE);       // Bit synchronization Configuration.
  WriteConfigurationRegister(CC_AGCCTRL2, CC_AGCCTRL2_VALUE); // AGC control.
  WriteConfigurationRegister(CC_AGCCTRL1, CC_AGCCTRL1_VALUE); // AGC control.
  WriteConfigurationRegister(CC_AGCCTRL0, CC_AGCCTRL0_VALUE); // AGC control.
  WriteConfigurationRegister(CC_FSCAL3, CC_FSCAL3_VALUE);     // Frequency synthesizer calibration.
  WriteConfigurationRegister(CC_FSCAL2, CC_FSCAL2_VALUE);     // Frequency synthesizer calibration.
  WriteConfigurationRegister(CC_FSCAL1, CC_FSCAL1_VALUE);     // Frequency synthesizer calibration.
  WriteConfigurationRegister(CC_FSCAL0, CC_FSCAL0_VALUE);     // Frequency synthesizer calibration.
  WriteConfigurationRegister(CC_TEST2, CC_TEST2_VALUE);       // Various test settings.
  WriteConfigurationRegister(CC_TEST1, CC_TEST1_VALUE);       // Various test settings.
  WriteConfigurationRegister(CC_TEST0, CC_TEST0_VALUE);       // Various test settings.
  WriteConfigurationRegister(CC_FIFOTHR, CC_FIFOTHR_VALUE);   // fifo threshold
  WriteConfigurationRegister(CC_IOCFG2, CC_IOCFG2_VALUE);     // GDO2 output pin configuration.
  WriteConfigurationRegister(CC_IOCFG0, CC_IOCFG0_VALUE);     // GDO0 output pin configuration.
  WriteConfigurationRegister(CC_PKTCTRL1, CC_PKTCTRL1_VALUE); // Packet automation control.
  WriteConfigurationRegister(CC_PKTCTRL0, CC_PKTCTRL0_VALUE); // Packet automation control.

  WriteConfigurationRegister(CC_PATABLE, CC_Pwr0dBm);

  WriteConfigurationRegister(CC_MCSM2, CC_MCSM2_VALUE);
  WriteConfigurationRegister(CC_MCSM1, CC_MCSM1_VALUE);
}
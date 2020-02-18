#include <device.h>
#include <drivers/gpio.h>
#include <drivers/spi.h>
#include <logging/log.h>
#include <zephyr.h>

#include <vector>

#include "cc1101.h"

LOG_MODULE_REGISTER();

struct MagicPathRadioPacket {
  uint8_t r, g, b;
} __attribute__((__packed__));

struct UsbHostPacket {
  uint16_t From;  // 2
  uint16_t To;    // 2
  uint8_t stuff[7];
} __attribute__ ((__packed__));

void main(void) {
  LOG_WRN("Hello! Application started successfully.");


  Cc1101 cc1101;
  cc1101.Init();
  cc1101.SetChannel(1);
  MagicPathRadioPacket pkt = {
      .r = 0,
      .g = 0,
      .b = 255};

  while (true) {
    UsbHostPacket pkt_2;
    if (cc1101.Receive(100, &pkt_2)) {
      LOG_INF("Got packet from %d", pkt_2.From);
    }
    k_sleep(100);
  }
}

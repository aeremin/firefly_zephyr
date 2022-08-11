#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <vector>

#include <vector>
#include <kernel.h>
#include <random/rand32.h>

#include "buzzer.h"
#include "cc1101.h"
#include "rgb_led.h"
#include "timer.h"
#include "eeprom.h"

#include "gtest/gtest.h"
#include "pw_unit_test/googletest_style_event_handler.h"

Timer flusher([]{ printk("                \n"); });
Buzzer buzzer;

class PrintfEventHandler final : public pw::unit_test::GoogleTestStyleEventHandler {
 public:
  constexpr PrintfEventHandler(bool verbose = false)
      : GoogleTestStyleEventHandler(verbose) {}

 private:
  void Write(const char* content) override { printk("%s", content); }

  void WriteLine(const char* format, ...) override {
    va_list args;

    va_start(args, format);
    vprintk(format, args);
    printk("\n");
    va_end(args);
  }
};


Cc1101 cc1101;

TEST(Cc1101Test, CanInit) {
  cc1101.Init();
}

TEST(Cc1101Test, CanSetPacketSize) {
  cc1101.SetPacketSize(12);
  ASSERT_EQ(cc1101.GetPacketSize(), 12);
}

TEST(Cc1101Test, CanTransmitSomething) {
  struct Data {
    uint8_t a, b;
  };
  Data d = { .a = 5, .b = 10 };

  cc1101.Transmit(d);
}

RgbLed led;

TEST(RgbLedTest, InstantColorTransition) {
  const Color c = {255, 0, 0};
  led.SetColor(c);
  ASSERT_EQ(led.GetColor(), c);
}

TEST(RgbLedTest, SmoothColorTransition) {
  led.SetColor({0, 0, 0});
  led.SetColorSmooth({254, 0, 0}, 1000);
  ASSERT_EQ(led.GetColor(), Color(0, 0, 0));
  k_sleep(K_MSEC(550));
  auto r = led.GetColor().r;
  ASSERT_GE(r, 127 - 10);
  ASSERT_LE(r, 127 + 10);
  k_sleep(K_MSEC(550));
  ASSERT_EQ(led.GetColor().r, 254);
}

TEST(TimerTest, RunsDelayed) {
  std::atomic<uint8_t> counter = 0;
  const auto t = RunDelayed([&](){ ++counter; }, 30);
  k_sleep(K_MSEC(10));
  ASSERT_EQ(counter, 0);
  k_sleep(K_MSEC(10));
  ASSERT_EQ(counter, 0);
  k_sleep(K_MSEC(10));
  ASSERT_EQ(counter, 1);
  k_sleep(K_MSEC(10));
  ASSERT_EQ(counter, 1);
}

TEST(TimerTest, RunsEvery) {
  uint8_t counter = 0;
  const auto t = RunEvery([&](){ ++counter; }, 10);
  k_sleep(K_MSEC(10));
  ASSERT_EQ(counter, 1);
  k_sleep(K_MSEC(10));
  ASSERT_EQ(counter, 2);
  k_sleep(K_MSEC(10));
  ASSERT_EQ(counter, 3);
  k_sleep(K_MSEC(10));
  ASSERT_EQ(counter, 4);
}

TEST(EepromTest, CanReadWritten) {
  eeprom::EnablePower();

  for (int i = 0; i < 200; ++i) {
    uint16_t in = sys_rand32_get() % 32768 + 23;
    uint32_t address = (2 * sys_rand32_get()) % 1024;
    eeprom::Write(in, address);
    k_sleep(K_MSEC(5));
    uint16_t out = eeprom::Read<uint16_t>(address);
    ASSERT_EQ(in, out);
  }
}

int main() {
  PrintfEventHandler handler;
  pw::unit_test::RegisterEventHandler(&handler);

  int num_failures = RUN_ALL_TESTS();
  if (!num_failures) {
    buzzer.Beep(100, 600, 100);
  } else {
    buzzer.Beep(100, 600, 1000);
  }
  // Flash the console. For some reason it doesn't happen automatically.
  // flusher.RunDelayed(100);
  while(true) k_sleep(K_MSEC(1000));
}

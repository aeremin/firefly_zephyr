#include <array>
#include <functional>
#include <memory>
#include <vector>

#include <vector>
#include <kernel.h>
#include <random/rand32.h>
#include <ztest.h>

#include "cc1101.h"
#include "rgb_led.h"
#include "timer.h"
#include "eeprom.h"

void test_two_plus_two_is_four(void) {
  zassert_equal(2 + 2, 4, "2 + 2 is not 4");
}

class Cc1101Test {
 private:
	static Cc1101 cc1101;

 public:
	static void CanInit() {
		Cc1101 cc1101;
		cc1101.Init();
	}

	static void CanSetPacketSize() {
		cc1101.SetPacketSize(12);
		zassert_equal(cc1101.GetPacketSize(), 12, "");
	}

	static void CanTransmitSomething() {
		struct Data {
			uint8_t a, b;
		};
		Data d = { .a = 5, .b = 10 };

		cc1101.Transmit(d);
	}
};

Cc1101 Cc1101Test::cc1101;

class RgbLedTest {
	static RgbLed led;
public:
	static void InstantColorTransition() {
		const Color c = {255, 0, 0};
		led.SetColor(c);
		zassert_equal(led.GetColor(), c, "");
	}

	static void SmoothColorTransition() {
		led.SetColor({0, 0, 0});
		led.SetColorSmooth({254, 0, 0}, 1000);
		zassert_equal(led.GetColor(), Color(0, 0, 0), "");
		k_sleep(K_MSEC(550));
		zassert_within(led.GetColor().r, 127, 10, "");
		k_sleep(K_MSEC(550));
		zassert_equal(led.GetColor().r, 254, "");
	}
};

RgbLed RgbLedTest::led;

class TimerTest {
 public:
	static void RunsDelayed() {
		uint8_t counter = 0;
		const auto t = RunDelayed([&](){ ++counter; }, 30);
		k_sleep(K_MSEC(10));
		zassert_equal(counter, 0, "");
		k_sleep(K_MSEC(10));
		zassert_equal(counter, 0, "");
		k_sleep(K_MSEC(10));
		zassert_equal(counter, 1, "");
		k_sleep(K_MSEC(10));
		zassert_equal(counter, 1, "");
	}

	static void RunsEvery() {
		uint8_t counter = 0;
		const auto t = RunEvery([&](){ ++counter; }, 10);
		k_sleep(K_MSEC(10));
		zassert_equal(counter, 1, "");
		k_sleep(K_MSEC(10));
		zassert_equal(counter, 2, "");
		k_sleep(K_MSEC(10));
		zassert_equal(counter, 3, "");
		k_sleep(K_MSEC(10));
		zassert_equal(counter, 4, "");
	}
};

class EepromTest {
 public:
	static void CanReadWritten() {
		eeprom::EnablePower();

		for (int i = 0; i < 200; ++i) {
			uint16_t in = sys_rand32_get() % 32768 + 23;
			uint32_t address = (2 * sys_rand32_get()) % (31 * 1024);
			eeprom::Write(in, address);
			k_sleep(K_MSEC(5));
			uint16_t out = eeprom::Read<uint16_t>(address);
			zassert_equal(in, out, "%d != %d, iteration %d", in, out, i);
		}
	}
};

Timer flusher([]{ printk("                \n"); });

void test_main(void) {
  ztest_test_suite(smoke_test,
                   ztest_unit_test(test_two_plus_two_is_four),
									 ztest_unit_test(Cc1101Test::CanInit),
									 ztest_unit_test(Cc1101Test::CanSetPacketSize),
									 ztest_unit_test(Cc1101Test::CanTransmitSomething),
									 ztest_unit_test(RgbLedTest::InstantColorTransition),
									 ztest_unit_test(RgbLedTest::SmoothColorTransition),
									 ztest_unit_test(TimerTest::RunsDelayed),
									 ztest_unit_test(TimerTest::RunsEvery),
									 ztest_unit_test(EepromTest::CanReadWritten)
  );
  ztest_run_test_suite(smoke_test);
	// Flash the console. For some reason it doesn't happen automatically.
	flusher.RunDelayed(100);
}

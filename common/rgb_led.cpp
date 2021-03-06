#include "rgb_led.h"

#include <device.h>
#include <drivers/gpio.h>

namespace {
const uint32_t kFrequencyHertz = 1000;
const uint32_t kUsecPerSecond = 1000 * 1000;
const uint32_t kCyclePeriodUs = kUsecPerSecond / kFrequencyHertz;

uint32_t colorComponentToPulseWidth(uint8_t component) {
  return (kCyclePeriodUs / 255u) * component;
}

}

RgbLed::RgbLed(): timer_([this](){ this->OnTimer(); }) {
}

void RgbLed::EnablePowerStabilizer() {
  gpio_pin_configure(device_stabilizer_,
    DT_GPIO_PIN(DT_ALIAS(led_en), gpios), GPIO_OUTPUT_ACTIVE | DT_GPIO_FLAGS(DT_ALIAS(led_en), gpios));
}

void RgbLed::DisablePowerStabilizer() {
  gpio_pin_set(device_stabilizer_, DT_GPIO_PIN(DT_ALIAS(led_en), gpios), 0);
}

void RgbLed::SetColor(const Color& color) {
  color_ = color;
  target_color_ = color;
  timer_period_ = 0;
  timer_.Cancel();
  ActuateColor();
}

const Color& RgbLed::GetColor() const {
  return color_;
}

void RgbLed::ActuateColor() {
  pwm_pin_set_usec(device_r_, DT_PWMS_CHANNEL(DT_ALIAS(led_r)),
    kCyclePeriodUs, colorComponentToPulseWidth(color_.r), DT_PWMS_FLAGS(DT_ALIAS(led_r)));
  pwm_pin_set_usec(device_g_, DT_PWMS_CHANNEL(DT_ALIAS(led_g)),
    kCyclePeriodUs, colorComponentToPulseWidth(color_.g), DT_PWMS_FLAGS(DT_ALIAS(led_g)));
  pwm_pin_set_usec(device_b_, DT_PWMS_CHANNEL(DT_ALIAS(led_b)),
    kCyclePeriodUs, colorComponentToPulseWidth(color_.b), DT_PWMS_FLAGS(DT_ALIAS(led_b)));
}

void RgbLed::SetColorSmooth(const Color& color, uint32_t delay_ms) {
  target_color_ = color;
  timer_period_ = color_.DelayToTheNextAdjustment(target_color_, delay_ms);
  timer_.RunDelayed(timer_period_);
}

void RgbLed::OnTimer() {
  color_.Adjust(target_color_);
  ActuateColor();
  if (color_ != target_color_) timer_.RunDelayed(timer_period_);
}


RgbLedSequencer::RgbLedSequencer(RgbLed& led): led_(led), timer_([this](){ this->EndChunk(); }) {
}

void RgbLedSequencer::StartOrRestart(const LedChunk* sequence) {
  timer_.Cancel();
  current_ = sequence;
  StartChunk();
}

void RgbLedSequencer::StartChunk() {
  if (current_->type == LedChunkType::Finish) return;
  if (current_->type == LedChunkType::SetColor) {
    led_.SetColorSmooth(current_->color, current_->time_ms);
  }
  timer_.RunDelayed(current_->time_ms);
}

void RgbLedSequencer::EndChunk() {
  ++current_;
  StartChunk();
}

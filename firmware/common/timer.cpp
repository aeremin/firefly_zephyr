#include "timer.h"

namespace {
void GenericCallback(k_timer *timer) {
  auto* f = static_cast<pw::Function<void()>*>(k_timer_user_data_get(timer));
  (*f)();
}
}

Timer::Timer(pw::Function<void()> action): action_(std::move(action)) {
  k_timer_init(&timer_, GenericCallback, nullptr);
  k_timer_user_data_set(&timer_, const_cast<void*>(static_cast<const void*>(&action_)));
}

Timer::Timer(Timer&& other) {
  std::swap(timer_, other.timer_);
  std::swap(action_, other.action_);
}

const Timer& Timer::operator=(Timer&& other) {
  std::swap(timer_, other.timer_);
  std::swap(action_, other.action_);
  return *this;
}

Timer::~Timer() {
  Cancel();
}

void Timer::RunDelayed(uint32_t delay_ms) {
  Cancel();
  k_timer_start(&timer_, K_MSEC(delay_ms), K_FOREVER);
}


void Timer::RunEvery(uint32_t period_ms) {
  Cancel();
  k_timer_start(&timer_, K_MSEC(period_ms), K_MSEC(period_ms));
}

void Timer::Cancel() {
  k_timer_stop(&timer_);
}

Timer RunDelayed(pw::Function<void()> action, uint32_t delay_ms) {
  Timer t(std::move(action));
  t.RunDelayed(delay_ms);
  return t;
}

Timer RunEvery(pw::Function<void()> action, uint32_t period_ms) {
  Timer t(std::move(action));
  t.RunEvery(period_ms);
  return t;
}
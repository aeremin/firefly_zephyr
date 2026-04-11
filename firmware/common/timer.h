#pragma once
#include <functional>

#include <zephyr/kernel.h>

#include "pw_function/function.h"

class Timer {
 public:
  explicit Timer(pw::Function<void()> action);
  Timer(const Timer& other) = delete;
  Timer(Timer&& other);
  const Timer& operator=(Timer&& other);
  ~Timer();

  void RunDelayed(uint32_t delay_ms);
  void RunEvery(uint32_t period_ms);

  void Cancel();
 private:
  static void WorkHandler(k_work* work);
  k_timer timer_;
  k_work work_;
  pw::Function<void()> action_;
};

Timer RunDelayed(pw::Function<void()> action, uint32_t delay_ms);
Timer RunEvery(pw::Function<void()> action, uint32_t delay_ms);
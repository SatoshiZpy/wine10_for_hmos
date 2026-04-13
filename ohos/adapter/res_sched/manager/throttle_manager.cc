// Copyright (c) 2025 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/res_sched/manager/throttle_manager.h"
namespace ohos::adapter::res_sched::manager {

namespace {
constexpr int kMaxSpinLimit = 20;

/**
 * update an atomic variable to a larget value
 * @param current the atomic variable
 * @param next a larget value
 * @return `true` if `next` is larger than stored value and update succeed
 */
bool TryChangeToLarger(std::atomic<TimeMicro64>& current, TimeMicro64 next) {
  for (int i = 0; i < kMaxSpinLimit; i++) {
    TimeMicro64 prev_value = current.load();
    if (prev_value > next) {
      return false;
    }
    if (current.compare_exchange_weak(prev_value, next)) {
      return true;
    }
  }
  return false;
}
}  // namespace
ThrottleManager::ThrottleManager(TimeMicro64 throttle_delta)
    : throttle_delta(throttle_delta) {}

bool ThrottleManager::AttemptEnter(TimeMicro64 current_time) {
  // if current is not larger, return
  TimeMicro64 prev = next_time_.load();
  if (current_time < prev) {
    return false;
  }
  TimeMicro64 new_next_time = current_time + throttle_delta;
  return TryChangeToLarger(next_time_, new_next_time);
}

DebounceManager::DebounceManager(TimeMicro64 debounce_delta)
    : debounce_delta_(debounce_delta) {}

[[nodiscard]] bool DebounceManager::AttemptRegisterOrExtend(
    TimeMicro64 current_time) {
  TimeMicro64 next_time = current_time + debounce_delta_;
  TryChangeToLarger(end_time_, next_time);
  return TryChangeToLarger(schedule_time_, next_time);
}

[[nodiscard]] DebounceManager::CallbackState DebounceManager::CheckCallbackState(TimeMicro64 register_time) {
  if (schedule_time_.load() > register_time) {
    return DebounceManager::CallbackState::kIgnore;
  }
  TimeMicro64 end_value = end_time_.load();
  if (end_value > register_time) {
    if (TryChangeToLarger(schedule_time_, end_value)) {
      return DebounceManager::CallbackState::kNeedNewRegister;
    } else {
      return DebounceManager::CallbackState::kIgnore;
    }
  }
  return DebounceManager::CallbackState::kNeedExecute;
}

}  // namespace ohos::adapter::res_sched::manager

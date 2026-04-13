// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/power_monitor/power_monitor.h"

#include "ohos/adapter/aki_hook/aki_hook.h"

namespace ohos::adapter {
void PowerMonitor::SetMonitor(OhosPowerMonitor* monitor) {
  if (monitor_ == nullptr) {
    monitor_ = monitor;
  }
}
void PowerMonitor::ReleaseMonitor() {
  if (monitor_ != nullptr) {
    monitor_ = nullptr;
  }
}

void PowerMonitor::OnSuspend() {
  if (monitor_ != nullptr) {
    monitor_->OnSuspend();
  }
}
void PowerMonitor::OnResume() {
  if (monitor_ != nullptr) {
    monitor_->OnResume();
  }
}

void PowerMonitor::OnPowerStateChanged() {
  if (monitor_ != nullptr) {
    monitor_->OnPowerStateChanged();
  }
}

bool PowerMonitor::IsOnBatteryPower() {
  if (auto func = ohos::adapter::GetJSFunction("PowerMonitor.IsOnBatteryPower")) {
    return func->Invoke<bool>();
  }
  return false;
}

JSBIND_CLASS(PowerMonitor) {
  JSBIND_METHOD(OnSuspend);
  JSBIND_METHOD(OnResume);
  JSBIND_METHOD(OnPowerStateChanged);
}
}  // namespace ohos::adapter

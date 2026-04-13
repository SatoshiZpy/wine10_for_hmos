// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/screenlock_monitor/screenlock_monitor_adapter.h"

#include "ohos/adapter/aki_hook/aki_hook.h"

namespace ohos::adapter {

ScreenlockMonitorAdapter& ScreenlockMonitorAdapter::GetInstance() {
  static ScreenlockMonitorAdapter instance;
  return instance;
}

void ScreenlockMonitorAdapter::StartListeningForScreenlock(
    ScreenlockEventCallback screenlock_event_callback) {
  auto start_listening_func = ohos::adapter::GetJSFunction(
      "ScreenlockMonitorAdapter.StartListeningForScreenlock");
  if (start_listening_func) {
    std::function<void(const std::string&)> callback =
      [=](const std::string& event_type) {
        screenlock_event_callback(event_type);
      };
    start_listening_func->Invoke<void>(callback);
  }
}

void ScreenlockMonitorAdapter::StopListeningForScreenlock() {
  auto stop_listening_func = ohos::adapter::GetJSFunction(
      "ScreenlockMonitorAdapter.StopListeningForScreenlock");
  if (stop_listening_func) {
    stop_listening_func->Invoke<void>();
  }
}

}  // namespace ohos::adapter

// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/screenlock_monitor/screenlock_monitor_adapter.h"

#include "ohos/adapter/aki_hook/aki_hook.h"
#include "ohos/adapter/common/logging.h"

namespace ohos::adapter {

ScreenlockMonitorAdapter& ScreenlockMonitorAdapter::GetInstance() {
  static ScreenlockMonitorAdapter instance;
  return instance;
}

void ScreenlockMonitorAdapter::StartListeningForScreenlock(screenlock_event_callback callback) {
  screenlock_event_callback_ = callback;

  auto start_listening_func = ohos::adapter::GetJSFunction(
      "ScreenlockMonitorAdapter.StartListeningForScreenlock");
  if (start_listening_func == nullptr) {
    LOGE("start_listening_func is nullptr");
    return;
  }

  std::function<void(const std::string&)> event_callback =
    [=](const std::string& event_type) {
      if (screenlock_event_callback_) {
        screenlock_event_callback_(event_type.c_str());
      }
    };
  start_listening_func->Invoke<void>(event_callback);
}

void ScreenlockMonitorAdapter::StopListeningForScreenlock() {
  auto stop_listening_func = ohos::adapter::GetJSFunction(
      "ScreenlockMonitorAdapter.StopListeningForScreenlock");
  if (stop_listening_func) {
    stop_listening_func->Invoke<void>();
  }
}

}  // namespace ohos::adapter

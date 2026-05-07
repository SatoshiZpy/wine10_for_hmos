// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_SCREENLOCK_MONITOR_ADAPTER_H_
#define OHOS_ADAPTER_SCREENLOCK_MONITOR_ADAPTER_H_

#include <string>

#include "ohos/adapter/aki_hook/aki_hook.h"
#include "ohos/adapter/export.h"
#include "ohos/adapter/include/ohos_adapter_common.h"

namespace ohos::adapter {

class ADAPTER_EXPORT_API ScreenlockMonitorAdapter {
 public:
  static ScreenlockMonitorAdapter& GetInstance();

  void StartListeningForScreenlock(screenlock_event_callback callback);
  void StopListeningForScreenlock();

 private:
  ScreenlockMonitorAdapter() = default;
  ~ScreenlockMonitorAdapter() = default;
  ScreenlockMonitorAdapter(const ScreenlockMonitorAdapter&) = delete;
  ScreenlockMonitorAdapter& operator=(const ScreenlockMonitorAdapter&) = delete;
  
  screenlock_event_callback screenlock_event_callback_ = nullptr;
};

}  // namespace ohos::adapter

#endif  // OHOS_ADAPTER_SCREENLOCK_MONITOR_ADAPTER_H_

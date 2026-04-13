// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/export.h"

namespace ohos::adapter {
class ADAPTER_EXPORT_API OhosPowerMonitor {
 public:
  virtual ~OhosPowerMonitor() = default;
  virtual void OnSuspend() = 0;
  virtual void OnResume() = 0;
  virtual void OnPowerStateChanged() = 0;
};

class ADAPTER_EXPORT_API PowerMonitor {
 public:
  static void OnSuspend();
  static void OnResume();
  static void OnPowerStateChanged();
  static bool IsOnBatteryPower();
  static void SetMonitor(OhosPowerMonitor* monitor);
  static void ReleaseMonitor();

 private:
  inline static OhosPowerMonitor* monitor_ = nullptr;
};
}  // namespace ohos::adapter

// Copyright (c) 2025 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/context/context_adapter.h"

#include "ohos/adapter/aki_hook/aki_hook.h"
#include "ohos/adapter/common/logging.h"
#include "ohos/adapter/device_info/device_info.h"

namespace ohos::adapter {

ContextAdapter::ContextAdapter() {}
ContextAdapter& ContextAdapter::GetInstance() {
  static ContextAdapter adapter;
  return adapter;
}

void UpdateWindowDeviceModeSwitchCB(DeviceMode device_mode) {
  LOGI(
      "UpdateWindowDeviceModeSwitchCB device_mode:%{public}d",
      device_mode);
  ContextAdapter::GetInstance().SetDeviceMode(
      device_mode);
}

JSBIND_GLOBAL() {
  JSBIND_FUNCTION(UpdateWindowDeviceModeSwitchCB);
}
}  // namespace ohos::adapter::ContextPathAdapter

// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/default_application/default_application_adapter.h"
#include "ohos/adapter/aki_hook/aki_hook.h"
#include "ohos/adapter/common/logging.h"

namespace ohos::adapter {

bool DefaultApplicationAdapter::IsDefaultApplication() {
  auto func = ohos::adapter::GetJSFunction(
      "DefaultApplicationAdapter.isDefaultApplication");
  if (func) {
    return func->Invoke<bool>();
  }
  LOGE("[DefaultApplicationAdapter] isDefaultApplication function not found");
  return false;
}

bool DefaultApplicationAdapter::StartSettingsAbility() {
  auto func = ohos::adapter::GetJSFunction(
      "DefaultApplicationAdapter.StartSettingsAbility");
  if (func) {
    return func->Invoke<bool>();
  }
  LOGE("[DefaultApplicationAdapter] StartSettingsAbility function not found");
  return false;
}

}  // namespace ohos::adapter

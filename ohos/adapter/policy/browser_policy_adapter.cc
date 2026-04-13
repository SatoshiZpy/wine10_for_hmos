// Copyright (c) 2025 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/policy/browser_policy_adapter.h"

#include "ohos/adapter/aki_hook/aki_hook.h"

namespace ohos::adapter {

std::string BrowserPolicyAdapter::getManagedBrowserPolicy() {
  if (auto func = ohos::adapter::GetJSFunction(
          "BrowserPolicyAdapter.GetManagedBrowserPolicy")) {
    return func->Invoke<std::string>();
  }
  return "";
}

}  // namespace ohos::adapter

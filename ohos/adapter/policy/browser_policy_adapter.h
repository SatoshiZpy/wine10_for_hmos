// Copyright (c) 2025 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BROWSER_POLICY_ADAPTER_H_
#define BROWSER_POLICY_ADAPTER_H_

#include <string>

#include "ohos/adapter/export.h"

namespace ohos::adapter {

class ADAPTER_EXPORT_API BrowserPolicyAdapter {
 public:
  BrowserPolicyAdapter() = default;
  ~BrowserPolicyAdapter() = default;
  static std::string getManagedBrowserPolicy();
};

}  // namespace ohos::adapter

#endif  // BROWSER_POLICY_ADAPTER_H_

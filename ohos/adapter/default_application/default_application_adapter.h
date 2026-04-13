// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_DEFAULT_APPLICATION_ADAPTER_H_
#define OHOS_ADAPTER_DEFAULT_APPLICATION_ADAPTER_H_

#include <string>

#include "ohos/adapter/export.h"

namespace ohos::adapter {

class ADAPTER_EXPORT_API DefaultApplicationAdapter {
 public:
  static bool IsDefaultApplication();
  static bool StartSettingsAbility();
};

}  // namespace ohos

#endif  // OHOS_ADAPTER_DEFAULT_APPLICATION_ADAPTER_H_

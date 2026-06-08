// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_NET_SYSTEM_SETTINGS_ADAPTER_H_
#define OHOS_ADAPTER_NET_SYSTEM_SETTINGS_ADAPTER_H_

#include <string>
#include "ohos/adapter/aki_hook/aki_hook.h"
#include "ohos/adapter/export.h"

namespace ohos::adapter::net {

class ADAPTER_EXPORT_API SystemSettingsAdapter {
 public:
  static SystemSettingsAdapter& GetInstance();

  SystemSettingsAdapter(const SystemSettingsAdapter&) = delete;
  SystemSettingsAdapter& operator=(const SystemSettingsAdapter&) = delete;
  ~SystemSettingsAdapter() = default;

  void ShowSystemSettings(const std::string& uri,
                          const std::string& sub_uri = std::string{});
 private:
  SystemSettingsAdapter() = default;
};

}  // namespace ohos::adapter::net

#endif  // OHOS_ADAPTER_NET_SYSTEM_SETTINGS_ADAPTER_H_

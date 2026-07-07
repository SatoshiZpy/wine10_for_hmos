// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/net/system_settings_adapter.h"

namespace ohos::adapter::net {

SystemSettingsAdapter& SystemSettingsAdapter::GetInstance() {
  static SystemSettingsAdapter instance;
  return instance;
}

void SystemSettingsAdapter::ShowSystemSettings(const std::string& uri,
                                               const std::string& sub_uri) {
  if (auto func =
      ohos::adapter::GetJSFunction("NetConnection.ShowSystemSettings")) {
    if (sub_uri.empty()) {
      func->Invoke<void>(uri);
    } else {
      func->Invoke<void>(uri, sub_uri);
    }
  }
}

}  // namespace ohos::adapter::net

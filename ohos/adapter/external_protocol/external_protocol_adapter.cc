// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/external_protocol/external_protocol_adapter.h"

#include "ohos/adapter/aki_hook/aki_hook.h"

namespace ohos::adapter {

ExternalProtocolAdapter& ExternalProtocolAdapter::GetInstance() {
  static ExternalProtocolAdapter instance;
  return instance;
}

void ExternalProtocolAdapter::OpenExternal(const std::string& url_str) {
  auto func =
    ohos::adapter::GetJSFunction("ExternalProtocolAdapter.OpenExternal");
  if (func) {
    func->Invoke<void>(url_str);
  }
}

}  // namespace ohos

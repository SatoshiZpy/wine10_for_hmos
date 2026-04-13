// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/device_user_auth/device_user_auth.h"

#include "ohos/adapter/aki_hook/aki_hook.h"
#include "ohos/adapter/common/logging.h"

namespace ohos::adapter::device_user_auth {

// static
DeviceUserAuthAdapter& DeviceUserAuthAdapter::GetInstance() {
  static DeviceUserAuthAdapter instance;
  return instance;
}

DeviceUserAuthAdapter::BiometricCheckResult
DeviceUserAuthAdapter::CheckBiometricAvailable() {
  std::promise<BiometricCheckResult> promise;
  std::future<BiometricCheckResult> future = promise.get_future();
  std::function<void(bool, int)> callback =
      [&promise](bool can_biom, int biom_type) {
        promise.set_value({can_biom, biom_type});
      };
  auto func = ohos::adapter::GetJSFunction(
      "DeviceUserAuthAdapter.CheckBiometricAvailable");
  if (func) {
    func->Invoke<void>(callback);
    return future.get();
  }
  LOGE("[DeviceUserAuthAdapter] CheckBiometricAvailable function not found");
  return {false, -1};
}

bool DeviceUserAuthAdapter::StartUserAuth(const std::string& prompt_string,
                                          bool enable_biom,
                                          int biom_type) {
  std::promise<bool> promise;
  std::function<void(bool)> callback = [&promise](bool ret_value) -> void {
    promise.set_value(ret_value);
  };
  auto func =
      ohos::adapter::GetJSFunction("DeviceUserAuthAdapter.StartUserAuth");
  if (func) {
    func->Invoke<void>(prompt_string, callback, enable_biom, biom_type);
    bool result = promise.get_future().get();
    return result;
  }
  LOGE("[DeviceUserAuthAdapter] StartUserAuth function not found");
  return false;
}

}  // namespace ohos::adapter::device_user_auth

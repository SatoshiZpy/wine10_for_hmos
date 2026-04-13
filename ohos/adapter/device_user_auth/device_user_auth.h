// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_DEVICE_USER_AUTH_DEVICE_USER_AUTH_ADAPTER_H_
#define OHOS_ADAPTER_DEVICE_USER_AUTH_DEVICE_USER_AUTH_ADAPTER_H_

#include <string>
#include <napi/native_api.h>
#include "ohos/adapter/export.h"

namespace ohos::adapter::device_user_auth {

class ADAPTER_EXPORT_API DeviceUserAuthAdapter {
 public:
  struct BiometricCheckResult {
    bool can_biom;
    int biom_type;
  };

  static DeviceUserAuthAdapter& GetInstance();

  BiometricCheckResult CheckBiometricAvailable();

  bool StartUserAuth(const std::string& prompt_string, bool enable_biom, int biom_type);

  int GetBiomType() const {
      return biom_type_;
  }

  void SetBiomType(int biom_type) {
      biom_type_ = biom_type;
  }

 private:
  DeviceUserAuthAdapter() = default;
  ~DeviceUserAuthAdapter() = default;
  DeviceUserAuthAdapter(const DeviceUserAuthAdapter&) = delete;
  DeviceUserAuthAdapter& operator=(const DeviceUserAuthAdapter&) = delete;
  int biom_type_;
};
}  // namespace ohos::adapter::device_user_auth

#endif  // OHOS_ADAPTER_DEVICE_USER_AUTH_DEVICE_USER_AUTH_ADAPTER_H_

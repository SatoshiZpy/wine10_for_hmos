// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/export.h"

#include <string>

namespace ohos::adapter::device_info {

const std::string kManufacture = "manufacture";
const std::string kProductModel = "productModel";
const std::string kDistributionOSName = "distributionOSName";
const std::string kDistributionOSVersion = "distributionOSVersion";
const std::string kDisplayVersion = "displayVersion";
const std::string kOsReleaseType = "osReleaseType";

enum class DeviceType {
  _2IN1,
  _TABLET
};

enum SdkVersion {
  SDK_VERSION_12 = 12,
  SDK_VERSION_13 = 13,
  SDK_VERSION_14 = 14,
  SDK_VERSION_15 = 15,
};

class ADAPTER_EXPORT_API DeviceInfo {
 public:
  DeviceInfo(const DeviceInfo&) = delete;
  DeviceInfo& operator=(const DeviceInfo&) = delete;
  static DeviceInfo& GetInstance();
  std::string Get(const std::string& property);
  DeviceType GetDeviceType();
  static int SdkApi();

 private:
  DeviceInfo();
  ~DeviceInfo() = default;
  static DeviceType device_type_;
  static int sdk_api_;
};
}  // namespace ohos::adapter::device_info

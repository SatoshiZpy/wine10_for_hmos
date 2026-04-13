// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
 
#include "ohos/adapter/device_info/device_info.h"
#include "ohos/adapter/common/logging.h"

#include <deviceinfo.h>
#include <map>
#include <string>
 
using stringFunction = std::function<std::string()>;
 
namespace ohos::adapter::device_info {

const int kSdkApiDefaultValue = -1;
int DeviceInfo::sdk_api_ = kSdkApiDefaultValue;

namespace {
std::map<std::string, std::string> DEFAULT_VALUE = {
    {kManufacture, "Unknown"},
    {kProductModel, "Unknown"},
    {kDistributionOSName, "OHOS"},
    {kDistributionOSVersion, "1.0.0.0"},
    {kDisplayVersion, "Unknown"},
    {kOsReleaseType, "Unknown"}
  };
 
std::map<std::string, stringFunction> DEVICE_INFO = {
    {kManufacture, []() {return OH_GetManufacture();}},
    {kProductModel, []() {return OH_GetProductModel();}},
    {kDistributionOSName, []() {return OH_GetDistributionOSName();}},
    {kDistributionOSVersion, []() {return OH_GetDistributionOSVersion();}},
    {kDisplayVersion,  []() {return OH_GetDisplayVersion();}},
    {kOsReleaseType,  []() {return OH_GetOsReleaseType();}}
  };

std::map<std::string, DeviceType> DEVICE_TYPE = {
    {"2in1", DeviceType::_2IN1},
    {"tablet", DeviceType::_TABLET}
  };
}

DeviceInfo& DeviceInfo::GetInstance() {
  static DeviceInfo device_info_;
  return device_info_;
}

// set default device type to 2in1
DeviceType DeviceInfo::device_type_ = DeviceType::_2IN1;
DeviceInfo::DeviceInfo() {
  std::string deviceType = OH_GetDeviceType();
  auto it = DEVICE_TYPE.find(deviceType);
  if (it != DEVICE_TYPE.end()) {
    DeviceInfo::device_type_ = it->second;
  } else {
    LOGE("Got invalid device type: %{public}s", deviceType.c_str());
  }
}
 
std::string DeviceInfo::Get(const std::string& property) {
  std::string deviceInfo = "";
  auto it = DEVICE_INFO.find(property);
  if (it != DEVICE_INFO.end()) {
    deviceInfo = it->second();
  }
  return deviceInfo == "" ? DEFAULT_VALUE.at(property) : deviceInfo;
}

DeviceType DeviceInfo::GetDeviceType() {
  return DeviceInfo::device_type_;
}

int DeviceInfo::SdkApi() {
  if (DeviceInfo::sdk_api_ == -1) {
    DeviceInfo::sdk_api_ = OH_GetSdkApiVersion();
  }
  return DeviceInfo::sdk_api_;
}
}  // namespace ohos::adapter::device_info
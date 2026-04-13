// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_PERMISSION_MANAGER_PERMISSION_MANAGER_ADAPTER_H
#define OHOS_ADAPTER_PERMISSION_MANAGER_PERMISSION_MANAGER_ADAPTER_H

#include <string>
#include "ohos/adapter/export.h"

namespace ohos::adapter {
namespace permission {

enum class OHOSPermissionType : int32_t {
  NO,
  LOCATION,
  CAMERA,
  MICROPHONE,
  DESKTOP_CAPTURE,
  BLUETOOTH,
  PASTEBOARD,
  SCREENSHOT
};


enum class PermissionActivationResult : int32_t {
  SUCCESS = 1,
  JS_BINDING_ERROR = 0,
  EMPTY_ARRAY = -1,
  PERMISSION_VERIFICATION_FAILED = 201,
  PARAMETER_ERROR = 401,
  CAPABILITY_NOT_SUPPORTED = 801,
  OPERATION_NOT_PERMITTED = 13900001,
  UNKNOWN_ERROR = 13900042
};

//The request is rejected by the system. The return value is - 1.
const int kRequestPermissionDenied = -1;
const int kRequestPermissionFail = -2;
const int kOpenPermissionConfirmFail = -1;

class ADAPTER_EXPORT_API PermissionManagerAdapter {
 public:
  static constexpr const char* kOhosPermissionLocation =
    "ohos.permission.LOCATION";
  static constexpr const char* kOhosPermissionApproximatelyLocation =
    "ohos.permission.APPROXIMATELY_LOCATION";
  static constexpr const char* kOhosPermissionMicrophone =
    "ohos.permission.MICROPHONE";
  static constexpr const char* kOhosPermissionCamera =
    "ohos.permission.CAMERA";
  static constexpr const char* kOhosPermissionPastboard =
    "ohos.permission.READ_PASTEBOARD";
  static constexpr const char* kOhosPermissionBluetooth =
    "ohos.permission.ACCESS_BLUETOOTH";
  static bool RequestPermission(const OHOSPermissionType& type);
  static bool CheckPermission(const OHOSPermissionType& type);
  static void FileAccessPersist(const std::string &url);
  static PermissionActivationResult ActivateFileAccessPersist(const std::string &url);
  static int OpenPermissionConfirm(const OHOSPermissionType& type);
  static int RequestPermissionCode(const OHOSPermissionType& type);
};

}  // namespace permission
}  // namespace ohos::adapter

#endif

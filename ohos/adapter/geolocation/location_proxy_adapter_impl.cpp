// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "location_proxy_adapter_impl.h"

#include "ohos/adapter/aki_hook/aki_hook.h"
#include "location_adapter.h"
#include "ohos/adapter/common/logging.h"
#include "ohos/adapter/permission_manager/permission_manager_adapter.h"

namespace ohos::adapter {
namespace {
bool RequestPermission() {
  return permission::PermissionManagerAdapter::RequestPermission(
    permission::OHOSPermissionType::LOCATION);
}
}  // namespace

bool LocationProxyAdapter::StartListening(
    std::shared_ptr<LocationCallbackAdapter> location_callback,
    bool high_accuracy) {
  if (!location_callback) {
    LOGE("[LocationProxyAdapter] location_callback is null");
    return false;
  }
  if (!RequestPermission()) {
    LOGE("[LocationProxyAdapter] requestPermission denied");
    return false;
  }
  auto start_listening =
      ohos::adapter::GetJSFunction("GeolocationAdapter.StartListening");
  if (!start_listening) {
    LOGE("[LocationProxyAdapter] GetJSFunction failed");
    return false;
  }
  std::function<void(aki::Value)> on_location_report =
      [=](aki::Value location) {
        std::unique_ptr<LocationInfo> location_info =
            std::make_unique<LocationInfo>(location);
        location_callback->OnLocationReport(location_info);
      };
  std::function<void(int)> on_error_report = [=](int errcode) {
    location_callback->OnErrorReport(errcode);
  };
  start_listening->Invoke<void>(on_location_report, on_error_report,
                                high_accuracy);
  return true;
}

bool LocationProxyAdapter::StopListening(
    std::shared_ptr<LocationCallbackAdapter> callback) {
  if (auto stop_listening =
          ohos::adapter::GetJSFunction("GeolocationAdapter.StopListening")) {
    stop_listening->Invoke<void>();
  }
  return true;
}
}  // namespace ohos::adapter

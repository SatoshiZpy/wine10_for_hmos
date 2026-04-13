// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "screenshot_adapter.h"

#include "ohos/adapter/aki_hook/aki_hook.h"
#include "ohos/adapter/common/logging.h"
#include "ohos/adapter/permission_manager/permission_manager_adapter.h"

namespace ohos::adapter {

bool RequestScreenshotPermission() {
  return permission::PermissionManagerAdapter::RequestPermission(
      permission::OHOSPermissionType::SCREENSHOT);
}

ScreenshotAdapter& ScreenshotAdapter::GetInstance() {
  static ScreenshotAdapter instance;
  return instance;
}

std::unique_ptr<DesktopFrame> ScreenshotAdapter::StartCapture() {
  if (!RequestScreenshotPermission()) {
    LOGE("[ScreenshotAdapter] RequestScreenshotPermission denied");
    return std::make_unique<DesktopFrame>();
  }

  auto start_capture_func = ohos::adapter::GetJSFunction(
      "ScreenshotAdapter.StartCapture");
  if (start_capture_func) {
    std::promise<std::unique_ptr<DesktopFrame>> insert_promise;
    std::function<void(aki::Value, aki::ArrayBuffer)> callback =
        [&](aki::Value image_info, aki::ArrayBuffer image_data) {
      if (image_info.IsNull()) {
        insert_promise.set_value(std::make_unique<DesktopFrame>());
        return;
      }

      std::unique_ptr<DesktopFrame> frame = std::make_unique<DesktopFrame>();
      frame->size_.width_ = image_info["size"]["width"].As<int>();
      frame->size_.height_ = image_info["size"]["height"].As<int>();
      frame->stride_ = image_info["stride"].As<int>();
      frame->data_ = image_data.GetData();

      insert_promise.set_value(std::move(frame));
    };

    start_capture_func->Invoke<void>(callback);

    return insert_promise.get_future().get();
  }
  return std::make_unique<DesktopFrame>();
}

}  // namespace ohos::adapter

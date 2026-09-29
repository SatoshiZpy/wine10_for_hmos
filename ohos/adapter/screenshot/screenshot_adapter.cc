// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "screenshot_adapter.h"

#include <cstdlib>
#include <cstring>

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

DesktopFrame::~DesktopFrame() {
  free(data_);
  data_ = nullptr;
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

      auto frame = std::make_unique<DesktopFrame>();
      int width = image_info["size"]["width"].As<int>();
      int height = image_info["size"]["height"].As<int>();
      int stride = image_info["stride"].As<int>();
      if (stride <= 0) stride = width * 4;

      const uint8_t* src = static_cast<const uint8_t*>(image_data.GetData());
      if (width > 0 && height > 0 && src) {
        // The AKI ArrayBuffer is only guaranteed to be alive for the duration
        // of this callback, so take an owned copy of the pixels right here.
        size_t size = static_cast<size_t>(stride) * height;
        if ((frame->data_ = static_cast<uint8_t*>(malloc(size)))) {
          memcpy(frame->data_, src, size);
          frame->size_.width_ = width;
          frame->size_.height_ = height;
          frame->stride_ = stride;
          frame->format_ = image_info["pixelFormat"].As<int>();
        } else {
          LOGE("[ScreenshotAdapter] out of memory (%zu bytes)", size);
        }
      }

      insert_promise.set_value(std::move(frame));
    };

    start_capture_func->Invoke<void>(callback);

    return insert_promise.get_future().get();
  }
  return std::make_unique<DesktopFrame>();
}

}  // namespace ohos::adapter

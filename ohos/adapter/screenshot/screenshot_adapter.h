// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_SCREENSHOT_SCREENSHOT_ADAPTER_H_
#define OHOS_ADAPTER_SCREENSHOT_SCREENSHOT_ADAPTER_H_

#include <string>

#include "ohos/adapter/aki_hook/aki_hook.h"
#include "ohos/adapter/export.h"

namespace ohos::adapter {

struct DesktopSize {
  int32_t width() const { return width_; }
  int32_t height() const { return height_; }

  int32_t width_ = 0;
  int32_t height_ = 0;
};

struct DesktopFrame {
  DesktopSize size() const { return size_; }
  int stride() const { return stride_; }
  uint8_t* data() const { return data_; }

  DesktopSize size_;
  int stride_;
  uint8_t* data_;
};

class ADAPTER_EXPORT_API ScreenshotAdapter {
 public:
  static ScreenshotAdapter& GetInstance();

  std::unique_ptr<DesktopFrame> StartCapture();

 private:
  ScreenshotAdapter() = default;
  ~ScreenshotAdapter() = default;
  ScreenshotAdapter(const ScreenshotAdapter&) = delete;
  ScreenshotAdapter& operator=(const ScreenshotAdapter&) = delete;
};

}  // namespace ohos::adapter

#endif  // OHOS_ADAPTER_SCREENSHOT_SCREENSHOT_ADAPTER_H_

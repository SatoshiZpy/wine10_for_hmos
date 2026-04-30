// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_APP_WINDOW_SYSTEM_FLOATING_WINDOW_ADAPTER_H_
#define OHOS_ADAPTER_APP_WINDOW_SYSTEM_FLOATING_WINDOW_ADAPTER_H_

#include <cstdint>
#include <string>

#include "ohos/adapter/export.h"
#include "ohos/adapter/window/window_common.h"

namespace ohos::adapter::window {

class ADAPTER_EXPORT_API SystemFloatingWindowAdapter {
 private:
  SystemFloatingWindowAdapter() = default;
  ~SystemFloatingWindowAdapter() = default;

 public:
  static SystemFloatingWindowAdapter& GetInstance();

  int32_t Create(const NewWindowParam& param);
  void Close(int32_t id);
  void SetBounds(int32_t id,
                 const WindowRect& rect);
  void SetWindowLimits(int32_t min_width,
                       int32_t min_height,
                       int32_t max_width,
                       int32_t max_height,
                       int32_t id);
  void StartWindowMoving(int32_t id);
};

}  // namespace ohos::adapter::window
#endif  // OHOS_ADAPTER_APP_WINDOW_SYSTEM_FLOATING_WINDOW_ADAPTER_H_

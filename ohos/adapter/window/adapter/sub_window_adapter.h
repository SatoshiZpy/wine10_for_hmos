// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_SUBWINDOW_SUBWINDOW_ADAPTER_H_
#define OHOS_ADAPTER_SUBWINDOW_SUBWINDOW_ADAPTER_H_

#include <cstdint>
#include <string>

#include "ohos/adapter/export.h"
#include "ohos/adapter/window/window_common.h"

namespace ohos::adapter::window {

class ADAPTER_EXPORT_API SubWindowAdapter {
 public:

  static SubWindowAdapter& GetInstance();

  int32_t Create(const NewWindowParam& param);
  void Close(int32_t id);

  void Show(int32_t id);
  void Hide(int32_t id);

  void SetBounds(int32_t id, const WindowRect& rect);

 private:
  SubWindowAdapter() = default;
  ~SubWindowAdapter() = default;
};

}  // namespace ohos::adapter::window
#endif  // OHOS_ADAPTER_SUBWINDOW_SUBWINDOW_ADAPTER_H_

// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/window/wine_float_window.h"

#include "ohos/adapter/common/logging.h"
#include "ohos/adapter/window/adapter/system_floating_window_adapter.h"

namespace ohos::adapter::window {

WineFloatWindow::WineFloatWindow()
  : WineWindow() {}

WineFloatWindow::~WineFloatWindow() = default;

WineFloatWindow* WineFloatWindow::AsFloatWindow()
{
  return this;
}

int32_t WineFloatWindow::OnCreateWindow()
{
  return SystemFloatingWindowAdapter::GetInstance().Create(GetParam());
}

void WineFloatWindow::Close()
{
  SystemFloatingWindowAdapter::GetInstance().Close(GetWidget());
}

void WineFloatWindow::SetBounds(const WindowRect& rect)
{
  previous_bounds_ = bounds_;
  bounds_ = rect;

  SystemFloatingWindowAdapter::GetInstance().SetBounds(GetWidget(), rect);
}

}  // namespace ohos::adapter::window

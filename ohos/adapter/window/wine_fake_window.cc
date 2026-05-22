// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/window/wine_fake_window.h"

namespace ohos::adapter::window {

WineFakeWindow::WineFakeWindow()
  : WineWindow() {}

WineFakeWindow::~WineFakeWindow() = default;

WineFakeWindow* WineFakeWindow::AsFakeWindow()
{
  return this;
}

}  // namespace ohos::adapter::window

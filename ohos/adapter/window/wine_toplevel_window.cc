// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/window/wine_toplevel_window.h"

#include "ohos/adapter/common/logging.h"
#include "ohos/adapter/window/adapter/app_window_adapter.h"

namespace ohos::adapter::window {

WineToplevelWindow::WineToplevelWindow()
  : WineWindow() {}

WineToplevelWindow::~WineToplevelWindow() = default;

WineToplevelWindow* WineToplevelWindow::AsToplevelWindow()
{
  return this;
}

int32_t WineToplevelWindow::OnCreateWindow()
{
  if (GetWidget() == USER_DEFAULT_WIDGET_ID) {
    if (!WineWindow::InitSurface()) {
        LOGE("init surface failed");
        return -1;
    }
    return USER_DEFAULT_WIDGET_ID;
  }
  return AppWindowAdapter::GetInstance().Create(GetParam());
}

void WineToplevelWindow::Show()
{
  AppWindowAdapter::GetInstance().Show(GetWidget());
}

void WineToplevelWindow::Hide()
{
  AppWindowAdapter::GetInstance().Hide(GetWidget());
}

void WineToplevelWindow::Close()
{
  AppWindowAdapter::GetInstance().Close(GetWidget());
}

void WineToplevelWindow::SetBounds(const WindowRect& rect)
{
  AppWindowAdapter::GetInstance().SetBounds(GetWidget(), rect, []{});
}

}  // namespace ohos::adapter::window

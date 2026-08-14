// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/window/wine_toplevel_window.h"

#include <memory>

#include "ohos/adapter/common/logging.h"
#include "ohos/adapter/window/adapter/app_window_adapter.h"
#include "ohos/adapter/window/wine_window_manager.h"
#include "ohos/adapter/xcomponent/event/window_event_common.h"

namespace ohos::adapter::window {

using namespace ohos::adapter::xcomponent;

WineToplevelWindow::WineToplevelWindow()
  : WineWindow() {}

WineToplevelWindow::~WineToplevelWindow() = default;

WineToplevelWindow* WineToplevelWindow::AsToplevelWindow()
{
  return this;
}

int32_t WineToplevelWindow::OnCreateWindow()
{
  // reuse the initial UI window
  if (GetWidget() == USER_DEFAULT_WIDGET_ID) {
    LOGI("reuse the initial UI window");
    if (!WineWindow::InitSurface()) {
      LOGW("init failed: surface not ready");
    }
    return USER_DEFAULT_WIDGET_ID;
  }
  return AppWindowAdapter::GetInstance().Create(GetParam());
}

void WineToplevelWindow::Show()
{
  AppWindowAdapter::GetInstance().Show(GetWidget());
  WineWindow::Show();
}

void WineToplevelWindow::Hide()
{
  AppWindowAdapter::GetInstance().Hide(GetWidget());
  WineWindow::Hide();
}

void WineToplevelWindow::Close()
{
  if (WineWindowManager::GetInstance().IsWindowAtLast()) {
    LOGW("waiting for the app thread to exit safely, cache last window");
    AppWindowAdapter::GetInstance().Hide(GetWidget());
    WineWindowManager::GetInstance().SetLastActiveWidgetId(GetWidget());
  } else {
    AppWindowAdapter::GetInstance().Close(GetWidget());
  }
}

bool WineToplevelWindow::IsVisible() const {
  return WineWindow::IsVisible() && PlatformWindowState::kMinimized != state_;
}

void WineToplevelWindow::SetBounds(const WindowRect& rect)
{
  previous_bounds_ = bounds_;
  bounds_ = rect;
  AppWindowAdapter::GetInstance().SetBounds(GetWidget(), rect, []{});
}

void WineToplevelWindow::TriggerStateChanges() {
  if (state_ == PlatformWindowState::kMinimized) {
    AppWindowAdapter::GetInstance().Minimize(GetWidget());
  } else if (state_ == PlatformWindowState::kFullScreen) {
    AppWindowAdapter::GetInstance().SetFullScreen(GetWidget());
  } else if (state_ == PlatformWindowState::kMaximized) {
    AppWindowAdapter::GetInstance().Maximize(GetWidget());
  } else if (state_ == PlatformWindowState::kNormal) {
    if (previous_state_ == PlatformWindowState::kMinimized) {
      AppWindowAdapter::GetInstance().Restore(GetWidget());
    } else {
      AppWindowAdapter::GetInstance().UnMaximize(GetWidget());
    }
  }

  // loses focus when enter or exiting full screen
  if (previous_state_ == PlatformWindowState::kFullScreen ||
        state_ == PlatformWindowState::kFullScreen) {
    AppWindowAdapter::GetInstance().Activate(GetWidget());
  }
}

void WineToplevelWindow::UpdateStateChanges() {}

}  // namespace ohos::adapter::window

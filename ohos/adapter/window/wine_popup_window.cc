// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/window/wine_popup_window.h"

#include "ohos/adapter/common/logging.h"
#include "ohos/adapter/window/adapter/sub_window_adapter.h"

namespace ohos::adapter::window {

WinePopupWindow::WinePopupWindow(std::shared_ptr<WineWindow> parent)
  : WineWindow() {
  set_parent_window(parent);
}

WinePopupWindow::~WinePopupWindow() = default;

WinePopupWindow* WinePopupWindow::AsPopupWindow()
{
  return this;
}

int32_t WinePopupWindow::OnCreateWindow()
{
  return SubWindowAdapter::GetInstance().Create(GetParam());
}

void WinePopupWindow::Show()
{
  SubWindowAdapter::GetInstance().Show(GetWidget());
  WineWindow::Show();
}

void WinePopupWindow::Hide()
{
  SubWindowAdapter::GetInstance().Hide(GetWidget());
  WineWindow::Hide();
}

void WinePopupWindow::Close()
{
  SubWindowAdapter::GetInstance().Close(GetWidget());
}

bool WinePopupWindow::IsVisible() const {
  return WineWindow::IsVisible() && PlatformWindowState::kMinimized != state_;
}

void WinePopupWindow::SetBounds(const WindowRect& rect)
{
  previous_bounds_ = bounds_;
  bounds_ = rect;

  SubWindowAdapter::GetInstance().SetBounds(GetWidget(), rect);
}

void WinePopupWindow::TriggerStateChanges() {
  if (state_ == PlatformWindowState::kMinimized) {
    SubWindowAdapter::GetInstance().Minimize(GetWidget());
  } else if (state_ == PlatformWindowState::kFullScreen) {
    SubWindowAdapter::GetInstance().SetFullScreen(GetWidget());
  } else if (state_ == PlatformWindowState::kMaximized) {
    SubWindowAdapter::GetInstance().Maximize(GetWidget());
  } else if (state_ == PlatformWindowState::kNormal) {
    if (previous_state_ == PlatformWindowState::kMinimized) {
      SubWindowAdapter::GetInstance().Restore(GetWidget());
    } else {
      SubWindowAdapter::GetInstance().UnMaximize(GetWidget());
    }
  }

  // loses focus when enter or exiting full screen
  if (previous_state_ == PlatformWindowState::kFullScreen ||
        state_ == PlatformWindowState::kFullScreen) {
    SubWindowAdapter::GetInstance().Activate(GetWidget());
  }
}

void WinePopupWindow::UpdateStateChanges() {}

}  // namespace ohos::adapter::window

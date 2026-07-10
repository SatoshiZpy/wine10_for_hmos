// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/window/wine_window.h"

#include "ohos/adapter/common/logging.h"
#include "ohos/adapter/window/window_common.h"
#include "ohos/adapter/xcomponent/adapter/window_adapter.h"
#include "ohos/adapter/xcomponent/xcomponent_utils.h"

namespace ohos::adapter::window {

using namespace ohos::adapter::xcomponent;

WineWindow::WineWindow() {}

WineWindow::~WineWindow() {}

WindowStyle WineWindow::BuilderStyle(const NewWindowParam& param) {
  WindowStyle styles;
  bool has_title_bar = has_feature(WindowFeatures::WINDOW_TITLE_BAR, param.feature_mask);
  styles.hide_title_bar = !has_title_bar;
  styles.close_button = has_title_bar && has_feature(WindowFeatures::WINDOW_CLOSE_BUTTON, param.feature_mask);
  styles.minimize_button = has_title_bar && has_feature(WindowFeatures::WINDOW_MINIMIZE_BUTTON, param.feature_mask);
  styles.maximize_button = has_title_bar && has_feature(WindowFeatures::WINDOW_MAXIMIZE_BUTTON, param.feature_mask);
  styles.use_floating_window = has_feature(WindowFeatures::USE_FLOATING_WINDOW, param.feature_mask);
  styles.use_dark_mode = has_feature(WindowFeatures::USE_DARK_MODE, param.feature_mask);
  styles.always_on_top = is_state(WindowState::WINDOW_TOP_MOST, param.window_state_mask);

  bool is_enable = !is_state(WindowState::WINDOW_DISABLED, param.window_state_mask);
  styles.resizable = is_enable && has_feature(WindowFeatures::WINDOW_RESIZABLE, param.feature_mask);
  styles.focusable = is_enable && !has_feature(WindowFeatures::WINDOW_NO_ACTIVATION, param.feature_mask);
  styles.touchable = is_enable && has_feature(WindowFeatures::WINDOW_TOUCHABLE, param.feature_mask);
  styles.show_window = is_state(WindowState::WINDOW_VISIBLE, param.window_state_mask);
  return styles;
}

bool WineWindow::Initialize(const NewWindowParam& param)
{
  widget_ = param.widget_id;
  window_unique_id_ = XComponentUtils::ConvertWidgetIdToWindowId(widget_);
  type_ = (WindowInitType)param.window_type;
  features_ = param.feature_mask;
  window_state_ = param.window_state_mask;
  hwnd_ = param.hwnd;
  owner_pid_ = param.owner_pid;
  newParam_ = param;

  OnInitialize(param);

  int widget_id = OnCreateWindow();
  if (widget_id <= 0) {
      return false;
  }
  SetWidget(widget_id);

  RegistWindowEvent();
  return true;
}

WineFakeWindow* WineWindow::AsFakeWindow()
{
  return nullptr;
}

WineFloatWindow* WineWindow::AsFloatWindow()
{
  return nullptr;
}

WinePopupWindow* WineWindow::AsPopupWindow()
{
  return nullptr;
}

WineToplevelWindow* WineWindow::AsToplevelWindow()
{
  return nullptr;
}

void WineWindow::SetWidget(int32_t new_widget)
{
  if (new_widget <= 0 || widget_ == new_widget) {
    return;
  }

  LOGI("widget id changed: %{public}d --> %{public}d", widget_, new_widget);
  widget_ = new_widget;
  window_unique_id_ = XComponentUtils::ConvertWidgetIdToWindowId(new_widget);
}

void WineWindow::OnSurfaceCreated()
{
  LOGI("surface created id: %{public}d", GetWidget());
  UpdateSurface(WindowStateMonitor::State::WINDOW_ENABLE);
}

void WineWindow::OnSurfaceDestoryed()
{
  LOGI("surface destoryed id: %{public}d", GetWidget());
  UpdateSurface(WindowStateMonitor::State::WINDOW_DISABLE);
}

void WineWindow::Show() {}

void WineWindow::Hide() {}

void WineWindow::Close() {}

bool WineWindow::IsVisible() const
{
  return false;
}

void WineWindow::SetTitle(const std::string& title) {}

void WineWindow::SetBounds(const WindowRect& rect) {}

void WineWindow::SetFullscreen(bool fullscreen, int64_t target_display_id) {}

void WineWindow::Maximize() {}

void WineWindow::Minimize() {}

void WineWindow::Restore() {}

void WineWindow::Activate() {}

void WineWindow::Deactivate() {}

std::string WineWindow::GetWindowUniqueId() const
{
  return window_unique_id_;
}

bool WineWindow::InitSurface()
{
  uint64_t surface_id;
  window_ =  WindowAdapter::GetInstance().GetWindow(GetWidget());
  if (!window_) {
      LOGW("window is not exists, windowId: %{public}d", GetWidget());
      return false;
  }

  auto ret = OH_NativeWindow_NativeWindowHandleOpt(window_, SET_FORMAT, NATIVEBUFFER_PIXEL_FMT_BGRA_8888);
  if (ret != 0) {
      LOGE("set format error: %{public}d", ret);
      return false;
  }

  ret = OH_NativeWindow_GetSurfaceId(reinterpret_cast<OHNativeWindow*>(window_), &surface_id);
  if (ret != 0) {
    LOGE("get xcomponent surface id error: %{public}d", ret);
    return false;
  }

  LOGI("Get surface id: %{public}lu", surface_id);
  SetSurfaceId(surface_id);
  return true;
}

OHNativeWindow* WineWindow::GetNativeWindow()
{
  if (window_ != nullptr || InitSurface()) {
    return window_;
  }

  // Waiting for surface initialization
  if (!HasSurface()) {
    LOGW("surface not ready, waiting...");
    if (!WaitSurface()) {
      LOGE("wait surface timeout");
    }
  }

  if (!InitSurface()) {
      LOGE("init surface failed");
      return nullptr;
  }
  return window_;
}

}  // namespace ohos::adapter::window

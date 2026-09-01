// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/window/wine_window.h"

#include "ohos/adapter/common/logging.h"
#include "ohos/adapter/window/window_common.h"
#include "ohos/adapter/xcomponent/adapter/window_adapter.h"
#include "ohos/adapter/xcomponent/xcomponent_utils.h"

namespace ohos::adapter::window {

WineWindow::WineWindow()
{
  state_ = PlatformWindowState::kNormal;
}

WineWindow::~WineWindow()
{
  UnRegistWindowEvent();
}

WindowStyle WineWindow::BuilderStyle(const NewWindowParam& param)
{
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
  bounds_ = param.bounds;
  window_style_ = BuilderStyle(param);
  activatable_ = window_style_.touchable;
  widget_ = param.widget_id;
  window_unique_id_ = XComponentUtils::ConvertWidgetIdToWindowId(widget_);
  type_ = (WindowInitType)param.window_type;
  features_ = param.feature_mask;
  window_state_ = param.window_state_mask;
  hwnd_ = param.hwnd;
  owner_pid_ = param.owner_pid;
  newParam_ = param;

  OnInitialize();

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

void WineWindow::RegistWindowEvent() {
  WindowAdapter::GetInstance().RegistWindowEvent(
    GetWidget(), [this](int32_t widget_id, std::shared_ptr<Event> event) {
      if (widget_id != GetWidget()) {
        LOGE("handle event error no matched widget_id: %{public}d,%{public}d", widget_id, GetWidget());
        return;
      }

      // Handle arkui events on the wine UI thread
      HandleEvent(event);
    });
}

void WineWindow::UnRegistWindowEvent() {
  WindowAdapter::GetInstance().UnregistWindowEvent(GetWidget());
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

void WineWindow::Applied(const WindowRect& origin_bounds,
                         const WindowRect& new_bounds,
                         bool force)
{
  // Forward window change events to Wine.
  bool origin_changed =
      origin_bounds.left != new_bounds.left || origin_bounds.top != new_bounds.top;
  bool size_changed =
      origin_bounds.width != new_bounds.width || origin_bounds.height != new_bounds.height;
  if ( origin_changed || size_changed || force) {
    LOGI("Applied new_bounds: %{public}d, %{public}d, %{public}d, %{public}d reason %{public}d",
      new_bounds.left, new_bounds.top, new_bounds.width, new_bounds.height, pending_reason_);

    input_event event;
    event.type = OHOS_WINDOW_RECT_CHANGE_EVENT;
    event.hwnd = hwnd();
    event.widget_id = GetWidget();
    event.data.window_rect_change_event.left = new_bounds.left;
    event.data.window_rect_change_event.top = new_bounds.top;
    event.data.window_rect_change_event.width = new_bounds.width;
    event.data.window_rect_change_event.height = new_bounds.height;
    event.data.window_rect_change_event.reason = pending_reason_;
    EventManager::GetInstance().WindowDispatcher().ExecuteCallback(event);
  }
}

void WineWindow::Show()
{
  if (is_visible_) {
    return;
  }
  is_visible_ = true;
}

void WineWindow::Hide()
{
  if (!is_visible_) {
    return;
  }
  is_visible_ = false;
}

void WineWindow::Close() {}

bool WineWindow::IsVisible() const
{
  return is_visible_;
}

void WineWindow::SetTitle(const std::string& title) {}

void WineWindow::SetBounds(const WindowRect& rect) {}

void WineWindow::SetFullscreen(bool fullscreen) {
  // We must track the previous state to correctly say our state as long as it
  // can be the maximized instead of normal one.
  PlatformWindowState new_state = PlatformWindowState::kUnknown;
  if (fullscreen) {
    new_state = PlatformWindowState::kFullScreen;
  } else if (previous_state_ == PlatformWindowState::kMinimized) {
    new_state = previous_enter_fullscreen_state_;
  } else {
    new_state = previous_state_;
  }

  SetWindowState(new_state);
}

void WineWindow::Maximize()
{
  SetWindowState(PlatformWindowState::kMaximized);
}

void WineWindow::Minimize()
{
  SetWindowState(PlatformWindowState::kMinimized);
}

void WineWindow::Restore()
{
  SetWindowState(PlatformWindowState::kNormal);
}

void WineWindow::Activate()
{
  input_event event;
  event.type = OHOS_WINDOW_FOCUS_EVENT;
  event.hwnd = hwnd();
  event.widget_id = GetWidget();
  event.data.window_focus_event.widget_id = GetWidget();
  EventManager::GetInstance().WindowDispatcher().ExecuteCallback(event);
}

void WineWindow::Deactivate()
{
  input_event event;
  event.type = OHOS_WINDOW_BLUR_EVENT;
  event.hwnd = hwnd();
  event.widget_id = GetWidget();
  event.data.window_focus_event.widget_id = GetWidget();
  EventManager::GetInstance().WindowDispatcher().ExecuteCallback(event);
}

std::string WineWindow::GetWindowUniqueId() const
{
  return window_unique_id_;
}

bool WineWindow::InitSurface()
{
  uint64_t surface_id;
  window_ =  WindowAdapter::GetInstance().GetWindow(GetWidget());
  if (!window_) {
      LOGW("window is not exists, widgetId: %{public}d", GetWidget());
      return false;
  }

  auto ret = OH_NativeWindow_NativeWindowHandleOpt(window_, SET_FORMAT, NATIVEBUFFER_PIXEL_FMT_BGRA_8888);
  if (ret != 0) {
      LOGE("set format error: %{public}d", ret);
      return false;
  }

  int64_t usage = NATIVEBUFFER_USAGE_CPU_WRITE | NATIVEBUFFER_USAGE_MEM_DMA | NATIVEBUFFER_USAGE_CPU_READ;
  ret = OH_NativeWindow_NativeWindowHandleOpt(window_, SET_USAGE, usage);
  if (ret != 0) {
    LOGE("set usage error: %{public}d", ret);
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

  if (!IsVisible()) {
    LOGW("the window status is invisible, widgetId: %{public}d", GetWidget());
    return nullptr;
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

void WineWindow::HandleEvent(std::shared_ptr<Event> event) {
  switch (event->type()) {
    case EventType::ET_SURFACE_FOCUS: {
      OnFocusEvent();
      break;
    }
    case EventType::ET_SURFACE_BLUR: {
      OnBlurEvent();
      break;
    }
    case EventType::ET_WINDOW_CHANGE: {
      OnWindowEvent(event);
      break;
    }
    case EventType::ET_WINDOW_RECT_CHANGE: {
      OnWindowRectChangeEvent(event);
      break;
    }
    case EventType::ET_WINDOW_STATUS_CHANGE: {
      OnWindowStatusChangeEvent(event);
      break;
    }
    case EventType::ET_WINDOW_CAPTION_BUTTON_RECT_CHANGE: {
      OnWindowCaptionButtonRectChangeEvent(event);
      break;
    }
    case EventType::ET_WINDOW_DISPLAY_ID_CHANGE: {
      OnWindowDisplayIdChangeEvent(event);
      break;
    }
    default:
      LOGE("EventType::ET_UNKNOWN not handle: %{public}d", event->type());
      break;
  }
}

void WineWindow::OnFocusEvent() {
  if (!activatable_) {
    return;
  }
  SetFocus(true);
  Activate();
}

void WineWindow::OnBlurEvent() {
  if (!activatable_) {
    return;
  }
  SetFocus(false);
  Deactivate();
}

void WineWindow::OnWindowEvent(std::shared_ptr<Event> event) {
  auto window_event = std::static_pointer_cast<WindowEvent>(event);
  auto window_type = window_event->window_event_type_;

  switch (window_type) {
    case WindowEventType::WINDOW_SHOWN:
      if (state_ == PlatformWindowState::kMinimized) {
        SetWindowState(previous_enter_minimize_state_, false);
        pending_reason_ = 2; /* RectChangeReason::RECOVER */
        Applied(previous_bounds_, bounds_, true);
      }
      break;
    case WindowEventType::WINDOW_HIDDEN:
      SetWindowState(PlatformWindowState::kMinimized, false);
      break;
    default:
      break;
  }
}

void WineWindow::OnWindowStatusChangeEvent(
    std::shared_ptr<Event> event) {
  auto window_event = std::static_pointer_cast<WindowStatusChangeEvent>(event);
  is_split_screen_ = window_event->status == WindowStatusType::SPLIT_SCREEN;
}

void WineWindow::OnWindowCaptionButtonRectChangeEvent(
    std::shared_ptr<Event> event) {
  auto window_event = std::static_pointer_cast<WindowCaptionButtonRectChangeEvent>(event);
  caption_button_bounds_ = WindowRect{window_event->right,
                                      window_event->top,
                                      window_event->width,
                                      window_event->height};
}

void WineWindow::OnWindowRectChangeEvent(std::shared_ptr<Event> event) {
  auto window_event = std::static_pointer_cast<WindowRectChangeEvent>(event);
  auto reason = window_event->reason;
  PlatformWindowState current_state = state_;
  bool handle = false;
  PlatformWindowState new_state;

  pending_reason_ = static_cast<int32_t>(reason);

  UpdateBoundsChanges(window_event->top, window_event->left,
                      window_event->width, window_event->height);

  // receive event from arkui,we need to deal with it.
  switch (current_state) {
    case PlatformWindowState::kNormal: {
      if (reason == RectChangeReason::MAXIMIZE) {
        handle = true;
        new_state = PlatformWindowState::kMaximized;
        restored_bounds_ = previous_bounds_;
      }
      break;
    }
    case PlatformWindowState::kMaximized:
      if (reason == RectChangeReason::RECOVER) {
        handle = true;
        new_state = PlatformWindowState::kNormal;
      } else if (reason == RectChangeReason::MAXIMIZE) {
        // Double-click the title bar to enter the branch.
        restored_bounds_ = previous_bounds_;
      }
      break;
    default:
      break;
  }

  if (handle) {
    // This event is from ArkUI, the changes of state is already triggered.
    SetWindowState(new_state, false);
  }
}

void WineWindow::OnWindowDisplayIdChangeEvent(std::shared_ptr<Event> event) {
  auto window_event = std::static_pointer_cast<WindowDisplayIdChangeEvent>(event);
  SetCurrentDisplayId(window_event->display_id);
}

void WineWindow::UpdateBoundsChanges(int top, int left,
                                     int width, int height) {
  previous_bounds_ = bounds_;
  bounds_ = WindowRect{left, top, width, height};
  // we need to change the window bounds when receiving the event of
  // bounds changing.
  Applied(previous_bounds_, bounds_);
}

void WineWindow::SetWindowState(PlatformWindowState new_state, bool isTrigger) {
  if (new_state == state_) {
    return;
  }
  LOGI("SetWindowState from: %{public}d, to: %{public}d",
    static_cast<std::underlying_type_t<PlatformWindowState>>(state_),
    static_cast<std::underlying_type_t<PlatformWindowState>>(new_state));

  if (new_state == PlatformWindowState::kMinimized) {
    // Record the state before minimize.
    previous_enter_minimize_state_ = state_;
  }

  previous_state_ = state_;
  state_ = new_state;

  // we need to record the last window state when enter full screen
  if (previous_enter_fullscreen_state_ != previous_state_ &&
      (previous_state_ == PlatformWindowState::kNormal ||
       previous_state_ == PlatformWindowState::kMaximized)) {
    previous_enter_fullscreen_state_ = previous_state_;
  }

  if (isTrigger) {
    TriggerStateChanges();
  } else {
    UpdateStateChanges();
  }
}

}  // namespace ohos::adapter::window

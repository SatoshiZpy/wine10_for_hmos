// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <js_native_api.h>
#include <js_native_api_types.h>
#include <string>

#include "ohos/adapter/aki_hook/aki_hook.h"
#include "ohos/adapter/common/logging.h"
#include "ohos/adapter/common/trace.h"
#include "ohos/adapter/window/window_common.h"
#include "ohos/adapter/xcomponent/adapter/window_adapter.h"

namespace ohos::adapter::xcomponent {

void OnWindowInitSize(const aki::Value window_rect,
                      const aki::Value drawable_rect) {
  WindowRect rect;
  rect.top = window_rect["top"].As<int>();
  rect.left = window_rect["left"].As<int>();
  rect.width = window_rect["width"].As<int64_t>();
  rect.height = window_rect["height"].As<int64_t>();

  WindowRect content_rect;
  content_rect.top = drawable_rect["top"].As<int>();
  content_rect.left = drawable_rect["left"].As<int>();
  content_rect.width = drawable_rect["width"].As<int64_t>();
  content_rect.height = drawable_rect["height"].As<int64_t>();

  WindowAdapter::GetInstance().SetInitialBounds(rect, content_rect);
}

void OnWindowStatusChange(int32_t widget_id,
                          WindowStatusType status) {
  TRACE_EVENT_1("OnWindowStatusChange", "widget_id", widget_id);
 
  auto event = std::make_shared<WindowStatusChangeEvent>();
  event->status = status;
  WindowAdapter::GetInstance().NotifyWindowEvent(widget_id, event);
}

void OnWindowInitState(const WindowStatusType state) {
  WindowAdapter::GetInstance().SetInitialState(state);
}

void OnWindowRectChange(int32_t widget_id,
                        const aki::Value window_size,
                        RectChangeReason reason) {
  TRACE_EVENT_1("OnWindowRectChange", "widget_id", widget_id);
  bool handle;

  switch (reason) {
    case RectChangeReason::MAXIMIZE:
    case RectChangeReason::RECOVER:
    // Events for dragging to resize window.
    case RectChangeReason::DRAG:
    case RectChangeReason::DRAG_END:
    // We receive an undefined reason when
    // entering split screen.
    case RectChangeReason::UNDEFINED:
    // Events for dragging to move window.
    case RectChangeReason::MOVE:
      handle = true;
      break;
    default:
      handle = false;
  }

  if (!handle) {
    return;
  }

  auto event = std::make_shared<WindowRectChangeEvent>();
  event->reason = reason;
  event->top = window_size["top"].As<int>();
  event->left = window_size["left"].As<int>();
  event->width = window_size["width"].As<int>();
  event->height = window_size["height"].As<int>();
  WindowAdapter::GetInstance().NotifyWindowEvent(widget_id, event);
}

void OnWindowSizeChange(int32_t widget_id,
                        const aki::Value window_size) {
  TRACE_EVENT_1("OnWindowSizeChange", "widget_id", widget_id);
  auto event = std::make_shared<WindowSizeChangeEvent>();
  event->top = window_size["top"].As<int>();
  event->left = window_size["left"].As<int>();
  event->width = window_size["width"].As<int>();
  event->height = window_size["height"].As<int>();
  WindowAdapter::GetInstance().NotifyWindowEvent(widget_id, event);
}

void OnWindowEvent(int32_t widget_id,
                   WindowEventType type) {
  TRACE_EVENT_0("OnWindowEvent");

  bool handle = false;
  switch (type) {
    case WindowEventType::WINDOW_SHOWN:
    case WindowEventType::WINDOW_HIDDEN:
    case WindowEventType::WINDOW_CLOSE:
      handle = true;
      break;
    default:
      break;
  }

  if (!handle) {
    return;
  }
  auto event = std::make_shared<WindowEvent>(type);
  WindowAdapter::GetInstance().NotifyWindowEvent(widget_id, event);
}

void OnWindowVisibilityChange(int32_t widget_id,
                              bool window_visible) {
  TRACE_EVENT_1("OnWindowVisibilityChange", "widget_id", widget_id);
  WindowEventType type = window_visible ? WindowEventType::WINDOW_VISIBLE
                                        : WindowEventType::WINDOW_OCCLUDED;
  auto event = std::make_shared<WindowEvent>(type);
  WindowAdapter::GetInstance().NotifyWindowEvent(widget_id, event);
}

void OnKeyboardHeightChange(int32_t widget_id, int32_t height) {
  TRACE_EVENT_1("OnKeyboardHeightChange", "height", height);
  WindowAdapter::GetInstance().NotifyKeyboardHeightEvent(widget_id, height);
}

void OnCaptionButtonRectChange(int32_t widget_id,
                               const aki::Value caption_button_rect) {
  TRACE_EVENT_1("OnCaptionButtonRectChange", "widget_id", widget_id);
  auto event = std::make_shared<WindowCaptionButtonRectChangeEvent>();
  event->top = caption_button_rect["top"].As<int>();
  event->right = caption_button_rect["right"].As<int>();
  event->width = caption_button_rect["width"].As<int>();
  event->height = caption_button_rect["height"].As<int>();
  WindowAdapter::GetInstance().NotifyWindowEvent(widget_id, event);
}

void SetSystemWindowLimits(const aki::Value window_limits) {
  WindowLimits limits;
  limits.max_width = window_limits["maxWidth"].As<int>();
  limits.max_height = window_limits["maxHeight"].As<int>();
  limits.min_width = window_limits["minWidth"].As<int>();
  limits.min_height = window_limits["minHeight"].As<int>();
  WindowAdapter::GetInstance().SetSystemWindowLimits(limits);
}

void OnDeviceModeChange(int32_t widget_id,
                        ChangeEventType type) {
  TRACE_EVENT_1("OnDeviceInfoChange", "widget_id", widget_id);

  auto event = std::make_shared<DeviceInfoChangeEvent>();
  event->change_event_type_ = type;
  WindowAdapter::GetInstance().NotifyWindowEvent(widget_id, event);
}

void OnTrayIconClick() {
  // Not tied to a window: dispatch to every window of this process.
  TRACE_EVENT_0("OnTrayIconClick");
  auto event = std::make_shared<TrayIconClickEvent>();
  WindowAdapter::GetInstance().NotifyWindowEvent(BROADCAST_WIDGET_ID, event);
}

JSBIND_GLOBAL() {
  JSBIND_FUNCTION(OnWindowInitSize);
  JSBIND_FUNCTION(OnWindowStatusChange);
  JSBIND_FUNCTION(OnWindowInitState);
  JSBIND_FUNCTION(OnWindowRectChange);
  JSBIND_FUNCTION(OnWindowSizeChange);
  JSBIND_FUNCTION(OnWindowEvent);
  JSBIND_FUNCTION(OnWindowVisibilityChange);
  JSBIND_FUNCTION(OnKeyboardHeightChange);
  JSBIND_FUNCTION(OnCaptionButtonRectChange);
  JSBIND_FUNCTION(SetSystemWindowLimits);
  JSBIND_FUNCTION(OnDeviceModeChange);
  JSBIND_FUNCTION(OnTrayIconClick);
}

}  // namespace ohos::adapter::xcomponent

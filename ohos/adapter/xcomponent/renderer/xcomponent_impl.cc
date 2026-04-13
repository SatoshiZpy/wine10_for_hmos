// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/xcomponent/renderer/xcomponent_impl.h"

#include "ohos/adapter/common/logging.h"
#include "ohos/adapter/common/trace.h"
#include "ohos/adapter/xcomponent/adapter/window_adapter.h"
#include "ohos/adapter/xcomponent/xcomponent_utils.h"

namespace ohos::adapter::xcomponent {

void OnSurfaceCreatedCB(OH_NativeXComponent* component, void* window) {
  XComponentInfo info;
  if (!XComponentUtils::GetRenderInfo(component, info)) {
    return;
  }
  auto impl = XComponentUtils::GetXComponent(info.widget_id);
  if (!impl) {
    return;
  }
  if (info.widget_id != impl->GetWidget()) {
    LOGE("xcomponent not matched, receive id %{public}d, id %{public}d",
         info.widget_id, impl->GetWidget());
    return;
  }

  if (info.pid != impl->GetOwnerPid()) {
    LOGE("xcomponent not matched, receive pid %{public}d, owner pid %{public}d",
         info.pid, impl->GetOwnerPid());
    return;
  }

  uint64_t width = 0;
  uint64_t height = 0;
  int32_t ret = OH_NativeXComponent_GetXComponentSize(component, window, &width, &height);
  if (ret != OH_NATIVEXCOMPONENT_RESULT_SUCCESS) {
    LOGE("Get xcomponent surface size error:%{public}lu %{public}lu, widget_id:%{public}d",
         width, height, info.widget_id);
    return;
  }
  
  LOGI("xcomponent create success, widget_id:%{public}d, owner_pid:%{public}d, size:(%{public}lu,%{public}lu)",
      info.widget_id, info.pid, width, height);
  WindowAdapter::GetInstance().AddWindow(info.widget_id, (OHNativeWindow *)window, info.pid);
  impl->SetInitialBounds(width, height);
  impl->OnSurfaceCreated();
}

void OnSurfaceChangedCB(OH_NativeXComponent* component, void* window) {
  XComponentInfo info;
  if (!XComponentUtils::GetRenderInfo(component, info)) {
    return;
  }
  auto impl = XComponentUtils::GetXComponent(info.widget_id);
  if (!impl) {
    return;
  }
  impl->OnSurfaceChanged();
}

void OnSurfaceDestroyedCB(OH_NativeXComponent* component, void* window) {
  XComponentInfo info;
  if (!XComponentUtils::GetRenderInfo(component, info)) {
    return;
  }
  auto impl = XComponentUtils::GetXComponent(info.widget_id);
  if (!impl) {
    return;
  }
  if (info.widget_id != impl->GetWidget()) {
    LOGE("xcomponent not matched, receive id %{public}d, id %{public}d",
         info.widget_id, impl->GetWidget());
    return;
  }
  LOGI("Destroy XComponent, widget_id:%{public}d", info.widget_id);
  WindowAdapter::GetInstance().RemoveWindow(info.widget_id);

  impl->OnSurfaceDestroyed();
}

void OnBlurEventCB(OH_NativeXComponent* component, void* window) {
  XComponentInfo info;
  if (!XComponentUtils::GetRenderInfo(component, info)) {
    return;
  }
  auto impl = XComponentUtils::GetXComponent(info.widget_id);
  if (!impl) {
    return;
  }
  auto event = std::make_shared<SurfaceEvent>(EventType::ET_SURFACE_BLUR);
  WindowAdapter::GetInstance().NotifyWindowEvent(info.widget_id, event);

  impl->OnBlurEvent();
}

void OnFocusEventCB(OH_NativeXComponent* component, void* window) {
  XComponentInfo info;
  if (!XComponentUtils::GetRenderInfo(component, info)) {
    return;
  }
  auto impl = XComponentUtils::GetXComponent(info.widget_id);
  if (!impl) {
    return;
  }
  auto event = std::make_shared<SurfaceEvent>(EventType::ET_SURFACE_FOCUS);
  WindowAdapter::GetInstance().NotifyWindowEvent(info.widget_id, event);

  impl->OnFocusEvent();
}

void OnTouchEventCB(OH_NativeXComponent* component, void* window) {
  XComponentInfo info;
  if (!XComponentUtils::GetRenderInfo(component, info)) {
    return;
  }
  auto impl = XComponentUtils::GetXComponent(info.widget_id);
  if (!impl) {
    return;
  }
  OH_NativeXComponent_TouchEvent touch_event;
  int32_t ret =
      OH_NativeXComponent_GetTouchEvent(component, window, &touch_event);
  if (ret != OH_NATIVEXCOMPONENT_RESULT_SUCCESS) {
    LOGE("Get xcomponent touch event fail. widget_id:%{public}d", info.widget_id);
    return;
  }

  OH_NativeXComponent_TouchPointToolType tool_type =
      OH_NativeXComponent_TouchPointToolType::
          OH_NATIVEXCOMPONENT_TOOL_TYPE_UNKNOWN;
  OH_NativeXComponent_GetTouchPointToolType(component, 0, &tool_type);
  if (tool_type == OH_NATIVEXCOMPONENT_TOOL_TYPE_UNKNOWN) {
    // skip unknown tool type which will cause double click in pc
    return;
  }
  float tilt_x = 0.0f;
  float tilt_y = 0.0f;
  OH_NativeXComponent_GetTouchPointTiltX(component, 0, &tilt_x);
  OH_NativeXComponent_GetTouchPointTiltY(component, 0, &tilt_y);

  float display_x = 0.0f;
  float display_y = 0.0f;
  OH_NativeXComponent_GetTouchPointDisplayX(component, 0, &display_x);
  OH_NativeXComponent_GetTouchPointDisplayY(component, 0, &display_y);
  TouchPointCoordinate coordinate;
  coordinate.tilt_x = tilt_x;
  coordinate.tilt_y = tilt_y;
  coordinate.display_x = display_x;
  coordinate.display_y = display_y;

  impl->OnTouchEvent(touch_event, tool_type, std::move(coordinate));
}

void OnMouseEventCB(OH_NativeXComponent* component, void* window) {
  XComponentInfo info;
  if (!XComponentUtils::GetRenderInfo(component, info)) {
    return;
  }
  auto impl = XComponentUtils::GetXComponent(info.widget_id);
  if (!impl) {
    return;
  }
  OH_NativeXComponent_MouseEvent mouse_event;
  int32_t ret =
      OH_NativeXComponent_GetMouseEvent(component, window, &mouse_event);
  if (ret != OH_NATIVEXCOMPONENT_RESULT_SUCCESS) {
    LOGE("Get xcomponent mouse event fail. widget_id:%{public}d", info.widget_id);
    return;
  }

  impl->OnMouseEvent(mouse_event);
}

void OnKeyEventCB(OH_NativeXComponent* component, void* window) {
  XComponentInfo info;
  if (!XComponentUtils::GetRenderInfo(component, info)) {
    return;
  }
  auto impl = XComponentUtils::GetXComponent(info.widget_id);
  if (!impl) {
    return;
  }
  OH_NativeXComponent_KeyEvent* key_event;
  int32_t ret = OH_NativeXComponent_GetKeyEvent(component, &key_event);
  if (ret != OH_NATIVEXCOMPONENT_RESULT_SUCCESS) {
    LOGE("Get xcomponent key event fail. widget_id:%{public}d", info.widget_id);
    return;
  }

  impl->OnKeyEvent(*key_event);
}

void OnHoverEventCB(OH_NativeXComponent* component, bool is_hover) {
  XComponentInfo info;
  if (!XComponentUtils::GetRenderInfo(component, info)) {
    return;
  }
  auto impl = XComponentUtils::GetXComponent(info.widget_id);
  if (!impl) {
    return;
  }
  impl->OnHoverEvent(is_hover);
}

XComponentImpl::XComponentImpl(const XComponentInfo& info) {
  this->type_ = info.type;
  this->widget_id_ = info.widget_id;
  this->owner_pid_ = info.pid;

  surface_callback_ = {
    .OnSurfaceCreated = OnSurfaceCreatedCB,
    .OnSurfaceChanged = OnSurfaceChangedCB,
    .OnSurfaceDestroyed = OnSurfaceDestroyedCB,
    .DispatchTouchEvent = OnTouchEventCB
  };

  mouse_callback_ = {
    .DispatchMouseEvent = OnMouseEventCB,
    .DispatchHoverEvent = OnHoverEventCB
  };
}

XComponentImpl::~XComponentImpl() {
  LOGI("~XComponentImpl destructive XComponentImpl instance.");
  if (instance_) {
    instance_ = nullptr;
  }
}

void XComponentImpl::Initialize(OH_NativeXComponent* component,
                                XComponentDelegate* delegate,
                                std::shared_ptr<InputEventCallBack> callback) {
  LOGI("XComponentImpl initialize instance.");
  if (instance_ != nullptr &&
      instance_ != component) {
    LOGW("XComponentImpl RegisterCallback have already set xcomponent!");
  }
  instance_ = component;
  delegate_ = delegate;
  event_callback_ = callback;

  OH_NativeXComponent_RegisterCallback(instance_, &surface_callback_);
  OH_NativeXComponent_RegisterMouseEventCallback(instance_, &mouse_callback_);
  OH_NativeXComponent_RegisterKeyEventCallback(instance_, OnKeyEventCB);
  OH_NativeXComponent_RegisterBlurEventCallback(instance_, OnBlurEventCB);
  OH_NativeXComponent_RegisterFocusEventCallback(instance_, OnFocusEventCB);
}

void XComponentImpl::SetInitialBounds(int32_t initial_width,
                                      int32_t initial_height) {
  initial_width_ = initial_width;
  initial_height_ = initial_height;
}

__attribute__((no_sanitize("cfi", "cfi-icall")))
void XComponentImpl::EventReissueAfterRegisterInputEventCallBack() {
  if (has_hover_event_before_register_) {
    has_hover_event_before_register_ = false;
    event_callback_->mouse_hover_event_callback(GetWidget(),
                                                is_hover_before_register_);
    LOGW("XComponentImpl hover event"
         "was reissued after register event_callback_.");
  }
  is_event_reissuance_required_ = false;
}

__attribute__((no_sanitize("cfi", "cfi-icall")))
void XComponentImpl::OnPanEvent(const PanAction action,
                                const PanEvent& event) {
  TRACE_EVENT_1("XComponentImpl::OnPanEvent", "widget_id", GetWidget());
  if (event_callback_ != nullptr) {
    if (is_event_reissuance_required_) {
      EventReissueAfterRegisterInputEventCallBack();
    }
    event_callback_->pan_event_callback(action, GetWidget(), event);
  }
}

__attribute__((no_sanitize("cfi", "cfi-icall")))
void XComponentImpl::OnPinchEvent(const std::string& pinch_step,
                                  const PinchEvent& event) {
  TRACE_EVENT_1("XComponentImpl::OnPinchEvent", "widget_id", GetWidget());
  if (event_callback_ != nullptr) {
    if (is_event_reissuance_required_) {
      EventReissueAfterRegisterInputEventCallBack();
    }
    event_callback_->pinch_event_callback(pinch_step, GetWidget(), event);
  }
}

__attribute__((no_sanitize("cfi", "cfi-icall")))
void XComponentImpl::OnTouchEvent(
    const OH_NativeXComponent_TouchEvent& touch_event,
    const OH_NativeXComponent_TouchPointToolType tool_type,
    const TouchPointCoordinate& coordinate) {
  TRACE_EVENT_1("XComponentImpl::OnTouchEvent", "widget_id", GetWidget());
  if (event_callback_) {
    if (is_event_reissuance_required_) {
      EventReissueAfterRegisterInputEventCallBack();
    }
    event_callback_->touchEventCallback(GetWidget(),
                                        touch_event,
                                        tool_type,
                                        coordinate);
  }
}

__attribute__((no_sanitize("cfi", "cfi-icall")))
void XComponentImpl::OnDoubleTapEvent(const TapEvent& tap_event) {
  TRACE_EVENT_1("XComponentImpl::OnDoubleTapEvent", "widget_id", GetWidget());
  if (event_callback_) {
    if (is_event_reissuance_required_) {
      EventReissueAfterRegisterInputEventCallBack();
    }
    event_callback_->double_tap_event_callback(GetWidget(), tap_event);
  }
}

__attribute__((no_sanitize("cfi", "cfi-icall")))
void XComponentImpl::OnMouseEvent(
    const OH_NativeXComponent_MouseEvent& mouse_event) {
  if (event_callback_) {
    if (is_event_reissuance_required_) {
      EventReissueAfterRegisterInputEventCallBack();
    }
    event_callback_->mouseEventCallback(GetWidget(), mouse_event);
  }
}

__attribute__((no_sanitize("cfi", "cfi-icall")))
void XComponentImpl::OnKeyEvent(OH_NativeXComponent_KeyEvent& key_event) {
  TRACE_EVENT_1("XComponentImpl::OnKeyEvent", "widget_id", GetWidget());
  if (event_callback_) {
    if (is_event_reissuance_required_) {
      EventReissueAfterRegisterInputEventCallBack();
    }
    event_callback_->keyEventCallback(GetWidget(), key_event);
  }
}

__attribute__((no_sanitize("cfi", "cfi-icall")))
void XComponentImpl::OnHoverEvent(const bool is_hover) {
  if (event_callback_) {
    if (is_event_reissuance_required_) {
      EventReissueAfterRegisterInputEventCallBack();
    }
    event_callback_->mouse_hover_event_callback(GetWidget(), is_hover);
  } else {
    is_hover_before_register_ = is_hover;
    has_hover_event_before_register_ = true;
    is_event_reissuance_required_ = true;
  }
}

void XComponentImpl::OnBlurEvent() {
  TRACE_EVENT_1("XComponentImpl::OnBlurEvent", "widget_id", GetWidget());
  delegate_->OnActivationChanged(GetWidget(), false);
}

void XComponentImpl::OnFocusEvent() {
  TRACE_EVENT_1("XComponentImpl::OnFocusEvent", "widget_id", GetWidget());
  delegate_->OnActivationChanged(GetWidget(), true);
}

void XComponentImpl::OnSurfaceCreated() {
  TRACE_EVENT_0("OnSurfaceCreated");
  delegate_->OnWidgetAvailable(GetWidget());
}

void XComponentImpl::OnSurfaceChanged() {
  TRACE_EVENT_0("OnSurfaceChanged");
}

void XComponentImpl::OnSurfaceDestroyed() {
  TRACE_EVENT_0("OnSurfaceDestroyed");
  delegate_->OnWidgetDestroyed(GetWidget());
}

void XComponentImpl::RequestLayout() {
  auto event = std::make_shared<SurfaceEvent>(EventType::ET_SURFACE_CHANGE);
  event->width = initial_width_;
  event->height = initial_height_;
  WindowAdapter::GetInstance().NotifyWindowEvent(GetWidget(), event);
}

__attribute__((no_sanitize("cfi", "cfi-icall")))
void XComponentImpl::OnDragEnterEvent(
    const ohos::adapter::OhosDragInfo& drag_info) {
  TRACE_EVENT_1("XComponentImpl::OnDragEnterEvent", "widget_id", GetWidget());
  if (event_callback_ != nullptr) {
    if (is_event_reissuance_required_) {
      EventReissueAfterRegisterInputEventCallBack();
    }
    event_callback_->dragEnterEventCallback(GetWidget(), drag_info);
  }
}

__attribute__((no_sanitize("cfi", "cfi-icall")))
void XComponentImpl::OnDragMoveEvent(const float window_x,
                                     const float window_y) {
  TRACE_EVENT_1("XComponentImpl::OnDragMoveEvent", "widget_id", GetWidget());
  if (event_callback_ != nullptr) {
    if (is_event_reissuance_required_) {
      EventReissueAfterRegisterInputEventCallBack();
    }
    event_callback_->dragMoveEventCallback(GetWidget(), window_x, window_y);
  }
}

__attribute__((no_sanitize("cfi", "cfi-icall")))
void XComponentImpl::OnDropEvent(const ohos::adapter::OhosDragInfo& drag_info) {
  TRACE_EVENT_1("XComponentImpl::OnDropEvent", "widget_id", GetWidget());
  if (event_callback_ != nullptr) {
    if (is_event_reissuance_required_) {
      EventReissueAfterRegisterInputEventCallBack();
    }
    event_callback_->dropEventCallback(GetWidget(), drag_info);
  }
}

__attribute__((no_sanitize("cfi", "cfi-icall")))
void XComponentImpl::OnDragLeaveEvent() {
  TRACE_EVENT_1("XComponentImpl::OnDragLeaveEvent", "widget_id", GetWidget());
  if (event_callback_ != nullptr) {
    if (is_event_reissuance_required_) {
      EventReissueAfterRegisterInputEventCallBack();
    }
    event_callback_->dragLeaveEventCallback(GetWidget());
  }
}

__attribute__((no_sanitize("cfi", "cfi-icall")))
void XComponentImpl::OnDragEndEvent() {
  TRACE_EVENT_1("XComponentImpl::OnDragEndEvent", "widget_id", GetWidget());
  if (event_callback_ != nullptr) {
    if (is_event_reissuance_required_) {
      EventReissueAfterRegisterInputEventCallBack();
    }
    event_callback_->dragEndEventCallback(GetWidget());
  }
}

__attribute__((no_sanitize("cfi", "cfi-icall")))
void XComponentImpl::SendWindowMouseEventForTabDrag(
    Input_MouseEvent* window_mouse_event) {
  if (event_callback_ != nullptr && window_mouse_event != nullptr) {
    event_callback_->sendWindowMouseEventForTabDragCallback(GetWidget(),
                                                            window_mouse_event);
  }
}

}  // namespace ohos::adapter::xcomponent

// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_XCOMPONENT_EVENT_INPUT_EVENT_COMMON_H_
#define OHOS_ADAPTER_XCOMPONENT_EVENT_INPUT_EVENT_COMMON_H_

#include <ace/xcomponent/native_interface_xcomponent.h>
#include <multimodalinput/oh_input_manager.h>

#include "ohos/adapter/drag_drop/drag_drop_ohos_adapter.h"
#include "ohos/adapter/export.h"
#include "ohos/adapter/xcomponent/event/gesture_event_common.h"

namespace ohos::adapter::xcomponent {

struct TouchPointCoordinate {
    float tilt_x;
    float tilt_y;
    float display_x;
    float display_y;
};

struct ADAPTER_EXPORT_API InputEventCallBack {
 public:
  void (*mouseEventCallback)(const int32_t widget_id,
                             const OH_NativeXComponent_MouseEvent& mouse_event);
  void (*keyEventCallback)(const int32_t widget_id,
                           OH_NativeXComponent_KeyEvent& key_event);
  void (*touchEventCallback)(const int32_t widget_id,
                             const OH_NativeXComponent_TouchEvent& touch_event,
                             const OH_NativeXComponent_TouchPointToolType tool_type,
                             const TouchPointCoordinate& coordinate);
  void (*pan_event_callback)(const PanAction action,
                             const int32_t widget_id,
                             const PanEvent& event);
  void (*pinch_event_callback)(const std::string& pinch_step,
                               const int32_t widget_id,
                               const PinchEvent& event);
  void (*double_tap_event_callback)(const int32_t widget_id,
                                 const TapEvent& tap_event);
  void (*dragEnterEventCallback)(const int32_t widget_id,
                                 const OhosDragInfo& drag_info);
  void (*dragLeaveEventCallback)(const int32_t widget_id);
  void (*dropEventCallback)(const int32_t widget_id,
                            const OhosDragInfo& drag_info);
  void (*dragEndEventCallback)(const int32_t widget_id);
  void (*dragMoveEventCallback)(const int32_t widget_id,
                                const float window_x,
                                const float window_y);
  void (*mouse_hover_event_callback)(const int32_t widget_id,
                                     const bool is_hover);
  void (*sendWindowMouseEventForTabDragCallback)(
                            const int32_t widget_id,
                            Input_MouseEvent* window_mouse_event);
};

}  // namespace ohos::adapter::xcomponent

#endif  // OHOS_ADAPTER_XCOMPONENT_EVENT_INPUT_EVENT_COMMON_H_

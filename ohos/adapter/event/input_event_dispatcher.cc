// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/event/input_event_dispatcher.h"

#include <stdint.h>
#include <unistd.h>

#include <ace/xcomponent/native_interface_xcomponent.h>
#include <ace/xcomponent/native_xcomponent_key_event.h>
#include <multimodalinput/oh_input_manager.h>

#include "ohos/adapter/common/logging.h"
#include "ohos/adapter/drag_drop/drag_drop_ohos_adapter.h"
#include "ohos/adapter/event/event_manager.h"
#include "ohos/adapter/xcomponent/adapter/window_adapter.h"

namespace ohos::adapter::event {

using namespace ohos::adapter::xcomponent;

void ConvertToInputEventUnion(input_event& event,
                              const input_event_type type,
                              const int32_t widget_id,
                              const OH_NativeXComponent_MouseEvent& mouse_event) {
    event.type = type;
    event.widget_id = widget_id;
    event.data.mouse_event.x = mouse_event.x;
    event.data.mouse_event.y = mouse_event.y;
    event.data.mouse_event.screen_x = mouse_event.screenX;
    event.data.mouse_event.screen_y = mouse_event.screenY;
    event.data.mouse_event.timestamp = mouse_event.timestamp;
    event.data.mouse_event.action = static_cast<int>(mouse_event.action);
    event.data.mouse_event.button = static_cast<int>(mouse_event.button);
}

void ConvertToInputEventUnion(input_event& event,
                              const input_event_type type,
                              const int32_t widget_id,
                              const PanEvent& pan_event)
{
    event.type = type;
    event.widget_id = widget_id;
    event.data.mouse_event.x = 0;
    event.data.mouse_event.y = 0;
    event.data.mouse_event.screen_x = 0;
    event.data.mouse_event.screen_y = 0;
    event.data.mouse_event.timestamp = 0;
    event.data.mouse_event.action = OHOS_MOUSE_ACTION_SCROLL;
    event.data.mouse_event.button = 0;
    event.data.mouse_event.wheel_x = pan_event.offset_x;
    event.data.mouse_event.wheel_y = pan_event.offset_y;
    event.data.mouse_event.source_tool = static_cast<int>(pan_event.source_tool);
}

void ConvertToInputEventUnion(input_event& event,
                              const input_event_type type,
                              const int32_t widget_id,
                              OH_NativeXComponent_KeyEvent& key_event)
{
    event.type = type;
    event.widget_id = widget_id;

    OH_NativeXComponent_KeyAction action = OH_NativeXComponent_KeyAction::OH_NATIVEXCOMPONENT_KEY_ACTION_UNKNOWN;
    OH_NativeXComponent_GetKeyEventAction(&key_event, &action);
    event.data.key_event.action = static_cast<int>(action);

    OH_NativeXComponent_KeyCode code = OH_NativeXComponent_KeyCode::KEY_UNKNOWN;
    OH_NativeXComponent_GetKeyEventCode(&key_event, &code);
    event.data.key_event.code = static_cast<int>(code);

    OH_NativeXComponent_EventSourceType source_type =
        OH_NativeXComponent_EventSourceType::OH_NATIVEXCOMPONENT_SOURCE_TYPE_UNKNOWN;
    OH_NativeXComponent_GetKeyEventSourceType(&key_event, &source_type);
    event.data.key_event.source_type = static_cast<int>(source_type);

    int64_t device_id = 0;
    OH_NativeXComponent_GetKeyEventDeviceId(&key_event, &device_id);
    event.data.key_event.device_id = device_id;

    int64_t timestamp = 0;
    OH_NativeXComponent_GetKeyEventTimestamp(&key_event, &timestamp);
    event.data.key_event.timestamp = timestamp;

    uint64_t modifier_key_states = 0;
    OH_NativeXComponent_GetKeyEventModifierKeyStates(&key_event, &modifier_key_states);
    event.data.key_event.modifier_key_states = modifier_key_states;

    bool is_num_lockon = false;
    OH_NativeXComponent_GetKeyEventNumLockState(&key_event, &is_num_lockon);
    event.data.key_event.is_num_lockon = is_num_lockon;

    bool is_caps_lockon = false;
    OH_NativeXComponent_GetKeyEventCapsLockState(&key_event, &is_caps_lockon);
    event.data.key_event.is_caps_lockon = is_caps_lockon;

    bool is_scroll_lockon = false;
    OH_NativeXComponent_GetKeyEventScrollLockState(&key_event, &is_scroll_lockon);
    event.data.key_event.is_scroll_lockon = is_scroll_lockon;
}

int ConverToMouseAction(const OH_NativeXComponent_TouchEventType type)
{
    switch(type) {
        case OH_NATIVEXCOMPONENT_DOWN:
            return static_cast<int32_t>(OH_NATIVEXCOMPONENT_MOUSE_PRESS);
        case OH_NATIVEXCOMPONENT_UP:
            return static_cast<int32_t>(OH_NATIVEXCOMPONENT_MOUSE_RELEASE);
        case OH_NATIVEXCOMPONENT_MOVE:
            return static_cast<int32_t>(OH_NATIVEXCOMPONENT_MOUSE_MOVE);
        case OH_NATIVEXCOMPONENT_CANCEL:
            return static_cast<int32_t>(OH_NATIVEXCOMPONENT_MOUSE_CANCEL);
        default:
            return static_cast<int32_t>(OH_NATIVEXCOMPONENT_MOUSE_NONE);
    }
}

void ConvertToInputEventUnion(input_event& event,
                              const input_event_type type,
                              const int32_t widget_id,
                              const OH_NativeXComponent_TouchEvent& touch_event,
                              const TouchPointCoordinate& coordinate)
{
    event.type = type;
    event.widget_id = widget_id;
    event.data.mouse_event.x = 0;
    event.data.mouse_event.y = 0;
    event.data.mouse_event.screen_x = coordinate.display_x;
    event.data.mouse_event.screen_y = coordinate.display_y;
    event.data.mouse_event.timestamp = touch_event.timeStamp;
    event.data.mouse_event.action = ConverToMouseAction(touch_event.type);
    event.data.mouse_event.button = static_cast<int>(OH_NATIVEXCOMPONENT_LEFT_BUTTON);
}

static void MouseEventCallback(const int32_t widget_id,
                               const OH_NativeXComponent_MouseEvent& mouse_event)
{
    if (widget_id <= 0) {
        LOGE("invalid widget_id");
        return;
    }
    if (mouse_event.action == OH_NATIVEXCOMPONENT_MOUSE_NONE) {
        LOGW("unknown action!");
        return;
    }

    input_event event;
    ConvertToInputEventUnion(event, OHOS_MOUSE_EVENT, widget_id, mouse_event);
    EventManager::GetInstance().InputDispatcher().ExecuteCallback(event);
}

static void MouseHoverEventCallback(const int32_t widget_id, const bool is_hover)
{

}

static void KeyEventCallback(const int32_t widget_id, OH_NativeXComponent_KeyEvent& key_event)
{
    if (widget_id <= 0) {
        LOGE("invalid widget_id");
        return;
    }
    input_event event;
    ConvertToInputEventUnion(event, OHOS_KEY_EVENT, widget_id, key_event);
    if (event.data.key_event.action == OH_NATIVEXCOMPONENT_KEY_ACTION_UNKNOWN) {
        LOGW("unknown action!");
        return;
    }
    EventManager::GetInstance().InputDispatcher().ExecuteCallback(event);
}

static void TouchEventCallback(const int32_t widget_id,
                               const OH_NativeXComponent_TouchEvent& touch_event,
                               const OH_NativeXComponent_TouchPointToolType touch_point_tool_type,
                               const TouchPointCoordinate& coordinate)
{
    if (widget_id <= 0) {
        LOGE("invalid widget_id");
        return;
    }
    input_event event;
    ConvertToInputEventUnion(event, OHOS_MOUSE_EVENT, widget_id, touch_event, coordinate);
    EventManager::GetInstance().InputDispatcher().ExecuteCallback(event);
}

static void PanEventCallback(const PanAction action,
                             const int32_t widget_id,
                             const PanEvent& pan_event)
{
    if (widget_id <= 0) {
        LOGW("invalid widget_id");
        return;
    }
    if (action != PanAction::kUpdate) {
        return;
    }

    input_event event;
    ConvertToInputEventUnion(event, OHOS_MOUSE_EVENT, widget_id, pan_event);
    EventManager::GetInstance().InputDispatcher().ExecuteCallback(event);
}

static void PinchEventCallback(const std::string& pinch_step,
                               const int32_t widget_id,
                               const PinchEvent& pinch_event)
{

}

static void DoubleTapEventCallback(const int32_t widget_id, const TapEvent& event)
{

}

static void DragEnterEventCallback(const int32_t widget_id,
                                   const OhosDragInfo& drag_info)
{

}

static void DragLeaveEventCallback(const int32_t widget_id)
{

}

static void DropEventCallback(const int32_t widget_id,
                              const OhosDragInfo& drag_info)
{

}

static void DragEndEventCallback(const int32_t widget_id)
{

}

static void DragMoveEventCallback(const int32_t widget_id,
                                  const float window_x,
                                  const float window_y)
{

}

static void SendWindowMouseEventForTabDragCallback(const int32_t widget_id,
                                                   Input_MouseEvent* mouse_event)
{

}

InputEventDispatcher::InputEventDispatcher()
{
    pid_ = getpid();
    xcomponent_callback_ = std::make_shared<InputEventCallBack>();
    xcomponent_callback_->mouseEventCallback = &MouseEventCallback;
    xcomponent_callback_->keyEventCallback = &KeyEventCallback;
    xcomponent_callback_->touchEventCallback = &TouchEventCallback;
    xcomponent_callback_->pan_event_callback = &PanEventCallback;
    xcomponent_callback_->pinch_event_callback = &PinchEventCallback;
    xcomponent_callback_->double_tap_event_callback = &DoubleTapEventCallback;
    xcomponent_callback_->dragEnterEventCallback = &DragEnterEventCallback;
    xcomponent_callback_->dragLeaveEventCallback = &DragLeaveEventCallback;
    xcomponent_callback_->dragMoveEventCallback = &DragMoveEventCallback;
    xcomponent_callback_->dropEventCallback = &DropEventCallback;
    xcomponent_callback_->dragEndEventCallback = &DragEndEventCallback;
    xcomponent_callback_->mouse_hover_event_callback = &MouseHoverEventCallback;
    xcomponent_callback_->sendWindowMouseEventForTabDragCallback = &SendWindowMouseEventForTabDragCallback;
}

void InputEventDispatcher::RegisterCallBack(input_event_callback callback)
{
    callback_ = callback;
}

void InputEventDispatcher::Dispatch(const input_event& event)
{
    if (callback_ == nullptr) {
        return;
    }
    callback_(&event);
}

void InputEventDispatcher::ExecuteCallback(const input_event& event)
{
    int owner_pid = WindowAdapter::GetInstance().GetOwnerPid(event.widget_id);
    if (owner_pid <= 0) {
        LOGE("not found owner_pid, widget_id: %{public}d", event.widget_id);
        return;
    }
    Dispatch(event);
}

}  // namespace ohos::adapter::event
// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/xcomponent/event/gesture_event_common.h"

#include <js_native_api.h>
#include <js_native_api_types.h>
#include <string>

#include "ohos/adapter/aki_hook/aki_hook.h"
#include "ohos/adapter/common/logging.h"
#include "ohos/adapter/common/trace.h"
#include "ohos/adapter/xcomponent/xcomponent_manager.h"

namespace ohos::adapter::xcomponent {

constexpr int32_t kDoubleTap = 2;

void OnPanEventCB(const int action_type,
                  int32_t widget_id,
                  const aki::Value gesture_event) {
  TRACE_EVENT_0("OnPanEventCB");
  PanEvent event;
  if (!(gesture_event.IsUndefined() || gesture_event.IsNull())) {
    event.offset_x = gesture_event["offsetX"].As<float>();
    event.offset_y = gesture_event["offsetY"].As<float>();
    event.velocity_x =
        static_cast<float>(gesture_event["velocityX"].As<double>());
    event.velocity_y =
        static_cast<float>(gesture_event["velocityY"].As<double>());
    event.source_tool =
        gesture_event["sourceTool"].As<GestureEventSourceTool>();
  }
  if (event.source_tool == GestureEventSourceTool::kUnknown) {
    // skip unknow source tool which trigger by long pressing and moving mouse,
    // because it will conflict with scroll bar action
    return;
  }
  auto render = XComponentManager::GetInstance()->GetXComponent(widget_id);
  if (render == nullptr) {
    LOGE("render: %{public}d not found", widget_id);
    return;
  }
  PanAction action = static_cast<PanAction>(action_type);
  render->OnPanEvent(action, event);
}

void OnPinchEventCB(const std::string& pinch_step,
                    int32_t widget_id,
                    const aki::Value gesture_event) {
  TRACE_EVENT_0("OnPinchEventCB");
  GestureEventSourceType source_type =
      gesture_event["source"].As<GestureEventSourceType>();
  if (source_type == GestureEventSourceType::kTouchScreen) {
    // pinch gesture should be skipped on touch screen, because it will be
    // detected by multi touch gesture.
    return;
  }

  PinchEvent pinch_event;
  pinch_event.offset_x = gesture_event["pinchCenterX"].As<float>();
  pinch_event.offset_y = gesture_event["pinchCenterY"].As<float>();
  pinch_event.scale = gesture_event["scale"].As<float>();

  auto render = XComponentManager::GetInstance()->GetXComponent(widget_id);
  if (render == nullptr) {
    LOGE("render: %{public}d not found", widget_id);
    return;
  }
  render->OnPinchEvent(pinch_step, pinch_event);
}

void OnDoubleTapCB(int32_t widget_id,
                   float offset_x,
                   float offset_y) {
  TRACE_EVENT_0("OnDoubleTapCB");

  TapEvent tap_event;
  tap_event.offset_x = offset_x;
  tap_event.offset_y = offset_y;
  tap_event.tap_count = kDoubleTap;

  auto render = XComponentManager::GetInstance()->GetXComponent(widget_id);
  if (render == nullptr) {
    LOGE("render: %{public}d not found", widget_id);
    return;
  }
  render->OnDoubleTapEvent(tap_event);
}

JSBIND_GLOBAL() {
  JSBIND_FUNCTION(OnPanEventCB);
  JSBIND_FUNCTION(OnPinchEventCB);
  JSBIND_FUNCTION(OnDoubleTapCB);
}

}  // namespace ohos::adapter::xcomponent

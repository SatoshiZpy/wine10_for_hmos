// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_XCOMPONENT_EVENT_GESTURE_EVENT_COMMON_H_
#define OHOS_ADAPTER_XCOMPONENT_EVENT_GESTURE_EVENT_COMMON_H_

#include <cstdint>

namespace ohos::adapter::xcomponent {

enum class GestureEventSourceType : int32_t {
  kUnknown,
  kMouse,
  kTouchScreen,
};

enum class GestureEventSourceTool : int32_t {
  kUnknown,
  kFinger,
  kPen,
  kRubber,
  kBrush,
  kPencil,
  kAirBrush,
  kMouse,
  kLens,
  kTouchpad,
};

enum class PanAction {
  kStart = 0,
  kUpdate,
  kEnd,
  kCancel,
};

struct BaseGestureEvent {
  GestureEventSourceTool source_tool{GestureEventSourceTool::kUnknown};
};

struct PanEvent : public BaseGestureEvent {
  float offset_x{0.0f};
  float offset_y{0.0f};
  float velocity_x{0.0f};
  float velocity_y{0.0f};
};

struct PinchEvent : public BaseGestureEvent {
  float offset_x{0.0f};
  float offset_y{0.0f};
  float scale{1.0f};
};

struct TapEvent : public BaseGestureEvent {
  float offset_x{0.0f};
  float offset_y{0.0f};
  int tap_count{0};
};

}  // namespace ohos::adapter::xcomponent

#endif  // OHOS_ADAPTER_XCOMPONENT_EVENT_GESTURE_EVENT_COMMON_H_

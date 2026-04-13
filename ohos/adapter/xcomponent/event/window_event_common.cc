// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/xcomponent/event/window_event_common.h"

#include <iostream>
#include <sstream>
#include <string>

namespace ohos::adapter::xcomponent {
std::string WithEnumValue(int value) {
  std::string tmp = "(";
  tmp += std::to_string(value);
  tmp += ")";
  return tmp;
}

std::string Event::GetName() const {
  return EventTypeName(type_);
}

std::string EventTypeName(EventType type) {
  switch (type) {
#define CASE_TYPE(t) \
    case t:            \
      return #t

      CASE_TYPE(ET_UNKNOWN);
      CASE_TYPE(ET_SURFACE_CHANGE);
      CASE_TYPE(ET_SURFACE_FOCUS);
      CASE_TYPE(ET_SURFACE_BLUR);
      CASE_TYPE(ET_WINDOW_SIZE_CHANGE);
      CASE_TYPE(ET_WINDOW_CHANGE);
      CASE_TYPE(ET_WINDOW_RECT_CHANGE);
      CASE_TYPE(ET_WINDOW_STATUS_CHANGE);
      CASE_TYPE(ET_WINDOW_CAPTION_BUTTON_RECT_CHANGE);
      CASE_TYPE(ET_DEVICE_MODE_CHANGED);

#undef CASE_TYPE
      default:
        return "";
  }
}

std::string WindowEventToString(WindowEventType eventType) {
  std::string name;
  switch (eventType) {
    case WindowEventType::WINDOW_SHOWN:
      name = "WINDOW_SHOWN";
      break;
    case WindowEventType::WINDOW_ACTIVE:
      name = "WINDOW_ACTIVE";
      break;
    case WindowEventType::WINDOW_INACTIVE:
      name = "WINDOW_INACTIVE";
      break;
    case WindowEventType::WINDOW_HIDDEN:
      name = "WINDOW_HIDDEN";
      break;
    case WindowEventType::WINDOW_OCCLUDED:
      name = "WINDOW_OCCLUDED";
      break;
    case WindowEventType::WINDOW_VISIBLE:
      name = "WINDOW_VISIBLE";
      break;
    case WindowEventType::WINDOW_DESTROYED:
      name = "WINDOW_DESTROYED";
      break;
    case WindowEventType::WINDOW_CLOSE:
      name = "WINDOW_CLOSE";
      break;
    default:
      name = "UNKNOWN_EVENT";
  }
  return name + WithEnumValue(static_cast<int>(eventType));
}

std::string WindowStatusToString(WindowStatusType status) {
  std::string name;
  switch (status) {
    case WindowStatusType::UNDEFINED:
      name = "UNDEFINED";
      break;
    case WindowStatusType::FULL_SCREEN:
      name = "FULL_SCREEN";
      break;
    case WindowStatusType::MAXIMIZE:
      name = "MAXIMIZE";
      break;
    case WindowStatusType::MINIMIZE:
      name = "MINIMIZE";
      break;
    case WindowStatusType::FLOATING:
      name = "FLOATING";
      break;
    case WindowStatusType::SPLIT_SCREEN:
      name = "SPLIT_SCREEN";
      break;
    default:
      name = "UNKNOWN_STATUS";
  }
  return name + WithEnumValue(static_cast<int>(status));
}

std::string RectChangeReasonToString(RectChangeReason reason) {
  std::string name;
  switch (reason) {
    case RectChangeReason::UNDEFINED:
      name = "UNDEFINED";
      break;
    case RectChangeReason::MAXIMIZE:
      name = "MAXIMIZE";
      break;
    case RectChangeReason::RECOVER:
      name = "RECOVER";
      break;
    case RectChangeReason::MOVE:
      name = "MOVE";
      break;
    case RectChangeReason::DRAG:
      name = "DRAG";
      break;
    case RectChangeReason::DRAG_START:
      name = "DRAG_START";
      break;
    case RectChangeReason::DRAG_END:
      name = "DRAG_END";
      break;
    default:
      name = "UNKNOWN_REASON";
  }
  return name + WithEnumValue(static_cast<int>(reason));
}

std::string ChangeEventToString(ChangeEventType changeType) {
  std::string name;
  switch (changeType) {
    case ChangeEventType::CHANGE_TO_NORMAL_MODE:
      name = "CHANGE_TO_NORMAL_MODE";
      break;
    case ChangeEventType::CHANGE_TO_FREE_MODE:
      name = "CHANGE_TO_FREE_MODE";
      break;
    default:
      name = "UNKNOWN_EVENT";
  }
  return name + WithEnumValue(static_cast<int>(changeType));
}

std::string Event::ToString() {
  std::ostringstream oss;
  oss << "Event(type: " << GetName() << ")";
  return oss.str();
}

std::string SurfaceEvent::ToString() {
  std::ostringstream oss;
  oss << GetName() << "(size: (" << width << "X" << height << "))";
  return oss.str();
}

std::string WindowRectChangeEvent::ToString() {
  std::ostringstream oss;
  oss << GetName() << "(reason: " << RectChangeReasonToString(reason) <<
      ", pos: (" << left << ", " << top << "), size: (" <<
      width << "X" << height << "))";
  return oss.str();
}

std::string WindowStatusChangeEvent::ToString() {
  std::ostringstream oss;
  oss << GetName() << "(status: " << WindowStatusToString(status) << ")";
  return oss.str();
}

std::string WindowSizeChangeEvent::ToString() {
  std::ostringstream oss;
  oss << GetName() << "(pos: (" << left << ", " << top <<
      "), size: (" << width << "X" << height << "))";
  return oss.str();
}

std::string WindowChangeEvent::ToString() {
  std::ostringstream oss;
  oss << GetName() << "(type: " << WindowEventToString(window_event_type_) << ")";
  return oss.str();
}

std::string WindowCaptionButtonRectChangeEvent::ToString() {
  std::ostringstream oss;
  oss << GetName() <<"(pos: (" << right << ", " << top <<
      "), size: (" << width << "X" << height << "))";
  return oss.str();
}

std::string DeviceInfoChangeEvent::ToString() {
  std::ostringstream oss;
  oss << GetName() << "(mode: " << ChangeEventToString(change_event_type_) << ")";
  return oss.str();
}
}  // namespace ohos::adapter::xcomponent

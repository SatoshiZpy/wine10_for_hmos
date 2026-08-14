// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_XCOMPONENT_EVENT_WINDOW_EVENT_COMMON_H_
#define OHOS_ADAPTER_XCOMPONENT_EVENT_WINDOW_EVENT_COMMON_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "ohos/adapter/export.h"

namespace ohos::adapter::xcomponent {

enum EventType : int32_t {
  ET_UNKNOWN = -1,
  ET_SURFACE_CHANGE = 0,
  ET_SURFACE_FOCUS = 1,
  ET_SURFACE_BLUR = 2,
  ET_WINDOW_SIZE_CHANGE = 3,
  ET_WINDOW_CHANGE = 4,
  ET_WINDOW_RECT_CHANGE = 5,
  ET_WINDOW_STATUS_CHANGE = 6,
  ET_WINDOW_CAPTION_BUTTON_RECT_CHANGE = 7,
  ET_DEVICE_MODE_CHANGED = 8,
  ET_WINDOW_DISPLAY_ID_CHANGE = 9
};

enum class WindowEventType {
  WINDOW_SHOWN = 1,
  WINDOW_ACTIVE = 2,
  WINDOW_INACTIVE = 3,
  WINDOW_HIDDEN = 4,
  WINDOW_OCCLUDED = 5,
  WINDOW_VISIBLE = 6,
  WINDOW_DESTROYED = 7,
  WINDOW_CLOSE= 1000
};

enum class WindowStatusType {
  UNDEFINED = 0,
  FULL_SCREEN,
  MAXIMIZE,
  MINIMIZE,
  FLOATING,
  SPLIT_SCREEN
};

enum class RectChangeReason {
  UNDEFINED = 0,
  MAXIMIZE,
  RECOVER,
  MOVE,
  DRAG,
  DRAG_START,
  DRAG_END
};

enum class ChangeEventType {
  CHANGE_TO_NORMAL_MODE = 0,
  CHANGE_TO_FREE_MODE
};

class ADAPTER_EXPORT_API Event {
 public:
  explicit Event(EventType type) : type_(type) {}
  virtual ~Event() {}
  virtual EventType type() { return type_; }
  virtual std::string ToString();
  std::string GetName() const;

 private:
  EventType type_;
};

class ADAPTER_EXPORT_API SurfaceEvent : public Event {
 public:
  explicit SurfaceEvent(EventType type)
    : Event(type) {}
  std::string ToString() override;
  int width = 0;
  int height = 0;
};

class ADAPTER_EXPORT_API WindowRectChangeEvent : public Event {
 public:
  explicit WindowRectChangeEvent()
    : Event(EventType::ET_WINDOW_RECT_CHANGE) {}
  std::string ToString() override;
  int top = 0;
  int left = 0;
  int width = 0;
  int height = 0;
  RectChangeReason reason = RectChangeReason::UNDEFINED;
};

class ADAPTER_EXPORT_API WindowStatusChangeEvent : public Event {
 public:
  explicit WindowStatusChangeEvent()
    : Event(EventType::ET_WINDOW_STATUS_CHANGE) {}
  std::string ToString() override;
  WindowStatusType status;
};

class ADAPTER_EXPORT_API WindowSizeChangeEvent : public Event {
 public:
  explicit WindowSizeChangeEvent()
    : Event(EventType::ET_WINDOW_SIZE_CHANGE) {}
  std::string ToString() override;
  int top = 0;
  int left = 0;
  int width = 0;
  int height = 0;
};

class ADAPTER_EXPORT_API WindowEvent : public Event {
 public:
  explicit WindowEvent(WindowEventType type)
    : Event(EventType::ET_WINDOW_CHANGE), window_event_type_(type) {}
  std::string ToString() override;
  const WindowEventType window_event_type_;
};

class ADAPTER_EXPORT_API WindowCaptionButtonRectChangeEvent : public Event {
 public:
  explicit WindowCaptionButtonRectChangeEvent()
    : Event(EventType::ET_WINDOW_CAPTION_BUTTON_RECT_CHANGE) {}
  std::string ToString() override;
  int top = 0;
  int right = 0;
  int width = 0;
  int height = 0;
};

class ADAPTER_EXPORT_API DeviceInfoChangeEvent : public Event {
 public:
  explicit DeviceInfoChangeEvent()
    : Event(EventType::ET_DEVICE_MODE_CHANGED) {}
  std::string ToString() override;
  ChangeEventType change_event_type_;
};

class ADAPTER_EXPORT_API WindowDisplayIdChangeEvent : public event {
 public:
  explicit WindowDisplayIdChangeEvent()
    : Event(EventType::ET_WINDOW_DISPLAY_ID_CHANGE) {}
  std::string ToString() override;
  int display_id = 0;
};


std::string EventTypeName(EventType type);
std::string WindowEventToString(WindowEventType eventType);
std::string WindowStatusToString(WindowStatusType status);
std::string RectChangeReasonToString(RectChangeReason reason);
}  // namespace ohos::adapter::xcomponent

using WindowEventCallBack =
  std::function<void(int32_t, std::shared_ptr<ohos::adapter::xcomponent::Event>)>;

#endif  // OHOS_ADAPTER_XCOMPONENT_EVENT_WINDOW_EVENT_COMMON_H_

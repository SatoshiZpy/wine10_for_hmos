// Copyright (c) 2025 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/xcomponent/event/window_event_filter_adapter.h"

#include <dlfcn.h>

#include "ohos/adapter/aki_hook/aki_hook.h"
#include "ohos/adapter/device_info/device_info.h"
#include "ohos/adapter/window/adapter/app_window_adapter.h"
#include "ohos/adapter/xcomponent/xcomponent_manager.h"

namespace ohos::adapter::window {

WindowEventFilterAdapter& WindowEventFilterAdapter::GetInstance() {
  static WindowEventFilterAdapter instance;
  return instance;
}
WindowEventFilterAdapter::WindowEventFilterAdapter()
    : tab_dragging_widget_id_(-1),
      window_manager_lib_("native_window_manager") {
  if (!can_filter_window_mouse_event_ &&
      device_info::DeviceInfo::SdkApi() >= device_info::SDK_VERSION_15) {
    if (!window_manager_lib_.IsLoaded()) {
      LOGE("[OhosTabDrag] native_window_manager library load fail");
      return;
    }
    if (!LoadAllEventFunctions()) {
      LOGE("[OhosTabDrag] LoadAllEventFunctions fail!");
      return;
    }
    can_filter_window_mouse_event_ = true;
  }
}

bool WindowEventFilterAdapter::LoadAllEventFunctions() {
  return window_manager_lib_.LoadFunction(&get_mouse_event_action_func_,
                                          "OH_Input_GetMouseEventAction") &&
         window_manager_lib_.LoadFunction(&get_mouse_event_display_x_func_,
                                          "OH_Input_GetMouseEventDisplayX") &&
         window_manager_lib_.LoadFunction(&get_mouse_event_display_y_func_,
                                          "OH_Input_GetMouseEventDisplayY") &&
         window_manager_lib_.LoadFunction(&get_mouse_event_button_func_,
                                          "OH_Input_GetMouseEventButton") &&
         window_manager_lib_.LoadFunction(&get_mouse_event_action_time_func_,
                                          "OH_Input_GetMouseEventActionTime") &&
         window_manager_lib_.LoadFunction(&get_mouse_event_window_id_func_,
                                          "OH_Input_GetMouseEventWindowId") &&
         window_manager_lib_.LoadFunction(&get_mouse_event_display_id_func_,
                                          "OH_Input_GetMouseEventDisplayId") &&
         window_manager_lib_.LoadFunction(
             &register_mouse_event_filter_func_,
             "OH_NativeWindowManager_RegisterMouseEventFilter") &&
         window_manager_lib_.LoadFunction(
             &un_register_mouse_event_filter_func_,
             "OH_NativeWindowManager_UnregisterMouseEventFilter");
}

WindowEventFilterAdapter::~WindowEventFilterAdapter() {
  get_mouse_event_action_func_ = nullptr;
  get_mouse_event_display_x_func_ = nullptr;
  get_mouse_event_display_y_func_ = nullptr;
  get_mouse_event_button_func_ = nullptr;
  get_mouse_event_action_time_func_ = nullptr;
  get_mouse_event_window_id_func_ = nullptr;
  register_mouse_event_filter_func_ = nullptr;
  un_register_mouse_event_filter_func_ = nullptr;
}

void WindowEventFilterAdapter::SendMouseEventForTabDrag(
    const int32_t widget_id,
    Input_MouseEvent* window_mouse_event) {
  auto render = xcomponent::XComponentManager::GetInstance()->GetXComponent(widget_id);
  if (render == nullptr) {
    LOGE("[OhosTabDrag]can not get render: %{public}d", widget_id);
    return;
  }
  render->SendWindowMouseEventForTabDrag(window_mouse_event);
}

void WindowEventFilterAdapter::SetDraggingTabWidgetId(int32_t widget_id) {
  LOGI(
      "[OhosTabDrag] WindowEventFilterAdapter::SetDraggingTabWidgetId, "
      "widget_id:%{public}d",
      widget_id);
  tab_dragging_widget_id_.store(widget_id, std::memory_order_release);
}

bool WindowEventFilterAdapter::IsTabDragging() const {
  return GetDraggingTabWidgetId() > 0;
}

int32_t WindowEventFilterAdapter::GetDraggingTabWidgetId() const {
  return tab_dragging_widget_id_.load(std::memory_order_acquire);
}

int32_t WindowEventFilterAdapter::GetTargetWindowIdAfterShiftEvent(
    int32_t origin_window_id) {
  std::shared_lock<std::shared_mutex> lock(window_event_mutext_);
  int32_t target_window_id = -1;
  if (origin_window_id == source_origin_window_id_) {
    target_window_id = source_window_id_;
  } else if (origin_window_id == target_origin_window_id_) {
    target_window_id = tartget_window_id_;
  }
  return target_window_id;
}

void WindowEventFilterAdapter::CacheShiftEventWindowIds(const int32_t source_id,
                                                        const int32_t target_id) {
  std::vector<int32_t> window_ids;
  window_ids.push_back(source_id);
  window_ids.push_back(target_id);
  std::vector<int32_t> origin_window_ids =
      AppWindowAdapter::GetInstance().GetOriginWindowIds(window_ids);
  if (origin_window_ids.size() != window_ids.size()) {
    LOGE(
        "[OhosTabDrag] WindowEventFilterAdapter::CacheShiftEventWindowIds "
        "GetOriginWindowIds fail, "
        "origin_window_ids size:%{public}lu",
        origin_window_ids.size());
    return;
  }
  LOGI(
      "[OhosTabDrag] WindowEventFilterAdapter::CacheShiftEventWindowIds "
      "origin_source-window-id:%{public}d, "
      "origin_target-window-id:%{public}d",
      origin_window_ids[0], origin_window_ids[1]);
  {
    std::unique_lock<std::shared_mutex> lock(window_event_mutext_);
    source_window_id_ = source_id;
    tartget_window_id_ = target_id;
    source_origin_window_id_ = origin_window_ids[0];
    target_origin_window_id_ = origin_window_ids[1];
  }
}

__attribute__((no_sanitize("cfi", "cfi-icall")))
static bool FilterMouseEvent(Input_MouseEvent* mouse_event) {
  WindowEventFilterAdapter& window_event_filter = WindowEventFilterAdapter::GetInstance();
  if (!window_event_filter.CanFilterWindowMouseEvent() ||
      mouse_event == nullptr) {
    return false;
  }
  // When tab dragging, the window mouse event is directly
  // sent to the UI thread of Wine.
  if (window_event_filter.IsTabDragging()) {
    int32_t origin_window_id =
        window_event_filter.GetWindowMouseEventWindowId(mouse_event);
    int32_t widget_id =
        window_event_filter.GetTargetWindowIdAfterShiftEvent(
            origin_window_id);
    if (widget_id <= 0) {
      widget_id = window_event_filter.GetDraggingTabWidgetId();
    }
    window_event_filter.SendMouseEventForTabDrag(widget_id, mouse_event);
    return true;
  }
  return false;
}

__attribute__((no_sanitize("cfi", "cfi-icall")))
void RegisterWindowEventFilter(int32_t origin_window_id) {
  WindowEventFilterAdapter& window_event_filter =
      WindowEventFilterAdapter::GetInstance();
  if (!window_event_filter.CanFilterWindowMouseEvent()) {
    LOGE(
        "[OhosTabDrag]WindowEventFilterAdapter::RegisterWindowEventFilter"
        " window_manager_lib is not loaded.");
    return;
  }
  window_event_filter.RegisterWindowEventFilterForWindow(origin_window_id);
}

__attribute__((no_sanitize("cfi", "cfi-icall")))
void ClearWindowEventFilter(int32_t origin_window_id) {
  WindowEventFilterAdapter& window_event_filter =
      WindowEventFilterAdapter::GetInstance();
  if (!window_event_filter.CanFilterWindowMouseEvent()) {
    LOGE(
        "[OhosTabDrag]WindowEventFilterAdapter::ClearWindowEventFilter "
        " window_manager_lib is not loaded.");
    return;
  }
  window_event_filter.UnRegisterWindowEventFilterForWindow(origin_window_id);
}

__attribute__((no_sanitize("cfi", "cfi-icall")))
int32_t WindowEventFilterAdapter::GetWindowMouseEventAction(
    Input_MouseEvent* window_mouse_event) {
  if (get_mouse_event_action_func_ == nullptr) {
    LOGE(
        "[OhosTabDrag]WindowEventFilterAdapter::GetWindowMouseEventAction fail");
    return 0;
  }
  return get_mouse_event_action_func_(window_mouse_event);
}

__attribute__((no_sanitize("cfi", "cfi-icall")))
int32_t WindowEventFilterAdapter::GetWindowMouseEventDisplayX(
    Input_MouseEvent* window_mouse_event) {
  if (get_mouse_event_display_x_func_ == nullptr) {
    LOGE(
        "[OhosTabDrag]WindowEventFilterAdapter::GetWindowMouseEventDisplayX "
        "fail");
    return 0;
  }
  return get_mouse_event_display_x_func_(window_mouse_event);
}

__attribute__((no_sanitize("cfi", "cfi-icall")))
int32_t WindowEventFilterAdapter::GetWindowMouseEventDisplayY(
    Input_MouseEvent* window_mouse_event) {
  if (get_mouse_event_display_y_func_ == nullptr) {
    LOGE(
        "[OhosTabDrag]WindowEventFilterAdapter::GetWindowMouseEventDisplayY fail");
    return 0;
  }
  return get_mouse_event_display_y_func_(window_mouse_event);
}

__attribute__((no_sanitize("cfi", "cfi-icall")))
int32_t WindowEventFilterAdapter::GetWindowMouseEventButton(
    Input_MouseEvent* window_mouse_event) {
  if (get_mouse_event_button_func_ == nullptr) {
    LOGE(
        "[OhosTabDrag]WindowEventFilterAdapter::GetWindowMouseEventButton fail");
    return 0;
  }
  return get_mouse_event_button_func_(window_mouse_event);
}

__attribute__((no_sanitize("cfi", "cfi-icall")))
int64_t WindowEventFilterAdapter::GetWindowMouseEventActionTime(
    Input_MouseEvent* window_mouse_event) {
  if (get_mouse_event_action_time_func_ == nullptr) {
    LOGE(
        "[OhosTabDrag]WindowEventFilterAdapter::GetWindowMouseEventActionTime fail");
    return 0;
  }
  return get_mouse_event_action_time_func_(window_mouse_event);
}

__attribute__((no_sanitize("cfi", "cfi-icall")))
int32_t WindowEventFilterAdapter::GetWindowMouseEventWindowId(
    Input_MouseEvent* window_mouse_event) {
  if (get_mouse_event_window_id_func_ == nullptr) {
    LOGE(
        "[OhosTabDrag]WindowEventFilterAdapter::GetWindowMouseEventWindowId fail");
    return 0;
  }
  return get_mouse_event_window_id_func_(window_mouse_event);
}

__attribute__((no_sanitize("cfi", "cfi-icall")))
int32_t WindowEventFilterAdapter::GetWindowMouseEventDisplayId(
    Input_MouseEvent* window_mouse_event) {
  if (get_mouse_event_display_id_func_ == nullptr) {
    LOGE(
        "[OhosTabDrag]WindowEventFilterAdapter::GetWindowMouseEventDisplayId fail");
    return -1;
  }
  return get_mouse_event_display_id_func_(window_mouse_event);
}

__attribute__((no_sanitize("cfi", "cfi-icall")))
void WindowEventFilterAdapter::RegisterWindowEventFilterForWindow(
    int32_t origin_window_id) {
  if (!register_mouse_event_filter_func_) {
    LOGW(
        "[OhosTabDrag]WindowEventFilterAdapter::RegisterWindowEventFilterForWindow "
        "register_mouse_event_filter_func_ is null");
    return;
  }
  auto result =
      register_mouse_event_filter_func_(origin_window_id, FilterMouseEvent);
  LOGI(
      "[OhosTabDrag]WindowEventFilterAdapter::RegisterWindowEventFilterForWindow, "
      "window_id:%{public}d, result:%{public}d",
      origin_window_id, result);
}
 
__attribute__((no_sanitize("cfi", "cfi-icall")))
void WindowEventFilterAdapter::UnRegisterWindowEventFilterForWindow(
    int32_t origin_window_id) {
  if (!un_register_mouse_event_filter_func_) {
    LOGW(
        "[OhosTabDrag]WindowEventFilterAdapter::UnRegisterWindowEventFilterForWindow "
        "un_register_mouse_event_filter_func_ is null");
    return;
  }
  auto result = un_register_mouse_event_filter_func_(origin_window_id);
  LOGI(
      "[OhosTabDrag]WindowEventFilterAdapter::UnRegisterWindowEventFilterForWindow, "
      "window_id:%{public}d, result:%{public}d",
      origin_window_id, result);
}

JSBIND_GLOBAL() {
  JSBIND_FUNCTION(RegisterWindowEventFilter);
  JSBIND_FUNCTION(ClearWindowEventFilter);
}
} // namespace ohos::adapter::window
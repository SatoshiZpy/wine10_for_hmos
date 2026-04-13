// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/xcomponent/xcomponent_manager.h"

#include <chrono>

#include "ohos/adapter/accessibility/accessibility_adapter.h"
#include "ohos/adapter/aki_hook/aki_hook.h"
#include "ohos/adapter/common/logging.h"
#include "ohos/adapter/common/trace.h"
#include "ohos/adapter/window/window_common.h"
#include "ohos/adapter/xcomponent/adapter/input_event_callback_adapter.h"
#include "ohos/adapter/xcomponent/adapter/window_adapter.h"
#include "ohos/adapter/xcomponent/xcomponent_utils.h"

using namespace ohos::adapter::window;

namespace ohos::adapter::xcomponent {

namespace {
WindowStyle BuilderStyle(const ipc::WindowParam& param) {
  WindowStyle styles;
  bool has_title_bar = has_feature(WindowFeatures::WINDOW_TITLE_BAR, param.features);
  styles.hide_title_bar = !has_title_bar;
  styles.close_button = has_title_bar && has_feature(WindowFeatures::WINDOW_CLOSE_BUTTON, param.features);
  styles.minimize_button = has_title_bar && has_feature(WindowFeatures::WINDOW_MINIMIZE_BUTTON, param.features);
  styles.maximize_button = has_title_bar && has_feature(WindowFeatures::WINDOW_MAXIMIZE_BUTTON, param.features);
  styles.use_floating_window = has_feature(WindowFeatures::USE_FLOATING_WINDOW, param.features);
  styles.use_dark_mode = has_feature(WindowFeatures::USE_DARK_MODE, param.features);
  styles.always_on_top = is_state(WindowState::WINDOW_TOP_MOST, param.window_state);

  bool is_enable = !is_state(WindowState::WINDOW_DISABLED, param.window_state);
  styles.resizable = is_enable && has_feature(WindowFeatures::WINDOW_RESIZABLE, param.features);
  styles.focusable = is_enable && !has_feature(WindowFeatures::WINDOW_NO_ACTIVATION, param.features);
  styles.touchable = is_enable && has_feature(WindowFeatures::WINDOW_TOUCHABLE, param.features);
  styles.show_widow = is_state(WindowState::WINDOW_VISIABLE, param.window_state);
  return styles;
}

NewWindowParam BuilderParam(const ipc::WindowParam& param) {
  NewWindowParam newParam;
  newParam.ability_type = (AbilityType)param.ability_type;
  newParam.parent_id = param.parent_id;
  newParam.widget_id = WindowAdapter::GetInstance().NextWindowWidgetId();
  newParam.bounds = {param.left,
                     param.top,
                     param.width,
                     param.height};
  newParam.feature_mask = param.features;
  newParam.window_state_mask = param.window_state;
  newParam.hwnd = param.hwnd;
  newParam.owner_pid = param.owner_pid;
  return newParam;
}
}

XComponentManager* XComponentManager::manager_ = nullptr;

XComponentManager::XComponentManager() {
  callback_ = InputEventCallbackAdapter::GetInstance().GetCallBack();
}

XComponentManager* XComponentManager::GetInstance() {
  if (manager_ == nullptr) {
    manager_ = new XComponentManager();
  }
  return manager_;
}

XComponentManager::~XComponentManager() {
  render_map_.clear();
}

std::shared_ptr<InputEventCallBack> XComponentManager::GetCallBack()
{
    return callback_;
}

void XComponentManager::Initialize(napi_env env, napi_value exports) {
  napi_value export_instance = nullptr;
  if (napi_get_named_property(env, exports, OH_NATIVE_XCOMPONENT_OBJ,
                              &export_instance) != napi_ok) {
    LOGE("XComponentManager::Initialize napi_get_named_property fail");
    return;
  }

  if (export_instance == nullptr) {
    LOGE("XComponentManager::Initialize export instance null");
    return;
  }

  OH_NativeXComponent* native_xcomponent = nullptr;
  if (napi_unwrap(env, export_instance,
                  reinterpret_cast<void**>(&native_xcomponent)) != napi_ok) {
    LOGE("XComponentManager::Initialize napi_unwrap fail");
    return;
  }

  if (native_xcomponent == nullptr) {
    LOGE("XComponentManager::Initialize native xcomponent null");
    return;
  }

  XComponentInfo info;
  if (!XComponentUtils::GetRenderInfo(native_xcomponent, info)) {
    return;
  }

  auto widget_id = info.widget_id;
  LOGI("XComponentManager::Initialize type:%{public}d, widget_id:%{public}d, owner_pid:%{public}d",
    info.type, info.widget_id, info.pid);
  auto render = CreateXComponent(widget_id, std::move(info));
  render->Initialize(native_xcomponent, this, GetCallBack());

  ohos::adapter::accessibility::AccessibilityAdapter::GetInstance().Initialize(
      native_xcomponent, info.render_id);
}

std::shared_ptr<XComponentImpl>
XComponentManager::CreateXComponent(int32_t widget_id, const XComponentInfo& info) {
  if (!render_map_.count(widget_id)) {
    render_map_[widget_id] = std::make_shared<XComponentImpl>(info);
  }
  return render_map_[widget_id];
}

std::shared_ptr<XComponentImpl>
XComponentManager::GetXComponent(int32_t widget_id) {
  if (!render_map_.count(widget_id)) {
    LOGW("widget_id: %{public}d not found", widget_id);
    return nullptr;
  }
  return render_map_[widget_id];
}

void XComponentManager::RemoveXComponent(int32_t widget_id) {
  render_map_.erase(widget_id);
  window_status_.erase(widget_id);
}

int32_t XComponentManager::CreateWindow(const ipc::WindowParam& param) {
  std::lock_guard<std::mutex> lock_protect(create_mutex_);
  int32_t widget_id = -1;
  NewWindowParam newParam = BuilderParam(param);
  TRACE_EVENT_1("XComponentManager::CreateWindow", "xcomponentid", newParam.widget_id);
  switch (param.window_type) {
    case WindowInitType::kWindow:
      widget_id = CreateMainWindow(std::move(newParam));
      break;
    case WindowInitType::kPopup:
      widget_id = CreateSubWindow(std::move(newParam));
      break;
    default:
      LOGE("unsupported window type:%{public}d", (int)param.window_type);
      break;
  }
  return widget_id;
}

void XComponentManager::CloseWindow(int32_t widget_id) {
  std::lock_guard<std::mutex> lock_protect(create_mutex_);
  auto render = GetXComponent(widget_id);
  if (!render) {
    LOGE("not found render id:%{public}d", widget_id);
    return;
  }

  bool sub_window = render->GetType() == XComponentType::kSubWindow;
  if (!sub_window) {
    AppWindowAdapter::GetInstance().Close(widget_id);
  } else {
    SubWindowAdapter::GetInstance().Close(widget_id);
  }
}

int32_t XComponentManager::CreateMainWindow(const NewWindowParam& param) {
  // no available windows, need to create
  if (reuse_window_.empty()) {
    creating_window_ = param.widget_id;
    LOGI("create window: %{public}d, rect: (%{public}d,%{public}d,%{public}d,%{public}d)",
      param.widget_id, param.bounds.left, param.bounds.top, param.bounds.width, param.bounds.height);

    window_status_.insert({param.widget_id, std::promise<bool>()});
    bool use_floating_window = has_feature(WindowFeatures::USE_FLOATING_WINDOW, param.feature_mask);
    if (use_floating_window) {
      SystemFloatingWindowAdapter::GetInstance().Create(param);
    } else {
      AppWindowAdapter::GetInstance().Create(param);
    }
    auto future = window_status_[param.widget_id].get_future();
    auto status = future.wait_for(std::chrono::seconds(3));
    if (status == std::future_status::timeout) {
      LOGE("create window: %{public}d timeout", param.widget_id);
      creating_window_ = -1;
      return -1;
    }
    LOGI("create window: %{public}d ok", param.widget_id);
    return param.widget_id;
  }

  // If arkui has already created a window, use it directly
  int32_t reuse_window_id = reuse_window_.front();
  reuse_window_.pop();
  LOGI("reuse window [%{public}d]", reuse_window_id);
  return reuse_window_id;
}

int32_t XComponentManager::CreateSubWindow(const NewWindowParam& param) {
  return SubWindowAdapter::GetInstance().Create(param);
}

void XComponentManager::RequestLayout(int32_t widget_id) {
  auto render = GetXComponent(widget_id);
  if (!render) {
    LOGE("not found render id:%{public}d", widget_id);
    return;
  }
  render->RequestLayout();
}

int32_t XComponentManager::UpdateWindow(int32_t widget_id, const ipc::WindowParam& param) {
  std::lock_guard<std::mutex> lock_protect(create_mutex_);
  TRACE_EVENT_1("XComponentManager::UpdateWindow", "xcomponentid", widget_id);
  return 0;
}

int32_t XComponentManager::SetWindowVisible(int widget_id, int visible) {
  std::lock_guard<std::mutex> lock_protect(create_mutex_);
  auto render = GetXComponent(widget_id);
  if (!render) {
    LOGE("not found render id:%{public}d", widget_id);
    return -1;
  }
  bool sub_window = render->GetType() == XComponentType::kSubWindow;
  if (!sub_window) {
    visible ? AppWindowAdapter::GetInstance().Show(widget_id) :
              AppWindowAdapter::GetInstance().Hide(widget_id);
  } else {
    visible ? SubWindowAdapter::GetInstance().Show(widget_id) :
              SubWindowAdapter::GetInstance().Hide(widget_id);
  }
  return 0;
}

void XComponentManager::SetWindowBounds(int32_t widget_id, const WindowRect& rect) {
  std::lock_guard<std::mutex> lock_protect(create_mutex_);
  auto render = GetXComponent(widget_id);
  if (!render) {
    LOGE("not found render id:%{public}d", widget_id);
    return;
  }

  bool sub_window = render->GetType() == XComponentType::kSubWindow;
  if (!sub_window) {
    AppWindowAdapter::GetInstance().SetBounds(widget_id, rect, []{});
  } else {
    SubWindowAdapter::GetInstance().SetBounds(widget_id, rect);
  }
}

void XComponentManager::OnActivationChanged(int32_t widget_id, bool active) {
  std::lock_guard<std::mutex> lock_protect(mutex_);
  auto render = GetXComponent(widget_id);
  if (!render || render->GetType() == XComponentType::kSubWindow) {
    LOGI("filter sub window render: %{public}d", widget_id);
    return;
  }
  // update focus window exclude sub window
  if (active) {
    SetActivateWindow(widget_id);
  }
}

void XComponentManager::OnWidgetAvailable(int32_t widget_id) {
  std::lock_guard<std::mutex> lock_protect(mutex_);
  auto render = GetXComponent(widget_id);
  if (!render || render->GetType() == XComponentType::kSubWindow) {
    LOGI("filter sub window render: %{public}d", widget_id);
    return;
  }
  // window created by the browser ui
  if (window_status_.contains(widget_id)) {
    window_status_[widget_id].set_value(true);
  } else {
    // window created by the ark ui, need to reuse it
    reuse_window_.push(widget_id);
  }
  creating_window_ = -1;

  // lost surface focus event in create xcomponent on OH platform, trigger manually
  SetActivateWindow(widget_id);
}

void XComponentManager::OnWidgetDestroyed(int32_t widget_id) {
  std::lock_guard<std::mutex> lock_protect(mutex_);
  RemoveXComponent(widget_id);
}

void XComponentManager::SetActivateWindow(int32_t widget_id) {
  activate_window_ = widget_id;

  auto jsFunc = ohos::adapter::GetJSFunction("ContextAdapter.SetActiveWindow");
  if (jsFunc) {
    jsFunc->Invoke<void>(widget_id);
  }
}

}  // namespace ohos::adapter::xcomponent

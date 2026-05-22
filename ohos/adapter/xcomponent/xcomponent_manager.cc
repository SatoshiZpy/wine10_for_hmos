// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/xcomponent/xcomponent_manager.h"

#include <chrono>

#include "ohos/adapter/accessibility/accessibility_adapter.h"
#include "ohos/adapter/aki_hook/aki_hook.h"
#include "ohos/adapter/common/logging.h"
#include "ohos/adapter/common/trace.h"
#include "ohos/adapter/xcomponent/adapter/input_event_callback_adapter.h"
#include "ohos/adapter/xcomponent/xcomponent_utils.h"

namespace ohos::adapter::xcomponent {

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
}

void XComponentManager::RequestLayout(int32_t widget_id) {
  auto render = GetXComponent(widget_id);
  if (!render) {
    LOGE("not found render id:%{public}d", widget_id);
    return;
  }
  render->RequestLayout();
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

  // lost surface focus event in create xcomponent on OH platform, trigger manually
  SetActivateWindow(widget_id);
}

void XComponentManager::OnWidgetDestroyed(int32_t widget_id) {
  std::lock_guard<std::mutex> lock_protect(mutex_);
  RemoveXComponent(widget_id);
  if (widget_id_ == widget_id) {
    widget_id_ = -1;
  }
}

void XComponentManager::SetActivateWindow(int32_t widget_id) {
  auto jsFunc = ohos::adapter::GetJSFunction("ContextAdapter.SetActiveWindow");
  if (jsFunc) {
    jsFunc->Invoke<void>(widget_id);
  }
  widget_id_ = widget_id;
}

int32_t XComponentManager::GetActivateWindow() {
  std::lock_guard<std::mutex> lock_protect(mutex_);
  return widget_id_;
}

}  // namespace ohos::adapter::xcomponent

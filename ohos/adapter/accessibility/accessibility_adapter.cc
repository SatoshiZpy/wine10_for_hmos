// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/accessibility/accessibility_adapter.h"

#include <dlfcn.h>
#include <cstring>

#include "aki/jsbind.h"
#include "ohos/adapter/accessibility/accessibility_delegate_ohos_registry.h"
#include "ohos/adapter/aki_hook/aki_hook.h"
#include "ohos/adapter/common/logging.h"
#include "ohos/adapter/device_info/device_info.h"

namespace ohos::adapter::accessibility {

__attribute__((no_sanitize("cfi", "cfi-icall")))
AccessibilityAdapter::AccessibilityAdapter()
    : native_accessibility_lib_("ace_ndk.z") {
  if (!native_accessibility_lib_.IsLoaded()) {
    LOGE(
        "AccessibilityAdapter::AccessibilityAdapter native_accessibility_lib_ "
        "load fail");
    return;
  }

  if (!native_accessibility_lib_.LoadFunction(
      &accessibility_provider_register_callback_fn_,
      "OH_ArkUI_AccessibilityProviderRegisterCallbackWithInstance")) {
    LOGE(
        "AccessibilityAdapter::AccessibilityAdapter get "
        "OH_ArkUI_AccessibilityProviderRegisterCallbackWithInstance failed");
    return;
  }
}

AccessibilityAdapter& AccessibilityAdapter::GetInstance() {
  static AccessibilityAdapter instance;
  return instance;
}

void AccessibilityAdapter::Speech(const std::string& text,
                                  int32_t id) {
  if (auto func = GetJSFunction("AccessibilityAdapter.Speech")) {
    func->Invoke<void>(text, id);
  } else {
    LOGW("[AccessibilityAdapter] Speech ets function not found");
  }
}

void AccessibilityAdapter::ShutDown() {
  if (auto func = GetJSFunction("AccessibilityAdapter.ShutDown")) {
    func->Invoke<void>();
  } else {
    LOGW("[AccessibilityAdapter] ShutDown ets function not found");
  }
}

__attribute__((no_sanitize("cfi", "cfi-icall")))
void AccessibilityAdapter::Initialize(OH_NativeXComponent* native_xcomponent,
                                      const std::string& id) {
  int32_t ret = OH_NativeXComponent_GetNativeAccessibilityProvider(
      native_xcomponent, &provider_);
  if (provider_ == nullptr) {
    LOGE(
        "AccessibilityAdapter::Initialize "
        "OH_NativeXComponent_GetNativeAccessibilityProvider get provider "
        "fail");
    return;
  }

  accessibility_provider_callbacks_ = {
      FindAccessibilityNodeInfosById,    FindAccessibilityNodeInfosByText,
      FindFocusedAccessibilityNode,      FindNextFocusAccessibilityNode,
      ExecuteAccessibilityAction,        ClearFocusedFocusAccessibilityNode,
      GetAccessibilityNodeCursorPosition};

  if (accessibility_provider_register_callback_fn_ == nullptr) {
    LOGE(
        "AccessibilityAdapter::Initialize "
        "OH_ArkUI_AccessibilityProviderRegisterCallbackWithInstance was not "
        "loaded correctly");
    return;
  }

  ret = accessibility_provider_register_callback_fn_(
      id.c_str(), provider_, &accessibility_provider_callbacks_);
  if (ret != 0) {
    LOGE(
        "AccessibilityAdapter::Initialize "
        "accessibility_provider_register_callback_fn_  run fail, instance_id: "
        "%{public}s, ret: %{public}d",
        id.c_str(), ret);
    return;
  }
}

// static
int32_t AccessibilityAdapter::FindAccessibilityNodeInfosById(
    const char* instance_id,
    int64_t element_id,
    ArkUI_AccessibilitySearchMode mode,
    int32_t request_id,
    NativeElementList element_list) {
  if (!element_list || !instance_id) {
    LOGE(
        "AccessibilityAdapter::FindAccessibilityNodeInfosById element_list or "
        "instance_id is null, request_id: %{public}d",
        request_id);
    return OH_NATIVEXCOMPONENT_RESULT_FAILED;
  }

  std::string xcomponent_id(instance_id);
  auto accessibility_delegate =
      AccessibilityDelegateOhosRegistry::GetInstance().GetAccessibilityDelegate(
          xcomponent_id);
  if (!accessibility_delegate) {
    LOGE(
        "AccessibilityAdapter::FindAccessibilityNodeInfosById "
        "accessibility_delegate of xcomponent_id: %{public}s can't not found, "
        "request_id: %{public}d",
        xcomponent_id.c_str(), request_id);
    return OH_NATIVEXCOMPONENT_RESULT_FAILED;
  }

  int32_t accessibility_id = static_cast<int32_t>(element_id);
  bool ret = accessibility_delegate->FindAccessibilityNodeInfosById(
      accessibility_id, mode, element_list);
  if (!ret) {
    LOGE(
        "AccessibilityAdapter::FindAccessibilityNodeInfosById failed, "
        "xcomponent_id: %{public}s, accessibility_id: %{public}d, request_id: "
        "%{public}d",
        xcomponent_id.c_str(), accessibility_id, request_id);
    return OH_NATIVEXCOMPONENT_RESULT_FAILED;
  }
  return OH_NATIVEXCOMPONENT_RESULT_SUCCESS;
}

// static
int32_t AccessibilityAdapter::FindAccessibilityNodeInfosByText(
    const char* instance_id,
    int64_t element_id,
    const char* text,
    int32_t request_id,
    NativeElementList element_list) {
  // TODO
  LOGW("AccessibilityAdapter::FindAccessibilityNodeInfosByText is not support, request_id: %{public}d", request_id);
  return OH_NATIVEXCOMPONENT_RESULT_FAILED;
}

// static
int32_t AccessibilityAdapter::FindFocusedAccessibilityNode(
    const char* instance_id,
    int64_t element_id,
    ArkUI_AccessibilityFocusType focusType,
    int32_t request_id,
    NativeElement element_info) {
  // TODO
  LOGW("AccessibilityAdapter::FindFocusedAccessibilityNode is not support, request_id: %{public}d", request_id);
  return OH_NATIVEXCOMPONENT_RESULT_FAILED;
}

// static
int32_t AccessibilityAdapter::FindNextFocusAccessibilityNode(
    const char* instance_id,
    int64_t element_id,
    ArkUI_AccessibilityFocusMoveDirection direction,
    int32_t request_id,
    NativeElement element_info) {
  // TODO
  LOGW("AccessibilityAdapter::FindNextFocusAccessibilityNode is not support, request_id: %{public}d", request_id);
  return OH_NATIVEXCOMPONENT_RESULT_FAILED;
}

// static
int32_t AccessibilityAdapter::ExecuteAccessibilityAction(
    const char* instance_id,
    int64_t element_id,
    ArkUI_Accessibility_ActionType action,
    ArkUI_AccessibilityActionArguments* action_arguments,
    int32_t request_id) {
  // TODO
  LOGW("AccessibilityAdapter::ExecuteAccessibilityAction is not support, request_id: %{public}d", request_id);
  return OH_NATIVEXCOMPONENT_RESULT_FAILED;
}

// static
int32_t AccessibilityAdapter::ClearFocusedFocusAccessibilityNode(
    const char* instance_id) {
  // TODO
  LOGW("AccessibilityAdapter::ClearFocusedFocusAccessibilityNode is not support");
  return OH_NATIVEXCOMPONENT_RESULT_FAILED;
}

// static
int32_t AccessibilityAdapter::GetAccessibilityNodeCursorPosition(
    const char* instance_id,
    int64_t element_id,
    int32_t request_id,
    int32_t* index) {
  // TODO
  LOGW("AccessibilityAdapter::GetAccessibilityNodeCursorPosition is not support, request_id: %{public}d", request_id);
  return OH_NATIVEXCOMPONENT_RESULT_FAILED;
}

}  // namespace ohos::adapter::accessibility

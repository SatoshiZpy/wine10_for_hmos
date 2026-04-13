// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_ACCESSIBILITY_ACCESSIBILITY_ADAPTER_H_
#define OHOS_ADAPTER_ACCESSIBILITY_ACCESSIBILITY_ADAPTER_H_

#include <ace/xcomponent/native_interface_xcomponent.h>
#include <arkui/native_interface_accessibility.h>
#include <string>

#include "ohos/adapter/common/shared_library.h"
#include "ohos/adapter/export.h"

namespace ohos::adapter::accessibility {

using NativeElementList = ArkUI_AccessibilityElementInfoList*;
using NativeElement = ArkUI_AccessibilityElementInfo*;

class NativeElementWrapper {};

class ADAPTER_EXPORT_API AccessibilityAdapter {
 public:
  class Delegate {
   public:
    Delegate() = default;
    virtual ~Delegate() = default;

    virtual bool FindAccessibilityNodeInfosById(
        int32_t element_id,
        ArkUI_AccessibilitySearchMode mode,
        NativeElementList element_list) = 0;
  };

  AccessibilityAdapter(const AccessibilityAdapter&) = delete;
  AccessibilityAdapter& operator=(const AccessibilityAdapter&) = delete;
  static AccessibilityAdapter& GetInstance();

  void Speech(const std::string& text, int32_t id);
  void ShutDown();

  void Initialize(OH_NativeXComponent* native_xcomponent, const std::string& id);
  static int32_t FindAccessibilityNodeInfosById(
      const char* instance_id,
      int64_t element_id,
      ArkUI_AccessibilitySearchMode mode,
      int32_t request_id,
      NativeElementList element_list);
  static int32_t FindAccessibilityNodeInfosByText(
      const char* instance_id,
      int64_t element_id,
      const char* text,
      int32_t request_id,
      NativeElementList element_list);
  static int32_t FindFocusedAccessibilityNode(
      const char* instance_id,
      int64_t element_id,
      ArkUI_AccessibilityFocusType focusType,
      int32_t request_id,
      NativeElement element_info);
  static int32_t FindNextFocusAccessibilityNode(
      const char* instance_id,
      int64_t element_id,
      ArkUI_AccessibilityFocusMoveDirection direction,
      int32_t request_id,
      NativeElement element_info);
  static int32_t ExecuteAccessibilityAction(
      const char* instance_id,
      int64_t element_id,
      ArkUI_Accessibility_ActionType action,
      ArkUI_AccessibilityActionArguments* action_arguments,
      int32_t request_id);
  static int32_t ClearFocusedFocusAccessibilityNode(const char* instance_id);
  static int32_t GetAccessibilityNodeCursorPosition(const char* instance_id,
                                                    int64_t element_id,
                                                    int32_t request_id,
                                                    int32_t* index);

 private:
  AccessibilityAdapter();
  ~AccessibilityAdapter() = default;

  ArkUI_AccessibilityProvider* provider_ = nullptr;
  ArkUI_AccessibilityProviderCallbacksWithInstance
      accessibility_provider_callbacks_;
  common::SharedLibrary native_accessibility_lib_;
  using NativeAccessibilityForProviderFunc =
      int32_t(const char*,
              ArkUI_AccessibilityProvider*,
              ArkUI_AccessibilityProviderCallbacksWithInstance*);
  NativeAccessibilityForProviderFunc*
      accessibility_provider_register_callback_fn_ = nullptr;
};

}  // namespace ohos::adapter::accessibility
#endif  // OHOS_ADAPTER_ACCESSIBILITY_ACCESSIBILITY_ADAPTER_H_

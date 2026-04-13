// Copyright (c) 2025 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/accessibility/accessibility_delegate_ohos_registry.h"

#include "ohos/adapter/common/logging.h"

namespace ohos::adapter::accessibility {

// static
AccessibilityDelegateOhosRegistry&
AccessibilityDelegateOhosRegistry::GetInstance() {
  static AccessibilityDelegateOhosRegistry instance;
  return instance;
}

std::shared_ptr<AccessibilityAdapter::Delegate>
AccessibilityDelegateOhosRegistry::GetAccessibilityDelegate(
    const std::string& window_id) {
  auto it = window_to_delegate_.find(window_id);
  if (it == window_to_delegate_.end()) {
    LOGE(
        "AccessibilityDelegateOhosRegistry::GetAccessibilityDelegate the "
        "delegate of window_id: %{public}s can't not found",
        window_id.c_str());
    return nullptr;
  }

  return it->second;
}

void AccessibilityDelegateOhosRegistry::RegisterAccessibilityDelegate(
    const std::string& window_id,
    std::shared_ptr<AccessibilityAdapter::Delegate> accessibility_delegate) {
  window_to_delegate_[window_id] = std::move(accessibility_delegate);
}

void AccessibilityDelegateOhosRegistry::UnregisterAccessibilityDelegate(
    const std::string& window_id) {
  auto it = window_to_delegate_.find(window_id);
  if (it != window_to_delegate_.end()) {
    window_to_delegate_.erase(it);
  }
}

}  // namespace ohos::adapter::accessibility

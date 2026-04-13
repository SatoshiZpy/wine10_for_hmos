// Copyright (c) 2025 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_ACCESSIBILITY_ACCESSIBILITY_DELEGATE_OHOS_REGISTRY_H_
#define OHOS_ADAPTER_ACCESSIBILITY_ACCESSIBILITY_DELEGATE_OHOS_REGISTRY_H_

#include <unordered_map>
#include "ohos/adapter/export.h"

#include "ohos/adapter/accessibility/accessibility_adapter.h"

namespace ohos::adapter::accessibility {

class ADAPTER_EXPORT_API AccessibilityDelegateOhosRegistry {
 public:
  // Get the global instance of this class.
  static AccessibilityDelegateOhosRegistry& GetInstance();

  AccessibilityDelegateOhosRegistry(const AccessibilityDelegateOhosRegistry&) =
      delete;
  AccessibilityDelegateOhosRegistry& operator=(
      const AccessibilityDelegateOhosRegistry&) = delete;

  // Retrieve an |accessibility delegate| by |window_id|.
  // Returns nullptr if no accessibility delegate is registered for |window_id|.
  std::shared_ptr<AccessibilityAdapter::Delegate> GetAccessibilityDelegate(
      const std::string& window_id);

  // Registers the |accessibility delegate| for the specified |window_id|.
  void RegisterAccessibilityDelegate(
      const std::string& window_id,
      std::shared_ptr<AccessibilityAdapter::Delegate> accessibility_delegate);

  // Removes the |accessibility delegate| for the specified |window_id|.
  // There must be an accessibility delegate registered for |window_id| when this
  // method is called.
  void UnregisterAccessibilityDelegate(const std::string& window_id);

 private:
  AccessibilityDelegateOhosRegistry() = default;
  ~AccessibilityDelegateOhosRegistry() = default;

  std::unordered_map<std::string, std::shared_ptr<AccessibilityAdapter::Delegate>>
      window_to_delegate_;
};

}  // namespace ohos::adapter::accessibility
#endif  // OHOS_ADAPTER_ACCESSIBILITY_ACCESSIBILITY_DELEGATE_OHOS_REGISTRY_H_

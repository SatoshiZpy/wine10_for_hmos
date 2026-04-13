// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_XCOMPONENT_XCOMPONENT_UTILS_H_
#define OHOS_ADAPTER_XCOMPONENT_XCOMPONENT_UTILS_H_

#include <algorithm>
#include <memory>
#include <string>

#include <ace/xcomponent/native_interface_xcomponent.h>

#include "ohos/adapter/xcomponent/xcomponent_manager.h"

namespace ohos::adapter::xcomponent {

class XComponentUtils {
 public:
  static std::string ConvertWidgetIdToWindowId(int32_t widget_id);
  static int32_t ConvertWindowIdToWidgetId(const std::string& window_id);
  static bool GetRenderInfo(OH_NativeXComponent* component, XComponentInfo& info);
  static XComponentType ConvertXComponentType(const std::string& type);
  static std::shared_ptr<XComponentImpl> GetXComponent(int32_t widget_id);
};

}  // namespace ohos::adapter::xcomponent

#endif  // OHOS_ADAPTER_XCOMPONENT_XCOMPONENT_UTILS_H_

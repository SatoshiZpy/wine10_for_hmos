// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/xcomponent/xcomponent_utils.h"

#include "ohos/adapter/common/logging.h"

namespace ohos::adapter::xcomponent {

const std::string kWindowPrefix = "surface";

std::string XComponentUtils::ConvertWidgetIdToWindowId(int32_t widget_id) {
  if (widget_id <= 0) {
    LOGW("invalid widget_id");
    return "";
  }
  return kWindowPrefix + std::to_string(widget_id);
}

int32_t XComponentUtils::ConvertWindowIdToWidgetId(const std::string& window_id) {
  if (!window_id.starts_with(kWindowPrefix) ||
      !std::all_of(window_id.begin() + kWindowPrefix.size(), window_id.end(),
          [](char i) { return isdigit(i); })) {
    return -1;
  }
  std::string widgetId = window_id.substr(kWindowPrefix.size());
  return std::stoi(widgetId);
}

bool XComponentUtils::GetRenderInfo(OH_NativeXComponent* component, XComponentInfo& info) {
  char id_str[OH_XCOMPONENT_ID_LEN_MAX + 1] = {0};
  uint64_t id_size = OH_XCOMPONENT_ID_LEN_MAX + 1;
  int32_t ret =
      OH_NativeXComponent_GetXComponentId(component, id_str, &id_size);
  if (ret != OH_NATIVEXCOMPONENT_RESULT_SUCCESS) {
    LOGE("Unable to get xcomponent id for id_str:%{public}s", id_str);
    return false;
  }
  std::string xcomponent_id(id_str);
  std::size_t first_pos = xcomponent_id.find_first_of(":");
  if (first_pos == std::string::npos) {
    LOGE("xcomponent invalid id: %{public}s", id_str);
    return false;
  }
  auto id = xcomponent_id.substr(0, first_pos);

  std::size_t last_pos = xcomponent_id.find_last_of(":");
  if (last_pos == std::string::npos) {
    LOGE("xcomponent invalid id: %{public}s", id_str);
    return false;
  }
  auto type = xcomponent_id.substr(first_pos + 1, last_pos - first_pos - 1);
  auto pid = xcomponent_id.substr(last_pos + 1);

  info.render_id = xcomponent_id;
  info.widget_id = std::stoi(id);
  info.type = ConvertXComponentType(type);
  info.pid = std::stoi(pid);
  return true;
}

XComponentType XComponentUtils::ConvertXComponentType(const std::string& type) {
  if (type == "xcomponent") {
    return XComponentType::kWindow;
  } else if (type == "subXcomponent") {
    return XComponentType::kSubWindow;
  } else {
    return XComponentType::kUnknown;
  }
}

std::shared_ptr<XComponentImpl> XComponentUtils::GetXComponent(int32_t widget_id) {
  auto manager = XComponentManager::GetInstance();
  if (!manager) {
    LOGW("Failed to get XComponentManager. widget_id:%{public}d", widget_id);
    return nullptr;
  }
  auto impl = manager->GetXComponent(widget_id);
  if (!impl) {
    LOGW("Failed to get xcomponent instance. widget_id:%{public}d", widget_id);
    return nullptr;
  }
  return impl;
}

}  // namespace ohos::adapter::xcomponent

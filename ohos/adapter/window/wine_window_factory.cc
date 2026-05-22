// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/window/wine_window.h"

#include "ohos/adapter/common/logging.h"
#include "ohos/adapter/window/window_common.h"
#include "ohos/adapter/window/wine_fake_window.h"
#include "ohos/adapter/window/wine_float_window.h"
#include "ohos/adapter/window/wine_popup_window.h"
#include "ohos/adapter/window/wine_toplevel_window.h"
#include "ohos/adapter/window/wine_window_manager.h"

namespace ohos::adapter::window {

// static
std::shared_ptr<WineWindow> WineWindow::Create(
    const ipc::WindowParam& param,
    int* widget_id)
{
  LOGI("WineWindow::Create, type is %{public}d", param.window_type);
  std::shared_ptr<WineWindow> window;
  switch (param.window_type) {
    case WindowInitType::kPopup:
    case WindowInitType::kMenu:
    case WindowInitType::kTooltip:
      // sub window created with a parent window set.
      if (auto parent = WineWindowManager::GetInstance().GetWindow(param.parent_id)) {
        window = std::make_shared<WinePopupWindow>(parent);
      } else {
        LOGW("Failed to determine parent: %{public}d for menu/popup window.", param.parent_id);
        auto fake_parent = std::make_shared<WineFakeWindow>();
        window = std::make_shared<WinePopupWindow>(fake_parent);
      }
      break;
    case WindowInitType::kWindow: {
      bool use_floating_window = has_feature(WindowFeatures::USE_FLOATING_WINDOW, param.features);
      if (!use_floating_window) {
        window = std::make_shared<WineToplevelWindow>();
      } else {
        window = std::make_shared<WineFloatWindow>();
      }
      break;
    }
    default:
      LOGE("unsupported window type %{public}d", param.window_type);
      return nullptr;
  }
  if (!window->Initialize(std::move(param))) {
    LOGE("Failed to create window, type %{public}d", param.window_type);
    return nullptr;
  } 
  *widget_id = window->GetWidget();
  WineWindowManager::GetInstance().AddWindow(*widget_id, window);
  return window;
}

}  // namespace ohos::adapter::window

// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/window/adapter/system_floating_window_adapter.h"

#include "ohos/adapter/aki_hook/aki_hook.h"

namespace ohos::adapter::window {

SystemFloatingWindowAdapter& SystemFloatingWindowAdapter::GetInstance() {
  static SystemFloatingWindowAdapter instance;
  return instance;
}

int32_t SystemFloatingWindowAdapter::Create(const NewWindowParam& param) {
  auto promise = std::make_shared<std::promise<int32_t>>();
  std::function<void(int32_t)> callback =
    [promise](int32_t reply_id) {
      promise->set_value(reply_id);
    };

  if (auto jsFunc = ohos::adapter::GetJSFunction("SystemFloatingWindow.CreateWindow")) {
    jsFunc->Invoke<void>(param, callback);
    auto future = promise->get_future();
    auto status = future.wait_for(std::chrono::seconds(3));
    if (status == std::future_status::timeout) {
      LOGE("SystemFloatingWindow::Create timeout");
      return -1;
    }
    return future.get();
  }
  LOGE("SystemFloatingWindow::Create error");
  return -1;
}

void SystemFloatingWindowAdapter::Close(int32_t id) {
  auto jsFunc =
      ohos::adapter::GetJSFunction("SystemFloatingWindow.CloseWindow");
  if (jsFunc) {
    jsFunc->Invoke<void>(id);
  }
}

void SystemFloatingWindowAdapter::SetBounds(int32_t id,
                                            const WindowRect& rect) {
  auto promise = std::make_shared<std::promise<bool>>();
  std::function<void(bool)> on_completed = [promise](bool successful) -> void {
    promise->set_value(successful);
  };
  auto jsFunc = ohos::adapter::GetJSFunction("SystemFloatingWindow.SetBounds");
  if (jsFunc) {
    jsFunc->Invoke<void>(id, rect, on_completed);
    auto future = promise->get_future();
    auto status = future.wait_for(std::chrono::seconds(3));
    if (status == std::future_status::timeout) {
      LOGE("SystemFloatingWindow.SetBounds timeout");
      return;
    }
    bool successful = future.get();
    if (!successful) {
      LOGE("SystemFloatingWindow.SetBounds failed");
    }
  }
}

void SystemFloatingWindowAdapter::SetWindowLimits(int32_t min_width,
                                                  int32_t min_height,
                                                  int32_t max_width,
                                                  int32_t max_height,
                                                  int32_t id) {
  auto jsFunc =
      ohos::adapter::GetJSFunction("SystemFloatingWindow.SetWindowLimits");
  if (jsFunc) {
    jsFunc->Invoke<void>(min_width, min_height, max_width, max_height, id);
  }
}

void SystemFloatingWindowAdapter::StartWindowMoving(int32_t id) {
  auto jsFunc = ohos::adapter::GetJSFunction("SystemFloatingWindow.StartWindowMoving");
  if (jsFunc) {
    jsFunc->Invoke<void>(id);
  }
}

}  // namespace ohos::adapter::window

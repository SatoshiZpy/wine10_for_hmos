// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/window/sub_window_adapter.h"

#include <chrono>
#include <cstdint>
#include <future>

#include "ohos/adapter/aki_hook/aki_hook.h"
#include "ohos/adapter/common/logging.h"
#include "ohos/adapter/common/trace.h"

namespace ohos::adapter::window {

SubWindowAdapter& SubWindowAdapter::GetInstance() {
  static SubWindowAdapter helper;
  return helper;
}

int32_t SubWindowAdapter::Create(const NewWindowParam& param) {
  TRACE_EVENT_0("SubWindowAdapter::Create");
  auto promise = std::make_shared<std::promise<int32_t>>();
  std::function<void(int32_t)> callback =
    [promise](int32_t reply_id) {
      promise->set_value(reply_id);
    };

  if (auto func = ohos::adapter::GetJSFunction("SubWindow.Create")) {
    func->Invoke<void>(param, callback);
    auto future = promise->get_future();
    auto status = future.wait_for(std::chrono::seconds(3));
    if (status == std::future_status::timeout) {
      LOGE("SubWindowAdapter::Create timeout");
      return -1;
    }
    return future.get();
  }
  LOGE("SubWindowAdapter::Create error");
  return -1;
}

void SubWindowAdapter::Close(int32_t id) {
  if (auto func = ohos::adapter::GetJSFunction("SubWindow.Close")) {
    func->Invoke<void>(id);
  }
}

void SubWindowAdapter::Show(int32_t id) {
  if (auto func = ohos::adapter::GetJSFunction("SubWindow.Show")) {
    func->Invoke<void>(id);
  }
}

void SubWindowAdapter::Hide(int32_t id) {
  if (auto func = ohos::adapter::GetJSFunction("SubWindow.Hide")) {
    func->Invoke<void>(id);
  }
}

void SubWindowAdapter::SetBounds(int32_t id, const WindowRect& rect) {
  if (auto func = ohos::adapter::GetJSFunction("SubWindow.SetBounds")) {
    func->Invoke<void>(id, rect);
  }
}
}  // namespace ohos::adapter::window

// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/window/adapter/app_window_adapter.h"

#include "ohos/adapter/aki_hook/aki_hook.h"
#include "ohos/adapter/common/logging.h"

namespace ohos::adapter::window {

AppWindowAdapter& AppWindowAdapter::GetInstance() {
  static AppWindowAdapter instance;
  return instance;
}

int32_t AppWindowAdapter::Create(const NewWindowParam& param) {
  auto promise = std::make_shared<std::promise<int32_t>>();
  std::function<void(int32_t)> callback =
    [promise](int32_t reply_id) {
      promise->set_value(reply_id);
    };

  if (auto jsFunc = ohos::adapter::GetJSFunction("AppWindow.CreateWindow")) {
    jsFunc->Invoke<void>(param, callback);
    auto future = promise->get_future();
    auto status = future.wait_for(std::chrono::seconds(3));
    if (status == std::future_status::timeout) {
      LOGE("AppWindowAdapter::Create timeout");
      return -1;
    }
    return future.get();
  }
  LOGE("AppWindowAdapter::Create error");
  return -1;
}

void AppWindowAdapter::Close(int32_t id) {
  auto jsFunc = ohos::adapter::GetJSFunction("AppWindow.CloseWindow");
  if (jsFunc) {
    jsFunc->Invoke<void>(id);
  }
}

void AppWindowAdapter::Show(int32_t id) {
  auto jsFunc = ohos::adapter::GetJSFunction("AppWindow.ShowWindow");
  if (jsFunc) {
    jsFunc->Invoke<void>(id);
  }
}

void AppWindowAdapter::Hide(int32_t id) {
  auto jsFunc = ohos::adapter::GetJSFunction("AppWindow.HideWindow");
  if (jsFunc) {
    jsFunc->Invoke<void>(id);
  }
}

void AppWindowAdapter::Activate(int32_t id) {
  auto jsFunc = ohos::adapter::GetJSFunction("AppWindow.ActivateWindow");
  if (jsFunc) {
    jsFunc->Invoke<void>(id);
  }
}

void AppWindowAdapter::SetFullscreen(int32_t id) {
  auto jsFunc = ohos::adapter::GetJSFunction("AppWindow.SetFullscreen");
  if (jsFunc) {
    jsFunc->Invoke<void>(id);
  }
}

void AppWindowAdapter::SetBounds(int32_t id,
                                 const WindowRect& rect,
                                 ChangeSizeCallback callback) {
  auto jsFunc = ohos::adapter::GetJSFunction("AppWindow.SetBounds");
  if (jsFunc) {
    jsFunc->Invoke<void>(id, rect, callback);
  }
}

void AppWindowAdapter::SetEnabled(bool enable, int32_t id) {
  auto jsFunc = ohos::adapter::GetJSFunction("AppWindow.SetEnabled");
  if (jsFunc) {
    jsFunc->Invoke<void>(enable, id);
  }
}

void AppWindowAdapter::Maximize(int32_t id) {
  auto jsFunc = ohos::adapter::GetJSFunction("AppWindow.Maximize");
  if (jsFunc) {
    jsFunc->Invoke<void>(id);
  }
}

void AppWindowAdapter::UnMaximize(int32_t id) {
  auto jsFunc = ohos::adapter::GetJSFunction("AppWindow.UnMaximize");
  if (jsFunc) {
    jsFunc->Invoke<void>(id);
  }
}

void AppWindowAdapter::Minimize(int32_t id) {
  auto jsFunc = ohos::adapter::GetJSFunction("AppWindow.Minimize");
  if (jsFunc) {
    jsFunc->Invoke<void>(id);
  }
}

void AppWindowAdapter::SetWindowLimits(int32_t min_width,
                                       int32_t min_height,
                                       int32_t max_width,
                                       int32_t max_height,
                                       int32_t id) {
  auto jsFunc = ohos::adapter::GetJSFunction("AppWindow.SetWindowLimits");
  if (jsFunc) {
    jsFunc->Invoke<void>(min_width, min_height, max_width, max_height, id);
  }
}

void AppWindowAdapter::SetTitle(const std::string& title, int32_t id) {
  auto jsFunc = ohos::adapter::GetJSFunction("AppWindow.SetTitle");
  if (jsFunc) {
    jsFunc->Invoke<void>(title, id);
  }
}

bool AppWindowAdapter::Relaunch() {
  auto jsFunc = ohos::adapter::GetJSFunction("AppWindow.Relaunch");
  if (jsFunc) {
    return jsFunc->Invoke<bool>();
  }
  return false;
}

void AppWindowAdapter::StartWindowMoving(int32_t id) {
  auto jsFunc = ohos::adapter::GetJSFunction("AppWindow.StartWindowMoving");
  if (jsFunc) {
    jsFunc->Invoke<void>(id);
  }
}

std::vector<int32_t> AppWindowAdapter::GetWindowsByCoordinate(const PointCoordinate& coordinate) {
  std::promise<std::vector<int32_t>> promise;
  std::function<void(std::vector<int32_t>)> callback =
      [&](std::vector<int32_t> window_ids) -> void {
    promise.set_value(window_ids);
  };
  auto func = ohos::adapter::GetJSFunction("AppWindow.GetWindowsByCoordinate");
  if (func) {
    func->Invoke<void>(coordinate, callback);
  }
  std::vector<int32_t> result = promise.get_future().get();
  return result;
}

void AppWindowAdapter::Restore(int32_t id) {
  auto jsFunc = ohos::adapter::GetJSFunction("AppWindow.Restore");
  if (jsFunc) {
    jsFunc->Invoke<void>(id);
  }
}

void AppWindowAdapter::StartWindowMovingWithOffset(const int32_t id,
                                                   const float offset_x,
                                                   const float offset_y) {
  auto jsFunc = ohos::adapter::GetJSFunction("AppWindow.StartWindowMovingWithOffset");
  if (jsFunc) {
    jsFunc->Invoke<void>(id, offset_x, offset_y);
  }
}

std::vector<int32_t> AppWindowAdapter::GetOriginWindowIds(
    std::vector<int32_t> window_ids) {
  auto jsFunc = ohos::adapter::GetJSFunction("AppWindow.GetOriginWindowIds");
  if (jsFunc) {
    return jsFunc->Invoke<std::vector<int32_t>>(window_ids);
  }
  return std::vector<int32_t>();
}

bool AppWindowAdapter::ShiftWindowEvent(const int32_t source_id,
                                        const int32_t target_id) {
  auto jsFunc = ohos::adapter::GetJSFunction("AppWindow.ShiftWindowEvent");
  if (jsFunc) {
    auto promise = std::make_shared<std::promise<bool>>();
    auto future = promise->get_future();
    std::function<void(bool)> callback = [promise](bool result) -> void {
      promise->set_value(result);
    };
    jsFunc->Invoke<void>(source_id, target_id, callback);
    auto status = future.wait_for(std::chrono::seconds(3));
    if (status == std::future_status::timeout) {
      LOGE("AppWindowAdapter::ShiftWindowEvent Wait timeout");
      return false;
    }
    bool result = future.get();
    return result;
  }
  return false;
}

void AppWindowAdapter::SetAlwaysOnTop(int32_t id, bool onTop) {
  auto jsFunc = ohos::adapter::GetJSFunction("AppWindow.SetAlwaysOnTop");
  if (jsFunc) {
    jsFunc->Invoke<void>(id, onTop);
  }
}

}  // namespace ohos::adapter::window

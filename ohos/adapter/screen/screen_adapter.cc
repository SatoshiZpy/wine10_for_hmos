// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/screen/screen_adapter.h"

#include "ohos/adapter/aki_hook/aki_hook.h"
#include "ohos/adapter/common/logging.h"

namespace ohos {
namespace adapter {

ScreenAdapter::ScreenAdapter() {}

static void ConvertDisplay(aki::Value complete_display, Display& display) {
  aki::Value base_display = complete_display["baseDisplay"];
  aki::Value avail_area = complete_display["availArea"];

  display.id = base_display["id"].As<int32_t>();
  display.name = base_display["name"].As<std::string>();
  display.alive = base_display["alive"].As<bool>();
  display.state = base_display["state"].As<DisplayState>();
  display.refresh_rate = base_display["refreshRate"].As<int32_t>();
  display.rotation = base_display["rotation"].As<DisplayRotation>();
  display.width = base_display["width"].As<int32_t>();
  display.height = base_display["height"].As<int32_t>();
  display.density_dpi = base_display["densityDPI"].As<float>();
  display.orientation = base_display["orientation"].As<DisplayOrientation>();
  display.density_pixels = base_display["densityPixels"].As<float>();
  display.scaled_density = base_display["scaledDensity"].As<float>();
  display.x_dpi = base_display["xDPI"].As<float>();
  display.y_dpi = base_display["yDPI"].As<float>();

  display.avail_area.left = avail_area["left"].As<int32_t>();
  display.avail_area.top = avail_area["top"].As<int32_t>();
  display.avail_area.width = avail_area["width"].As<int32_t>();
  display.avail_area.height = avail_area["height"].As<int32_t>();
}

bool ScreenAdapter::GetDefaultDisplay(Display& display) {
  if (default_display_.id > 0) {
    Display = default_display_;
    return true;
  }

  auto func = ohos::adapter::GetJSFunction("OhosDisplayAdapter.GetDefaultDisplay");
  if (!func) {
    LOGE("GetDefaultDisplay: JSFunction not available");
    return false;
  }

  std::promise<bool> promise;
  std::function<void(aki::Value)> callback =
    [&](aki::Value complete_display) {
    if (complete_display["id"].As<int32_t>() == -1) {
      promise.set_value(false);
      return;
    }
    ConvertDisplay(complete_display, display);
    promise.set_value(true);
  };
  
  func->Invoke<void>(callback);
  return promise.get_future().get();
}

void ScreenAdapter::SetDefaultDisplay(const Display& display) {
  LOGI("SetDefaultDisplay id: %{public}d, name: %{public}s, size: %{public}d, %{public}d"
       "avail_area: %{public}d, %{public}d, %{public}d, %{public}d",
    display.id, display.name.c_str(), display.width, display.height,
    display.avail_area.left, display.avail_area.top,
    display.avail_area.width, display.avail_area.height);
  default_display_ = display;
}

bool ScreenAdapter::GetAllDisplays(std::vector<Display>& ohos_displays) {
  auto func = ohos::adapter::GetJSFunction("OhosDisplayAdapter.GetAllDisplays");
  if (!func) {
    LOGE("GetAllDisplays: JSFunction not available");
    return false;
  }
  std::promise<bool> promise;
  std::function<void(aki::Value, int32_t)> callback =
    [&](aki::Value complete_displays, int32_t len) {
    if (!complete_displays.IsArray()) {
      LOGE("[Display]GetAllDisplays input param err, displays is not array.");
      promise.set_value(false);
      return;
    }

    for (int32_t i = 0; i < len; i++) {
      Display display;
      ConvertDisplay(complete_displays[i], display);
      ohos_displays.push_back(display);
    }

    promise.set_value(true);
  };

  func->Invoke<void>(callback);
  return promise.get_future().get();
}

ScreenAdapter& ScreenAdapter::GetInstance() {
  static ScreenAdapter helper;
  return helper;
}

void ScreenAdapter::RegisterDisplayMonitor() {
  if (auto func =
        ohos::adapter::GetJSFunction("OhosDisplayAdapter.registerDisplayMonitor")) {
    func->Invoke<void>();
  }
}

void ScreenAdapter::RegisterCallback(
    ScreenAdapter::DisplayChangeCallback callback) {
  callback_ = callback;
}

ScreenAdapter::DisplayChangeCallback ScreenAdapter::GetCallback() {
  return callback_;
}

void OnDisplayChangeCallback(const std::string& event, int32_t id) {
  if (ScreenAdapter::GetInstance().GetCallback() != nullptr) {
    ScreenAdapter::GetInstance().GetCallback()(event, id);
  }
}

void OnFontSizeChangeCallback(float zoom) {
  if (ScreenAdapter::GetInstance().GetCallback() != nullptr) {
    ScreenAdapter::GetInstance().GetCallback()("change", 0);
  }
}

void SetDefaultDisplay(aki::Value complete_display) {
  if (complete_display["id"].As<int32_t>() == -1) {
    return;
  }
  Display display;
  ConvertDisplay(complete_display, display);
  ScreenAdapter::GetInstance().SetDefaultDisplay(display);
}

JSBIND_GLOBAL() {
  JSBIND_FUNCTION(OnDisplayChangeCallback);
  JSBIND_FUNCTION(OnFontSizeChangeCallback);
  JSBIND_FUNCTION(SetDefaultDisplay);
}

}  // namespace adapter
}  // namespace ohos

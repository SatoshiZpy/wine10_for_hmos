// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/screen/screen_adapter.h"

#include <cmath>
#include <algorithm>

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
    display = default_display_;
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
  LOGI("SetDefaultDisplay id: %{public}d, name: %{public}s, size: %{public}d, %{public}d, "
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

WindowRect ScreenAdapter::GetDisplayAvailArea() {
  Display display;
  if (!GetDefaultDisplay(display)) {
    LOGE("GetDisplayAvailArea: get default display failed");
    return WindowRect{};
  }
  LOGI("GetDisplayAvailArea rect: %{public}d, %{public}d, %{public}d, %{public}d",
      display.avail_area.left, display.avail_area.top,
      display.avail_area.width, display.avail_area.height);
  return display.avail_area;
}

float ScreenAdapter::GetDensityPixels() {
  Display display;
  if (!GetDefaultDisplay(display)) {
    LOGE("GetDensityPixels: get default display failed");
    return 1.0f;
  }
  float dp = display.density_pixels;
  LOGI("densityPixels: %{public}f", dp);
  return dp > 0.0f ? dp : 1.0f;
}

int ScreenAdapter::GetSystemDpi() {
  Display display;
  if (!GetDefaultDisplay(display) ||
      display.avail_area.width <= 0 || display.avail_area.height <= 0) {
    LOGE("GetSystemDpi: get display physical size error");
    return 96;
  }

  LOGI("GetSystemDpi: x_dpi=%{public}f, y_dpi=%{public}f, (%{public}d,%{public}d)",
      display.x_dpi, display.y_dpi, display.avail_area.width, display.avail_area.height);

  // Calculate physical size (in inches)
  double width_inch = display.avail_area.width / display.x_dpi;
  double height_inch = display.avail_area.height / display.y_dpi;
  double diag_inch = std::sqrt(width_inch * width_inch + height_inch * height_inch);
  double diag_px = std::sqrt(
      static_cast<double>(display.avail_area.width * display.avail_area.width) +
      static_cast<double>(display.avail_area.height * display.avail_area.height));

  // Calculate physical PPI
  double physical_ppi = diag_px / diag_inch;

  // Determine usage scenario based on screen size and apply correction factor
  double correction_factor = 1.0;
  if (diag_inch <= 15.6) {
      // Laptop screens (12-15.6 inches): scale at 60-70% of physical PPI
      correction_factor = 0.65;
  } else if (diag_inch <= 27) {
      // Desktop monitors (21-27 inches): scale at 80-90% of physical PPI
      correction_factor = 0.85;
  } else {
      // Large-screen TVs (32 inches+): scale at 100% of physical PPI
      correction_factor = 1.0;
  }

  // Calculate effective target DPI (considering viewing distance)
  double effective_target_dpi = physical_ppi * correction_factor;
  int dpi = static_cast<int>(std::round(effective_target_dpi));
  dpi = std::max(96, std::min(240, dpi));

  LOGI("physical PPI: %{public}.2f, size: %{public}.1f inch, factor: %{public}.2f, effective DPI: %{public}d",
      physical_ppi, diag_inch, correction_factor, dpi);

  // Map to Windows standard DPI scaling levels
  if (dpi > 96 && dpi <= 108) dpi = 96;           // 100%
  else if (dpi > 108 && dpi <= 120) dpi = 120;    // 125%
  else if (dpi > 120 && dpi <= 144) dpi = 144;    // 150%
  else if (dpi > 144 && dpi <= 168) dpi = 168;    // 175%
  else if (dpi > 168 && dpi <= 192) dpi = 192;    // 200%
  LOGI("recommended DPI: %{public}d (scale: %{public}.0f%%)", dpi, (dpi * 100.0 / 96));

  return dpi;
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

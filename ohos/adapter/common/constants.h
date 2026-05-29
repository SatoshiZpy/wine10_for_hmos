// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_COMMON_CONSTANTS_H_
#define OHOS_ADAPTER_COMMON_CONSTANTS_H_

#include <cstdint>
#include <string>

#ifndef DECLARE_NAPI_METHOD
#define DECLARE_NAPI_METHOD(name, func) \
  { name, 0, func, 0, 0, 0, napi_default, 0 }
#endif

enum ProcessType {
  kMainProcess = 0,
  kServerProcess,
  kChildProcess,
  kRenderProcess,
  kCrashpadHandler,
  kGpuProcess,
};

enum AbilityType {
  kEntryAbility,
  kEmbeddedAbility,
};

enum WindowInitType {
  kWindow,
  kPopup,
  kMenu,
  kTooltip,
  kDrag,
  kBubble,
};

struct WindowRect {
 public:
  int32_t left = 0;
  int32_t top = 0;
  int32_t width = 0;
  int32_t height = 0;
};

struct WindowStyle {
 public:
  bool hide_title_bar = true;
  bool close_button = false;
  bool minimize_button = false;
  bool maximize_button = false;
  bool use_floating_window = false;
  bool use_dark_mode = false;
  bool always_on_top = false;
  bool resizable = true;
  bool is_modal = false;
  bool focusable = true;
  bool touchable = true;
  bool show_window = false;
};

struct NewWindowParam {
 public:
  AbilityType ability_type = AbilityType::kEntryAbility;
  int32_t parent_id = -1;
  int32_t widget_id = -1;
  WindowRect bounds;
  int32_t feature_mask;
  int32_t window_state_mask;
  int32_t init_color_argb;
  int32_t hwnd = -1;
  int32_t owner_pid = -1;
};

struct PointCoordinate {
 public:
  float x;
  float y;
  int32_t displayId;
};

struct WindowLimits {
  int max_width;
  int max_height;
  int min_width;
  int min_height;
};

enum DisplayOrientation {
  PORTRAIT = 0,
  LANDSCAPE = 1,
  PORTRAIT_INVERTED = 2,
  LANDSCAPE_INVERTED = 3,
};

enum DisplayRotation {
  ROTATE_0 = 0,
  ROTATE_90,
  ROTATE_180,
  ROTATE_270,
  ROTATE_INVALID,
};

enum DisplayState {
  STATE_UNKNOWN = 0,
  STATE_OFF = 1,
  STATE_ON = 2,
  STATE_DOZE = 3,
  STATE_DOZE_SUSPEND = 4,
  STATE_VR = 5,
  STATE_ON_SUSPEND = 6,
};

struct Display {
  int32_t id = -1;
  std::string name;
  bool alive = false;
  DisplayState state = STATE_UNKNOWN;
  int32_t refresh_rate = 0;
  DisplayRotation rotation = ROTATE_0;
  int32_t width = 0;
  int32_t height = 0;
  WindowRect avail_area;
  float density_dpi = 0.0;
  DisplayOrientation orientation = PORTRAIT;
  float density_pixels = 0.0;
  float scaled_density = 0.0;
  float x_dpi = 0.0;
  float y_dpi = 0.0;
};

inline bool IsRectEmpty(const WindowRect& rect) {
    return rect.width <= 0 || rect.height <= 0;
}

#endif  // OHOS_ADAPTER_COMMON_CONSTANTS_H_

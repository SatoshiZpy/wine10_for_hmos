// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/window/window_common.h"

#include <sstream>

#include "ohos/adapter/aki_hook/aki_hook.h"

namespace ohos::adapter::window {

JSBIND_CLASS(WindowRect) {
  JSBIND_PROPERTY(left);
  JSBIND_PROPERTY(top);
  JSBIND_PROPERTY(width);
  JSBIND_PROPERTY(height);
}

JSBIND_CLASS(WindowStyle) {
  JSBIND_PROPERTY(hide_title_bar);
  JSBIND_PROPERTY(close_button);
  JSBIND_PROPERTY(minimize_button);
  JSBIND_PROPERTY(maximize_button);
  JSBIND_PROPERTY(use_floating_window);
  JSBIND_PROPERTY(use_dark_mode);
  JSBIND_PROPERTY(always_on_top);
  JSBIND_PROPERTY(resizable);
  JSBIND_PROPERTY(is_modal);
  JSBIND_PROPERTY(focusable);
  JSBIND_PROPERTY(touchable);
  JSBIND_PROPERTY(show_widow);
}

JSBIND_CLASS(NewWindowParam) {
  JSBIND_PROPERTY(ability_type);
  JSBIND_PROPERTY(parent_id);
  JSBIND_PROPERTY(widget_id);
  JSBIND_PROPERTY(bounds);
  JSBIND_PROPERTY(feature_mask);
  JSBIND_PROPERTY(window_state_mask);
  JSBIND_PROPERTY(init_color_argb);
  JSBIND_PROPERTY(hwnd);
  JSBIND_PROPERTY(owner_pid);
}

JSBIND_CLASS(PointCoordinate) {
  JSBIND_PROPERTY(x);
  JSBIND_PROPERTY(y);
  JSBIND_PROPERTY(displayId);
}

}  // namespace ohos::adapter::window

// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_WINDOW_WINDOW_COMMON_H_
#define OHOS_ADAPTER_WINDOW_WINDOW_COMMON_H_

#include "ohos/adapter/export.h"
#include "ohos/adapter/common/constants.h"
#include <stdint.h>

namespace ohos::adapter::window {

using window_features_mask = int64_t;
using window_state_mask = int64_t;

enum WindowFeatures : int32_t {
    WINDOW_TITLE_BAR,
    WINDOW_CLOSE_BUTTON,
    WINDOW_MINIMIZE_BUTTON,
    WINDOW_MAXIMIZE_BUTTON,
    WINDOW_RESIZABLE,
    WINDOW_TOUCHABLE,
    WINDOW_HAS_SHADOW,
    WINDOW_NO_ACTIVATION,
    USE_FLOATING_WINDOW,
    USE_DARK_MODE,
};

enum WindowState : int32_t {
    WINDOW_DISABLED,
    WINDOW_NO_FOREGROUND,
    WINDOW_TOP_MOST,
    WINDOW_EXCLUDED_BY_CONTROL,
    WINDOW_MINIMIZED,
    WINDOW_MAXIMIZED,
    WINDOW_FULL_SCREEN,
    WINDOW_SPLIT_MODE,
    WINDOW_VISIABLE,
};

static inline window_features_mask features_mask_for_type(int32_t type)
{
    return ((window_features_mask)1 << type);
}

static inline bool has_feature(int32_t type, window_features_mask mask)
{
    return ((window_features_mask)1 << type) & mask;
}

static inline window_state_mask state_mask_for_type(int32_t type)
{
    return ((window_state_mask)1 << type);
}

static inline bool is_state(int32_t state, window_state_mask mask)
{
    return ((window_state_mask)1 << state) & mask;
}


}  // namespace ohos::adapter::window
#endif  // OHOS_ADAPTER_WINDOW_WINDOW_COMMON_H_

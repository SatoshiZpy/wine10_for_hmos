// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_WINDOW_WINE_FLOAT_WINDOW_H_
#define OHOS_ADAPTER_WINDOW_WINE_FLOAT_WINDOW_H_

#include "ohos/adapter/window/wine_window.h"

namespace ohos::adapter::window {

class WineFloatWindow : public WineWindow {
  public:
    WineFloatWindow();
    WineFloatWindow(WineFloatWindow&&) = delete;
    WineFloatWindow& operator=(WineFloatWindow&&) = delete;
    WineFloatWindow(const WineFloatWindow&) = delete;
    WineFloatWindow& operator=(const WineFloatWindow&) = delete;

    ~WineFloatWindow() override;

    WineFloatWindow* AsFloatWindow() override;

    int32_t OnCreateWindow() override;

    void Close() override;
    void SetBounds(const WindowRect& rect) override;
};

}  // namespace ohos::adapter::window
#endif  //OHOS_ADAPTER_WINDOW_WINE_FLOAT_WINDOW_H_
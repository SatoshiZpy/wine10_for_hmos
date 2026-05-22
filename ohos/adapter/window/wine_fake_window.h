// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_WINDOW_WINE_FAKE_WINDOW_H_
#define OHOS_ADAPTER_WINDOW_WINE_FAKE_WINDOW_H_

#include "ohos/adapter/window/wine_window.h"

namespace ohos::adapter::window {

class WineFakeWindow : public WineWindow {
  public:
    WineFakeWindow();
    WineFakeWindow(WineFakeWindow&&) = delete;
    WineFakeWindow& operator=(WineFakeWindow&&) = delete;
    WineFakeWindow(const WineFakeWindow&) = delete;
    WineFakeWindow& operator=(const WineFakeWindow&) = delete;

    ~WineFakeWindow() override;

    WineFakeWindow* AsFakeWindow() override;
};

}  // namespace ohos::adapter::window
#endif  //OHOS_ADAPTER_WINDOW_WINE_FAKE_WINDOW_H_
// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/window/wine_window.h"

#ifndef OHOS_ADAPTER_WINDOW_WINE_TOPLEVEL_WINDOW_H_
#define OHOS_ADAPTER_WINDOW_WINE_TOPLEVEL_WINDOW_H_

namespace ohos::adapter::window {

class WineToplevelWindow : public WineWindow {
  public:
    WineToplevelWindow();
    WineToplevelWindow(WineToplevelWindow&&) = delete;
    WineToplevelWindow& operator=(WineToplevelWindow&&) = delete;
    WineToplevelWindow(const WineToplevelWindow&) = delete;
    WineToplevelWindow& operator=(const WineToplevelWindow&) = delete;

    ~WineToplevelWindow() override;

    WineToplevelWindow* AsToplevelWindow() override;

    int32_t OnCreateWindow() override;

    void Show() override;
    void Hide() override;
    void Close() override;
    void SetBounds(const WindowRect& rect) override;
};

}  // namespace ohos::adapter::window
#endif  //OHOS_ADAPTER_WINDOW_WINE_TOPLEVEL_WINDOW_H_
// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_WINDOW_WINE_TOPLEVEL_WINDOW_H_
#define OHOS_ADAPTER_WINDOW_WINE_TOPLEVEL_WINDOW_H_

#include "ohos/adapter/window/wine_window.h"

namespace ohos::adapter::window {

using namespace ohos::adapter::xcomponent;

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
    void TriggerStateChanges() override;
    void UpdateStateChanges() override;

    void Show() override;
    void Hide() override;
    void Close() override;
    bool IsVisible() const override;
    void SetBounds(const WindowRect& rect) override;
};

}  // namespace ohos::adapter::window
#endif  //OHOS_ADAPTER_WINDOW_WINE_TOPLEVEL_WINDOW_H_
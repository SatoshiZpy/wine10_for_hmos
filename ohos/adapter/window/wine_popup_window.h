// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_WINDOW_WINE_POPUP_WINDOW_H_
#define OHOS_ADAPTER_WINDOW_WINE_POPUP_WINDOW_H_

#include "ohos/adapter/window/wine_window.h"

namespace ohos::adapter::window {

class WinePopupWindow : public WineWindow {
  public:
    explicit WinePopupWindow(std::shared_ptr<WineWindow> parent);
    WinePopupWindow(WinePopupWindow&&) = delete;
    WinePopupWindow& operator=(WinePopupWindow&&) = delete;
    WinePopupWindow(const WinePopupWindow&) = delete;
    WinePopupWindow& operator=(const WinePopupWindow&) = delete;

    ~WinePopupWindow() override;

    WinePopupWindow* AsPopupWindow() override;

    int32_t OnCreateWindow() override;

    void Show() override;
    void Hide() override;
    void Close() override;
    void SetBounds(const WindowRect& rect) override;
};

}  // namespace ohos::adapter::window
#endif  //OHOS_ADAPTER_WINDOW_WINE_POPUP_WINDOW_H_
// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/window/wine_popup_window.h"

#include "ohos/adapter/common/logging.h"
#include "ohos/adapter/window/adapter/sub_window_adapter.h"

namespace ohos::adapter::window {

WinePopupWindow::WinePopupWindow(std::shared_ptr<WineWindow> parent)
  : WineWindow() {
  set_parent_window(parent);
}

WinePopupWindow::~WinePopupWindow() = default;

WinePopupWindow* WinePopupWindow::AsPopupWindow()
{
  return this;
}

int32_t WinePopupWindow::OnCreateWindow()
{
  return SubWindowAdapter::GetInstance().Create(GetParam());
}

void WinePopupWindow::Show()
{
  SubWindowAdapter::GetInstance().Show(GetWidget());
}

void WinePopupWindow::Hide()
{
  SubWindowAdapter::GetInstance().Hide(GetWidget());
}

void WinePopupWindow::Close()
{
  if (type() == WindowInitType::kTooltip) {
    SubWindowAdapter::GetInstance().Destroy(GetWidget());
  } else {
    SubWindowAdapter::GetInstance().Close(GetWidget());
  }
}

void WinePopupWindow::SetBounds(const WindowRect& rect)
{
  SubWindowAdapter::GetInstance().SetBounds(GetWidget(), rect);
}

}  // namespace ohos::adapter::window

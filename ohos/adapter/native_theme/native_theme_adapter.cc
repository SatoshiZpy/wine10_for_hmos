// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/native_theme/native_theme_adapter.h"

#include "ohos/adapter/aki_hook/aki_hook.h"

namespace ohos::adapter::native_theme {

NativeThemeAdapter& NativeThemeAdapter::GetInstance() {
  static NativeThemeAdapter instance;
  return instance;
}

OhosColorMode NativeThemeAdapter::GetSystemThemeSource() {
  if (auto jsFuncSystemNativeThemeList =
        ohos::adapter::GetJSFunction("NativeThemeAdapter.GetSystemNativeTheme")) {
    return jsFuncSystemNativeThemeList->Invoke<OhosColorMode>();
  }
  return OhosColorMode::COLOR_MODE_NOT_SET;
}

void SetThemeSource(const OhosColorMode theme_source) {
  if (theme_source != OhosColorMode::COLOR_MODE_NOT_SET) {
    NativeThemeAdapter::GetInstance().NotifyThemeSourceEvent(theme_source);
  }
}

void NativeThemeAdapter::RegisterThemeSourceEvent(
    std::shared_ptr<ThemeSourceEventCallback> theme_source_event_callback) {
  theme_source_event_callback_ = theme_source_event_callback;
}

void NativeThemeAdapter::NotifyThemeSourceEvent(const OhosColorMode theme_source) {
  if (theme_source_event_callback_ != nullptr) {
    theme_source_event_callback_->OnThemeSourceChanged(theme_source);
  }
}

JSBIND_GLOBAL() {
  JSBIND_FUNCTION(SetThemeSource);
}

}  // namespace ohos::adapter::native_theme

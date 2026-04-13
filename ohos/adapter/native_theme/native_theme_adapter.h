// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_NATIVE_THEME_NATIVE_THEME_ADAPTER_H_
#define OHOS_ADAPTER_NATIVE_THEME_NATIVE_THEME_ADAPTER_H_

#include <cstdint>
#include <string>
#include "ohos/adapter/export.h"

namespace ohos::adapter::native_theme {

enum class OhosColorMode {
  COLOR_MODE_NOT_SET = -1,
  COLOR_MODE_DARK = 0,
  COLOR_MODE_LIGHT = 1
};

class ADAPTER_EXPORT_API ThemeSourceEventCallback {
  public:
    ThemeSourceEventCallback() = default;
    virtual ~ThemeSourceEventCallback() = default;
    virtual void OnThemeSourceChanged(const OhosColorMode theme_source) = 0;
};

class ADAPTER_EXPORT_API NativeThemeAdapter {
 public:
  NativeThemeAdapter(const NativeThemeAdapter&) = delete;
  NativeThemeAdapter& operator=(const NativeThemeAdapter&) = delete;
  static NativeThemeAdapter& GetInstance();

  OhosColorMode GetSystemThemeSource();
  void SetThemeSource(const OhosColorMode theme_source);
  void RegisterThemeSourceEvent(std::shared_ptr<ThemeSourceEventCallback> theme_source_event_callback);
  void NotifyThemeSourceEvent(const OhosColorMode theme_source);

 private:
  NativeThemeAdapter() = default;
  ~NativeThemeAdapter() = default;
  std::shared_ptr<ThemeSourceEventCallback> theme_source_event_callback_{nullptr};
};

}  // namespace ohos::adapter::native_theme
#endif  // OHOS_ADAPTER_NATIVE_THEME_NATIVE_THEME_ADAPTER_H_

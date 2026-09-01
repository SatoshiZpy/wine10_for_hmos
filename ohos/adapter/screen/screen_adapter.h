// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_SCREEN_SCREEN_ADAPTER_H_
#define OHOS_ADAPTER_SCREEN_SCREEN_ADAPTER_H_

#include <cstdint>
#include <string>
#include <vector>

#include "aki/jsbind.h"

#include "ohos/adapter/aki_hook/aki_hook.h"
#include "ohos/adapter/common/constants.h"
#include "ohos/adapter/export.h"

namespace ohos {
namespace adapter {

class ADAPTER_EXPORT_API ScreenAdapter {
 public:
  using DisplayChangeCallback =
      std::function<void(const std::string&, int32_t)>;

  void SetDefaultDisplay(const Display& display);
  bool GetDefaultDisplay(Display& display);
  bool GetAllDisplays(std::vector<Display>& displays);
  void RegisterDisplayMonitor();
  void RegisterCallback(DisplayChangeCallback callback);
  DisplayChangeCallback GetCallback();
  static ScreenAdapter& GetInstance();

 private:
  ScreenAdapter();

  DisplayChangeCallback callback_;
  Display default_display_;
};

}  // namespace adapter
}  // namespace ohos
#endif  // OHOS_ADAPTER_SCREEN_SCREEN_ADAPTER_H_

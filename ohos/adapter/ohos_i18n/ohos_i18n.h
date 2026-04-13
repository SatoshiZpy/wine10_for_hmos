// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_TIME_ZONE_TIME_ZONE_ADAPTER_H_
#define OHOS_ADAPTER_TIME_ZONE_TIME_ZONE_ADAPTER_H_

#include <string>

#include "ohos/adapter/export.h"

namespace ohos::adapter::ohos_i18n {
  const uint32_t kLen = 64;
  using TimeZoneChangeCallback = std::function<void(std::string&)>;
  std::string ADAPTER_EXPORT_API GetTimeZone();
  std::string ADAPTER_EXPORT_API GetLocaleLang();
  std::string ADAPTER_EXPORT_API getLocaleRegion();
  void ADAPTER_EXPORT_API RegisterTimeZoneListener(TimeZoneChangeCallback callback);
  void ADAPTER_EXPORT_API UnsubscribeTimeZoneListener();
}  // ohos::adapter::ohos_i18n
#endif  // OHOS_ADAPTER_TIME_ZONE_TIME_ZONE_ADAPTER_H_

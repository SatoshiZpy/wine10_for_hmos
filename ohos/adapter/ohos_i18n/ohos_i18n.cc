// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/ohos_i18n/ohos_i18n.h"

#include <BasicServicesKit/time_service.h>

#include "ohos/adapter/aki_hook/aki_hook.h"
#include "ohos/adapter/common/logging.h"

namespace ohos::adapter::ohos_i18n {
std::string GetTimeZone() {
  char time_zone[kLen] = "";
  TimeService_ErrCode code = OH_TimeService_GetTimeZone(time_zone, kLen);
  if (code == TimeService_ErrCode::TIMESERVICE_ERR_OK) {
    return std::string(time_zone);
  }
  LOGE("get [GetTimeZone] function fail.");
  return "";
}

std::string GetLocaleLang() {
  if (auto getLocaleLangFunc =
          ohos::adapter::GetJSFunction("OhosI18nAdapter.GetLocaleLang")) {
    return getLocaleLangFunc->Invoke<std::string>();
  }
  LOGE("get [GetLocaleLang] function fail.");
  return "";
}

std::string getLocaleRegion() {
  if (auto getLocaleRegionFunc =
          ohos::adapter::GetJSFunction("OhosI18nAdapter.getLocaleRegion")) {
    return getLocaleRegionFunc->Invoke<std::string>();
  }
  LOGE("get [getLocaleRegion] function fail.");
  return "";
}

void RegisterTimeZoneListener(TimeZoneChangeCallback callback) {
  auto func =
      ohos::adapter::GetJSFunction("OhosI18nAdapter.RegisterTimeZoneListener");
  if (func) {
    func->Invoke<void>(callback);
  }
}

void UnsubscribeTimeZoneListener() {
  auto func =
      ohos::adapter::GetJSFunction("OhosI18nAdapter.UnsubscribeTimeZoneListener");
  if (func) {
    func->Invoke<void>();
  }
}
}  // namespace ohos::adapter::ohos_i18n

// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/life_cycle/app_lifecycle.h"

#include "ohos/adapter/common/logging.h"

namespace ohos::adapter::life_cycle {

void AppLifeCycle::RegisterAppLifecycle(napi_env env, napi_value exports) {
  napi_property_descriptor desc[] = {
      {"onCreate", nullptr, AppLifeCycle::OnCreate, nullptr, nullptr, nullptr,
       napi_default, nullptr},
      {"onShow", nullptr, AppLifeCycle::OnShow, nullptr, nullptr, nullptr,
       napi_default, nullptr},
      {"onHide", nullptr, AppLifeCycle::OnHide, nullptr, nullptr, nullptr,
       napi_default, nullptr},
      {"onDestroy", nullptr, AppLifeCycle::OnDestroy, nullptr, nullptr, nullptr,
       napi_default, nullptr}};
  napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc);
}

napi_value AppLifeCycle::OnCreate(napi_env env, napi_callback_info info) {
  LOGE("AppLifeCycle::OnCreate");
  return nullptr;
}

napi_value AppLifeCycle::OnShow(napi_env env, napi_callback_info info) {
  LOGE("AppLifeCycle::OnShow");
  return nullptr;
}

napi_value AppLifeCycle::OnHide(napi_env env, napi_callback_info info) {
  LOGE("AppLifeCycle::OnHide");
  return nullptr;
}

napi_value AppLifeCycle::OnDestroy(napi_env env, napi_callback_info info) {
  LOGE("AppLifeCycle::OnDestroy");
  return nullptr;
}

}  // namespace life_cycle

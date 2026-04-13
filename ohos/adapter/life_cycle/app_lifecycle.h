// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_LIFE_CYCLE_APP_LIFECYCLE_H_
#define OHOS_ADAPTER_LIFE_CYCLE_APP_LIFECYCLE_H_

#include <napi/native_api.h>

namespace ohos::adapter::life_cycle {

class AppLifeCycle {
 public:
  AppLifeCycle() = default;
  ~AppLifeCycle() = default;

  static void RegisterAppLifecycle(napi_env env, napi_value exports);
  static napi_value OnCreate(napi_env env, napi_callback_info info);
  static napi_value OnShow(napi_env env, napi_callback_info info);
  static napi_value OnHide(napi_env env, napi_callback_info info);
  static napi_value OnDestroy(napi_env env, napi_callback_info info);
};

}  // namespace life_cycle

#endif  // OHOS_ADAPTER_LIFE_CYCLE_APP_LIFECYCLE_H_

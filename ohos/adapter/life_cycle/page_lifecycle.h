// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_LIFE_CYCLE_PAGE_LIFECYCLE_H_
#define OHOS_ADAPTER_LIFE_CYCLE_PAGE_LIFECYCLE_H_

#include <napi/native_api.h>

namespace ohos::adapter::life_cycle {

class PageLifeCycle {
 public:
  PageLifeCycle() = default;
  ~PageLifeCycle() = default;

  static void RegisterPageLifecycle(napi_env env, napi_value exports);
  static napi_value ToAppear(napi_env env, napi_callback_info info);
  static napi_value ToDisappear(napi_env env, napi_callback_info info);
  static napi_value OnPageShow(napi_env env, napi_callback_info info);
  static napi_value OnPageHide(napi_env env, napi_callback_info info);
};

}  // namespace life_cycle

#endif  // OHOS_ADAPTER_LIFE_CYCLE_PAGE_LIFECYCLE_H_

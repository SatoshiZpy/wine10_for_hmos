// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/life_cycle/page_lifecycle.h"

#include "ohos/adapter/common/logging.h"

namespace ohos::adapter::life_cycle {

void PageLifeCycle::RegisterPageLifecycle(napi_env env, napi_value exports) {
  napi_property_descriptor desc[] = {
      {"aboutToAppear", nullptr, PageLifeCycle::ToAppear, nullptr, nullptr,
       nullptr, napi_default, nullptr},
      {"aboutToDisappear", nullptr, PageLifeCycle::ToDisappear, nullptr,
       nullptr, nullptr, napi_default, nullptr},
      {"onPageShow", nullptr, PageLifeCycle::OnPageShow, nullptr, nullptr,
       nullptr, napi_default, nullptr},
      {"onPageHide", nullptr, PageLifeCycle::OnPageHide, nullptr, nullptr,
       nullptr, napi_default, nullptr}};
  napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc);
}

napi_value PageLifeCycle::ToAppear(napi_env env, napi_callback_info info) {
  LOGI("PageLifeCycle::ToAppear");
  return nullptr;
}

napi_value PageLifeCycle::ToDisappear(napi_env env, napi_callback_info info) {
  LOGI("PageLifeCycle::ToDisappear");
  return nullptr;
}

napi_value PageLifeCycle::OnPageShow(napi_env env, napi_callback_info info) {
  LOGI("PageLifeCycle::OnPageShow");
  return nullptr;
}

napi_value PageLifeCycle::OnPageHide(napi_env env, napi_callback_info info) {
  LOGI("PageLifeCycle::OnPageHide");
  return nullptr;
}

}  // namespace ohos::adapter::life_cycle

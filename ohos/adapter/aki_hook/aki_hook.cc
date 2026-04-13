// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/aki_hook/aki_hook.h"

#include "hitrace/trace.h"

#include "aki/jsbind.h"
#include "aki/value/napi/js_function.h"

namespace ohos {
namespace adapter {

const JSFunction* GetJSFunction(const std::string& func_name) {
  auto jsfunc = aki::JSBind::GetJSFunction(func_name);
  if (jsfunc == nullptr) {
    LOGE("GetJSFunction %{public}s failed", func_name.c_str());
    return nullptr;
  }
  return new JSFunction(jsfunc, func_name);
}

AkiHookLogger::AkiHookLogger(const std::string& func_name)
    : function_name_(func_name) {
  LOGI("JSFunction call %{public}s start", function_name_.c_str());
}

AkiHookLogger::~AkiHookLogger() {
  LOGI("JSFunction call %{public}s end", function_name_.c_str());
}

}  // namespace adapter
}  // namespace ohos

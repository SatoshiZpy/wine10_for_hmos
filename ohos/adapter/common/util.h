// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_COMMON_UTIL_H_
#define OHOS_ADAPTER_COMMON_UTIL_H_

#include <string>
#include "aki/jsbind.h"
#include "ohos/adapter/common/logging.h"

template <typename R, typename... P>
R GetAkiResult(const std::string& func_name,
               P... args,
               const R& default_value) {
  auto aki_func = ohos::adapter::GetJSFunction(func_name);
  if (!aki_func) {
    LOGE("GetAkiResult js binding error: %{public}s function undefined.",
         func_name.c_str());
    return default_value;
  }
  return aki_func->Invoke<R>(std::forward<P>(args)...);
}

template <typename R, typename... P>
R GetAkiResultAsync(const std::string& func_name,
                    P... args,
                    const R& default_value) {
  auto aki_func = ohos::adapter::GetJSFunction(func_name);
  if (!aki_func) {
    LOGE("GetAkiResult js binding error: %{public}s function undefined.",
         func_name.c_str());
    return default_value;
  }

  std::promise<R> result_promise;
  std::function<void(R)> callback = [&result_promise](R result) {
    result_promise.set_value(result);
  };
  aki_func->Invoke<void>(std::forward<P>(args)..., callback);
  return result_promise.get_future().get();
}

template <class T>
T GetValue(aki::Value aki_value, T default_value) {
  if (!(aki_value.IsUndefined() || aki_value.IsNull())) {
    return aki_value.As<T>();
  }
  return default_value;
}

uint32_t GetArrayLength(const aki::Value& array) {
  uint32_t length = 0;
  if (!array.IsArray()) {
    return length;
  }

  napi_get_array_length(aki::JSBind::GetScopedEnv(), array.GetHandle(),
                        &length);
  return length;
}

#endif  // OHOS_ADAPTER_COMMON_UTIL_H_

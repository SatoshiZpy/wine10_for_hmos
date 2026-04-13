// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_AKI_HOOK_AKI_HOOK_H_
#define OHOS_ADAPTER_AKI_HOOK_AKI_HOOK_H_

#include <utility>
#include "aki/jsbind.h"
#include "ohos/adapter/common/logging.h"
#include "ohos/adapter/common/trace.h"
namespace ohos {
namespace adapter {
class AkiHookLogger {
 public:
  explicit AkiHookLogger(const std::string& func_name);

  ~AkiHookLogger();

 private:
  std::string function_name_;
};

class JSFunction {
 public:
  JSFunction(const aki::JSFunction* func, const std::string& function_name)
      : func_(func), function_name_(function_name) {}

  template <typename R, typename... P>
  R Invoke(P... args) const {
    TRACE_EVENT_0(function_name_);
    return func_->Invoke<R>(std::forward<P>(args)...);
  }

  template <typename R, typename... P>
  std::future<R> InvokeAsync(P... args) const {
    TRACE_EVENT_0(function_name_);
    return func_->InvokeAsync<R>(std::forward<P>(args)...);
  }

 private:
  const aki::JSFunction* func_;
  std::string function_name_;
};

const JSFunction* GetJSFunction(const std::string& func_name);

}  // namespace adapter
}  // namespace ohos
#endif  // OHOS_ADAPTER_AKI_HOOK_AKI_HOOK_H_

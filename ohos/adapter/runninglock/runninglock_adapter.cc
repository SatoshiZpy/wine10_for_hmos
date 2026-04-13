// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/runninglock/runninglock_adapter.h"

#include "ohos/adapter/aki_hook/aki_hook.h"

namespace ohos::adapter::runninglock {

// static
RunningLockAdapter& RunningLockAdapter::GetInstance() {
  static RunningLockAdapter instance;
  return instance;
}

void RunningLockAdapter::Start() {
  auto js_func =
      ohos::adapter::GetJSFunction("RunningLock.Start");
   if (js_func) {
    js_func->InvokeAsync<void>();
  }
}

void RunningLockAdapter::Stop() {
  auto js_func =
      ohos::adapter::GetJSFunction("RunningLock.Stop");
  if (js_func) {
    js_func->Invoke<void>();
  }
}

}  // namespace ohos::adapter::runninglock

// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_RUNNINGLOCK_RUNNINGLOCK_ADAPTER_H_
#define OHOS_ADAPTER_RUNNINGLOCK_RUNNINGLOCK_ADAPTER_H_


#include "ohos/adapter/export.h"

namespace ohos::adapter::runninglock {

class ADAPTER_EXPORT_API RunningLockAdapter {
 private:
  RunningLockAdapter() = default;
  ~RunningLockAdapter() = default;
  RunningLockAdapter(const RunningLockAdapter&) = delete;
  RunningLockAdapter& operator=(const RunningLockAdapter&) = delete;

 public:
  static RunningLockAdapter& GetInstance();

  void Start();
  void Stop();
};

}  // namespace ohos::adapter::runningLock
#endif  // OHOS_ADAPTER_RUNNINGLOCK_RUNNINGLOCK_ADAPTER_H_

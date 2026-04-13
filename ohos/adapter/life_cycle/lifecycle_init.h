// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_LIFE_CYCLE_LIFECYCLE_INITIALIZER_H_
#define OHOS_ADAPTER_LIFE_CYCLE_LIFECYCLE_INITIALIZER_H_

#include <napi/native_api.h>

#include "ohos/adapter/export.h"

namespace ohos::adapter::life_cycle {

void ADAPTER_EXPORT_API Register(napi_env env, napi_value exports);

}  // namespace ohos::adapter::life_cycle

#endif  // OHOS_ADAPTER_LIFE_CYCLE_LIFECYCLE_INITIALIZER_H_

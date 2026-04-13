// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ADAPTER_WEB_ENTRY_INIT_H
#define ADAPTER_WEB_ENTRY_INIT_H

#include <node_api.h>

namespace ohos::adapter::web_entry {
void Register(napi_env env, napi_value exports);
}
#endif  // ADAPTER_WEB_ENTRY_INIT_H

// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/export.h"

#include "ohos/adapter/multiprocess/isolate_process/native_child_process_library.h"

extern "C" {
void ADAPTER_EXPORT_API
IsolateMain(NativeChildProcess_Args args);
}

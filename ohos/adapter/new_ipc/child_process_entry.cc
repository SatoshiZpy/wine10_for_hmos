// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/new_ipc/child_process_entry.h"

#include <unistd.h>

#include "ohos/adapter/common/logging.h"
#include "ohos/adapter/new_ipc/ipc_adapter.h"

using namespace ohos::adapter;

int StartIpcServerEntry(void)
{
    LOGI("StartIpcServerEntry pid=%{public}d", static_cast<int>(getpid()));

    if (!new_ipc::IpcServer::Instance().Start()) {
        LOGE("StartIpcServerEntry: start server failed");
        return -1;
    }

    return 0;
}

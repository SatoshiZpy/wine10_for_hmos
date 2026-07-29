// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/new_ipc/child_process_entry.h"

#include <unistd.h>

#include "ohos/adapter/common/constants.h"
#include "ohos/adapter/common/logging.h"
#include "ohos/adapter/multiprocess/app_spawn_communication.h"
#include "ohos/adapter/new_ipc/ipc_adapter.h"
#include "ohos/adapter/web_entry/web_entry.h"

using namespace ohos::adapter;

int NewIpcChildProcessEntry(const char* ipc_socket_path)
{
    LOGI("NewIpcChildProcessEntry pid=%{public}d, socket=%{public}s",
         getpid(), ipc_socket_path ? ipc_socket_path : "null");

    if (!ipc_socket_path || ipc_socket_path[0] == '\0') {
        LOGE("NewIpcChildProcessEntry: ipc_socket_path is empty");
        return -1;
    }

    if (!new_ipc::ChildProcess::Instance().ConnectToParent(ipc_socket_path)) {
        LOGE("NewIpcChildProcessEntry: ConnectToParent failed");
        return -1;
    }

    std::vector<std::pair<int, int>> ipc_fds;
    new_ipc::ChildProcess::Instance().GetFdIdsRemap(ipc_fds);
    multiprocess::AppSpawnCommunication::SetFdIdsRemap(ipc_fds);

    LOGI("NewIpcChildProcessEntry IPC established, %{public}zu fd remap entries",
         ipc_fds.size());

    return 0;
}

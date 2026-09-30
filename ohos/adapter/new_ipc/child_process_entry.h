// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_NEW_IPC_CHILD_PROCESS_ENTRY_H_
#define OHOS_ADAPTER_NEW_IPC_CHILD_PROCESS_ENTRY_H_

#include "ohos/adapter/export.h"

#ifdef __cplusplus
extern "C" {
#endif

// Called at startup of every wine process (the main process included):
// creates this process's IPC server socket, named after the pid, and keeps
// listening for other processes to connect when they embed a window of this
// process.  Independent of the spawn chain (which still goes through
// ipc_lite); connections are established on demand.
// Returns 0 on success.
int ADAPTER_EXPORT_API StartIpcServerEntry(void);

#ifdef __cplusplus
}
#endif

#endif  // OHOS_ADAPTER_NEW_IPC_CHILD_PROCESS_ENTRY_H_

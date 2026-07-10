// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_NEW_IPC_CHILD_PROCESS_ENTRY_H_
#define OHOS_ADAPTER_NEW_IPC_CHILD_PROCESS_ENTRY_H_

#include "ohos/adapter/export.h"

#ifdef __cplusplus
extern "C" {
#endif

// Entry function for child process started by new_ipc createProcess.
// ETS code calls this function with ipc_socket_path to establish IPC.
// ipc_socket_path: UDS path for connecting to parent process
// Returns process exit code.
int ADAPTER_EXPORT_API NewIpcChildProcessEntry(const char* ipc_socket_path);

#ifdef __cplusplus
}
#endif

#endif  // OHOS_ADAPTER_NEW_IPC_CHILD_PROCESS_ENTRY_H_

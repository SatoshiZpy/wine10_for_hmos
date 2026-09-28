// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_NEW_IPC_CHILD_PROCESS_ENTRY_H_
#define OHOS_ADAPTER_NEW_IPC_CHILD_PROCESS_ENTRY_H_

#include "ohos/adapter/export.h"

#ifdef __cplusplus
extern "C" {
#endif

// 每个 wine 进程（含主进程）启动时调用：以 pid 命名创建本进程的 IPC
// 服务端 socket 并常驻监听，等待其他进程在嵌入本进程窗口时发起连接。
// spawn 链路与此无关（仍走 ipc_lite）；连接按需建立。
// Returns 0 on success.
int ADAPTER_EXPORT_API StartIpcServerEntry(void);

#ifdef __cplusplus
}
#endif

#endif  // OHOS_ADAPTER_NEW_IPC_CHILD_PROCESS_ENTRY_H_

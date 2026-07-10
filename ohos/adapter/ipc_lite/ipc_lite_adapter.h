// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_IPC_LITE_IPC_LITE_ADAPTER_H_
#define OHOS_ADAPTER_IPC_LITE_IPC_LITE_ADAPTER_H_

#include <cstdint>
#include <string>
#include <vector>

namespace ohos::adapter::ipc_lite {

enum IpcLiteMessageType : int32_t {
    MSG_SET_PROCESS_INFO = 1,
    MSG_PID_RESPONSE     = 2,
};

bool DeserializeProcessInfo(
    const uint8_t* data, size_t len,
    std::vector<std::string>& commands,
    std::vector<int32_t>& remap_ids);

class IpcLiteHelper {
public:
    static int CreateProcess(const std::vector<std::string>& commands,
                             const std::vector<std::pair<int, int>>& fds);
    static void KillProcess();
};

class ChildProcessLiteHost {
public:
    static ChildProcessLiteHost& Instance();

    bool StartServer(const std::string& socket_path);
    int AcceptAndSetup(const std::vector<std::string>& commands,
                       const std::vector<std::pair<int, int>>& fds);
    void Close();

private:
    ChildProcessLiteHost() = default;
    ~ChildProcessLiteHost() = default;

    int server_fd_ = -1;
    std::string server_socket_path_;
    bool server_started_ = false;
};

}  // namespace ohos::adapter::ipc_lite

#endif  // OHOS_ADAPTER_IPC_LITE_IPC_LITE_ADAPTER_H_

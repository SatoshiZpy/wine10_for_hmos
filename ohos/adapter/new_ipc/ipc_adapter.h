// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_NEW_IPC_IPC_ADAPTER_H_
#define OHOS_ADAPTER_NEW_IPC_IPC_ADAPTER_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "ohos/adapter/new_ipc/ipc_channel.h"
#include "ohos/adapter/new_ipc/ipc_channel_manager.h"
#include "ohos/adapter/new_ipc/ipc_proxy_stub.h"
#include "ohos/adapter/new_ipc/ipc_types.h"

namespace ohos::adapter::new_ipc {

class ProcessManagerProxy;

class ChildProcess {
public:
    static ChildProcess& Instance();

    bool ConnectToParent(const std::string& socket_path);

    int GetDisplay(DisplayParam& display);
    int CreateProcess(const std::vector<std::string>& commands,
                      const std::vector<std::pair<int, int>>& fds);
    int KillProcess(int32_t pid);
    int OpenUrl(const std::string& url);
    int GetFdIdsRemap(std::vector<std::pair<int, int>>& fds);
    void SetFdIdsRemap(const std::vector<std::pair<int, int>>& fds);

private:
    ChildProcess();
    ~ChildProcess();

    std::unique_ptr<IpcChannel> channel_;
    std::unique_ptr<ProcessManagerProxy> proxy_;
    ChildProcessStub stub_;
    std::vector<std::pair<int, int>> fd_ids_remap_;
    bool connected_ = false;
};

class IpcHelper {
public:
    static int CreateProcess(const std::vector<std::string>& commands,
                             const std::vector<std::pair<int, int>>& fds);
    static int KillProcess(int32_t pid);
    static int CreateWindow(const WindowParam& param);
    static int UpdateWindow(int32_t widget_id, const WindowParam& param);
    static int SetWindowBounds(int32_t widget_id, const WindowRect& rect);
    static int SetWindowVisible(int32_t widget_id, bool visible);
    static int CloseWindow(int32_t widget_id);
    static int SetWindowTitle(int32_t widget_id, const std::string& title);
    static int GetDisplay(DisplayParam& param);
    static int OpenUrl(const std::string& url);
};

class ChildProcessHost {
public:
    static ChildProcessHost& Instance();

    bool StartServer(const std::string& socket_path);
    int32_t AcceptNewChild();
    bool SetProcessInfo(int32_t channel_id,
                        const std::vector<std::string>& commands,
                        const std::vector<std::pair<int, int>>& fds);
    bool SetWindowInfo(int32_t channel_id, int32_t widget_id, const WindowAttr& attr);
    bool SendInputEvent(int32_t channel_id, const std::vector<int8_t>& event_data);
    IpcChannelManager* GetManager() { return manager_.get(); }
    void CloseAll();

private:
    ChildProcessHost();
    ~ChildProcessHost();

    std::unique_ptr<IpcChannelManager> manager_;
    ProcessManagerStub stub_;
    bool server_started_ = false;
    std::string server_socket_path_;
};

}  // namespace ohos::adapter::new_ipc

#endif  // OHOS_ADAPTER_NEW_IPC_IPC_ADAPTER_H_

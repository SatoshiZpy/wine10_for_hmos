// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_NEW_IPC_IPC_CHANNEL_MANAGER_H_
#define OHOS_ADAPTER_NEW_IPC_IPC_CHANNEL_MANAGER_H_

#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "ohos/adapter/common/mutex.h"
#include "ohos/adapter/new_ipc/ipc_channel.h"

namespace ohos::adapter::new_ipc {

constexpr int32_t IPC_CHANNEL_ID_PARENT = -1;
constexpr int32_t IPC_CHANNEL_ID_UNKNOWN = 0;

struct IpcManagerMessage {
    int32_t channel_id;
    IpcMessage msg;
};

using IpcManagerCallback = std::function<void(const IpcManagerMessage& msg)>;

class IpcChannelManager {
public:
    IpcChannelManager();
    ~IpcChannelManager();

    bool StartServer(const std::string& socket_path);
    int32_t AcceptNewChannel();
    bool ConnectToServer(const std::string& socket_path);

    int32_t GetChannelCount();
    std::vector<int32_t> GetChannelIds();
    IpcChannel* GetChannel(int32_t channel_id);

    bool SendTo(int32_t channel_id, const SharedMemory& shm);
    bool Broadcast(const SharedMemory& shm);
    bool SendControlTo(int32_t channel_id, IpcMessageType type, const void* data = nullptr, uint32_t length = 0);

    void RemoveChannel(int32_t channel_id);
    void CloseAll();

    void SetCallback(IpcManagerCallback callback);
    bool StartListening();
    void StopListening();

    int32_t GetParentChannelId() const;

private:
    int32_t AllocateChannelId();
    void AcceptLoop();
    void ListenLoopForChannel(int32_t channel_id);
    void DispatchMessage(int32_t channel_id, const IpcMessage& msg);

    DomainSocket server_socket_;
    std::map<int32_t, std::shared_ptr<IpcChannel>> channels_;
    IpcManagerCallback callback_;
    Mutex mutex_;
    int32_t next_channel_id_ = 1;
    std::thread accept_thread_;
    std::atomic<bool> accepting_{false};
    std::atomic<bool> listening_{false};
    std::string server_socket_path_;
};

}  // namespace ohos::adapter::new_ipc

#endif  // OHOS_ADAPTER_NEW_IPC_IPC_CHANNEL_MANAGER_H_
// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_NEW_IPC_IPC_CHANNEL_H_
#define OHOS_ADAPTER_NEW_IPC_IPC_CHANNEL_H_

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "ohos/adapter/common/mutex.h"
#include "ohos/adapter/new_ipc/domain_socket.h"
#include "ohos/adapter/new_ipc/shared_memory.h"

namespace ohos::adapter::new_ipc {

enum class IpcChannelRole : uint8_t {
    SERVER = 0,
    CLIENT = 1,
};

enum class IpcChannelState : uint8_t {
    CLOSED      = 0,
    CONNECTING  = 1,
    CONNECTED   = 2,
    ERROR       = 3,
};

struct IpcMessage {
    IpcMessageType type;
    std::shared_ptr<SharedMemory> shm;
    std::vector<uint8_t> payload;
    std::vector<int> fds;
};

using IpcChannelCallback = std::function<void(const IpcMessage& msg)>;

class IpcChannel {
public:
    IpcChannel();
    ~IpcChannel();

    bool ConnectToServer(const std::string& socket_path);
    bool ConnectWithFd(int fd);

    void Close();

    bool ExchangeShmFd();
    bool SendRequest();
    bool SendRequestWithFds(const std::vector<int>& fds);
    bool WaitForReply();

    bool SendAck();
    bool SendReply();

    std::shared_ptr<SharedMemory> SendShmFd(const SharedMemory& shm);
    std::shared_ptr<SharedMemory> ReceiveShmFd();

    bool SendControlMessage(IpcMessageType type,
                             const void* data = nullptr,
                             uint32_t length = 0);

    void SetCallback(IpcChannelCallback callback);
    void SetRequestHandler(IpcChannelCallback handler);
    bool StartListening();
    void StopListening();

    IpcChannelState GetState() const;
    IpcChannelRole GetRole() const;
    int GetSocketFd() const;

    SharedMemory* GetShm();

private:
    bool SendShmFdInternal(const SharedMemory& shm, IpcMessageType type);
    std::shared_ptr<SharedMemory> ReceiveShmFdInternal(IpcMessageType expected_type);
    void ListenLoop();
    void DispatchMessage(const IpcMessage& msg);
    void HandleSyncRequest(const IpcMessage& msg);

    DomainSocket socket_;
    std::shared_ptr<SharedMemory> shm_;
    IpcChannelRole role_ = IpcChannelRole::CLIENT;
    IpcChannelState state_ = IpcChannelState::CLOSED;
    IpcChannelCallback callback_;
    IpcChannelCallback request_handler_;
    Mutex mutex_;
    std::thread listen_thread_;
    std::atomic<bool> listening_{false};
    bool shm_exchanged_ = false;
    /* 本端最近一次请求写完时的 shm 游标：应答从该位置追加，
     * WaitForReply 成功后把读游标拨到这里 */
    uint32_t sync_request_end_ = 0;
    std::mutex reply_std_mutex_;
    std::condition_variable reply_cv_;
    bool reply_ready_ = false;
};

}  // namespace ohos::adapter::new_ipc

#endif  // OHOS_ADAPTER_NEW_IPC_IPC_CHANNEL_H_

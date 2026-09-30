// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_NEW_IPC_DOMAIN_SOCKET_H_
#define OHOS_ADAPTER_NEW_IPC_DOMAIN_SOCKET_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "ohos/adapter/common/mutex.h"
#include "ohos/adapter/common/unix_socket_utils.h"

namespace ohos::adapter::new_ipc {

enum class IpcMessageType : uint8_t {
    SHM_FD       = 1,
    CONTROL      = 2,
    DATA         = 3,
    ACK          = 4,
    SYNC_REQUEST      = 5,
    SYNC_REPLY        = 6,
    DISCONNECT        = 7,
    SYNC_REQUEST_FDS  = 8,
};

struct IpcMessageHeader {
    IpcMessageType type;
    uint8_t reserved;
    uint16_t padding;
    uint32_t payload_length;
};

constexpr uint32_t IPC_MESSAGE_HEADER_SIZE = sizeof(IpcMessageHeader);
constexpr uint32_t IPC_MAX_PAYLOAD_SIZE = 4096;

class DomainSocket {
public:
    DomainSocket();
    ~DomainSocket();

    bool Listen(const std::string& socket_path);
    int Accept();
    bool Connect(const std::string& socket_path);
    bool AttachFd(int fd);

    bool SendMsg(const void* data, size_t length, const std::vector<int>& fds = {});
    bool SendFds(const std::vector<int>& fds);
    int RecvMsg(void* buffer, size_t length, std::vector<int>& fds);
    bool RecvFds(std::vector<int>& fds, size_t expected_count);

    bool SendControlMessage(IpcMessageType type, const void* data = nullptr, uint32_t length = 0);
    bool RecvControlMessage(IpcMessageType& type, void* data = nullptr, uint32_t* length = nullptr);

    void Close();
    /* Interrupt a thread blocked in accept()/recvmsg(): shutdown(2) acts
     * on the open file description itself (thread-safe); close() does not
     * wake calls already blocked in the kernel.  Does not take the
     * send/recv mutexes -- the caller may be blocked holding them. */
    void Shutdown();

    int GetFd() const { return fd_; }
    int GetListenFd() const { return listen_fd_; }
    bool IsConnected() const { return connected_; }

private:
    bool SetSocketTimeout(int fd, int timeout_ms);

    int fd_ = -1;
    int listen_fd_ = -1;
    bool connected_ = false;
    bool is_server_ = false;
    std::string socket_path_;
    /* Separate send/recv locks: ListenLoop's recvmsg holds its lock while
     * blocking for peer data, so sharing one lock with sendmsg would
     * deadlock every synchronous SendRequest (unix sockets are full
     * duplex, the two directions do not interfere).  Lock order is always
     * send -> recv (only Close holds both). */
    Mutex send_mutex_;
    Mutex recv_mutex_;
};

}  // namespace ohos::adapter::new_ipc

#endif  // OHOS_ADAPTER_NEW_IPC_DOMAIN_SOCKET_H_
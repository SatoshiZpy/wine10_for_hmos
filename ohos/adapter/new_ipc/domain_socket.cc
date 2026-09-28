// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/new_ipc/domain_socket.h"

#include <cerrno>
#include <cstring>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include "ohos/adapter/common/logging.h"
#include "ohos/adapter/common/unix_socket_utils.h"

namespace ohos::adapter::new_ipc {

DomainSocket::DomainSocket() = default;

DomainSocket::~DomainSocket()
{
    Close();
}

bool DomainSocket::Listen(const std::string& socket_path)
{
    MutexLock lock(send_mutex_);
    int fd = common::UnixSocketUtils::CreateServerSocket(socket_path);
    if (fd < 0) return false;

    listen_fd_ = fd;
    is_server_ = true;
    socket_path_ = socket_path;
    LOGI("DomainSocket::Listen path=%{public}s, listen_fd=%{public}d",
         socket_path.c_str(), listen_fd_);
    return true;
}

int DomainSocket::Accept()
{
    MutexLock lock(send_mutex_);
    if (listen_fd_ < 0) {
        LOGE("DomainSocket::Accept not listening");
        return -1;
    }

    int client_fd = accept(listen_fd_, nullptr, nullptr);
    if (client_fd < 0) {
        LOGE("DomainSocket::Accept accept failed, errno=%{public}d", errno);
        return -1;
    }

    LOGI("DomainSocket::Accept client_fd=%{public}d", client_fd);
    return client_fd;
}

bool DomainSocket::AttachFd(int fd)
{
    MutexLock lock(send_mutex_);
    if (fd < 0) {
        LOGE("DomainSocket::AttachFd invalid fd %{public}d", fd);
        return false;
    }
    if (fd_ >= 0) {
        LOGE("DomainSocket::AttachFd already has fd");
        return false;
    }
    fd_ = dup(fd);
    if (fd_ < 0) {
        LOGE("DomainSocket::AttachFd dup failed, errno=%{public}d", errno);
        return false;
    }
    connected_ = true;
    LOGI("DomainSocket::AttachFd fd=%{public}d → fd_=%{public}d", fd, fd_);
    return true;
}

bool DomainSocket::Connect(const std::string& socket_path)
{
    MutexLock lock(send_mutex_);
    int fd = common::UnixSocketUtils::CreateClientSocket(socket_path);
    if (fd < 0) return false;

    fd_ = fd;
    is_server_ = false;
    connected_ = true;
    socket_path_ = socket_path;
    LOGI("DomainSocket::Connect path=%{public}s, fd=%{public}d",
         socket_path.c_str(), fd_);
    return true;
}

bool DomainSocket::SendMsg(const void* data, size_t length, const std::vector<int>& fds)
{
    MutexLock lock(send_mutex_);
    if (fd_ < 0 || !connected_) {
        LOGE("DomainSocket::SendMsg not connected");
        return false;
    }
    return common::UnixSocketUtils::SendMsgWithFds(fd_, data, length, fds);
}

bool DomainSocket::SendFds(const std::vector<int>& fds)
{
    if (fds.empty()) {
        LOGE("DomainSocket::SendFds empty fds list");
        return false;
    }
    char dummy = 'F';
    return SendMsg(&dummy, 1, fds);
}

int DomainSocket::RecvMsg(void* buffer, size_t length, std::vector<int>& fds)
{
    MutexLock lock(recv_mutex_);
    if (fd_ < 0 || !connected_) {
        LOGE("DomainSocket::RecvMsg not connected");
        return -1;
    }
    ssize_t ret = common::UnixSocketUtils::RecvMsgWithFdsAndPid(fd_, buffer, length, fds);
    return static_cast<int>(ret);
}

bool DomainSocket::RecvFds(std::vector<int>& fds, size_t expected_count)
{
    char buffer[1];
    int ret = RecvMsg(buffer, 1, fds);
    if (ret < 0) {
        LOGE("DomainSocket::RecvFds RecvMsg failed");
        return false;
    }
    if (fds.size() != expected_count) {
        LOGE("DomainSocket::RecvFds count mismatch, expected=%{public}zu, got=%{public}zu",
             expected_count, fds.size());
        for (int fd : fds) close(fd);
        fds.clear();
        return false;
    }
    return true;
}

bool DomainSocket::SendControlMessage(IpcMessageType type, const void* data, uint32_t length)
{
    MutexLock lock(send_mutex_);
    if (fd_ < 0 || !connected_) return false;

    IpcMessageHeader header;
    header.type = type;
    header.reserved = 0;
    header.padding = 0;
    header.payload_length = length;

    struct msghdr msg = {};
    struct iovec iov[2];
    iov[0].iov_base = &header;
    iov[0].iov_len = IPC_MESSAGE_HEADER_SIZE;
    if (length > 0 && data) {
        iov[1].iov_base = const_cast<void*>(data);
        iov[1].iov_len = length;
        msg.msg_iov = iov;
        msg.msg_iovlen = 2;
    } else {
        msg.msg_iov = iov;
        msg.msg_iovlen = 1;
    }

    ssize_t ret = sendmsg(fd_, &msg, MSG_NOSIGNAL);
    if (ret < 0) {
        LOGE("DomainSocket::SendControlMessage sendmsg failed, errno=%{public}d", errno);
        return false;
    }
    return true;
}

bool DomainSocket::RecvControlMessage(IpcMessageType& type, void* data, uint32_t* length)
{
    MutexLock lock(recv_mutex_);
    if (fd_ < 0 || !connected_) return false;

    IpcMessageHeader header;
    struct msghdr msg = {};
    struct iovec iov;
    iov.iov_base = &header;
    iov.iov_len = IPC_MESSAGE_HEADER_SIZE;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    ssize_t ret = recvmsg(fd_, &msg, 0);
    if (ret < static_cast<ssize_t>(IPC_MESSAGE_HEADER_SIZE)) {
        LOGE("DomainSocket::RecvControlMessage recvmsg header failed, errno=%{public}d", errno);
        return false;
    }

    type = header.type;
    if (length) *length = header.payload_length;

    if (header.payload_length > 0 && data) {
        uint32_t to_read = header.payload_length > IPC_MAX_PAYLOAD_SIZE
                           ? IPC_MAX_PAYLOAD_SIZE : header.payload_length;
        ssize_t data_ret = recv(fd_, data, to_read, MSG_WAITALL);
        if (data_ret < 0) {
            LOGE("DomainSocket::RecvControlMessage recv data failed, errno=%{public}d", errno);
            return false;
        }
        if (length) *length = static_cast<uint32_t>(data_ret);
    }
    return true;
}

void DomainSocket::Close()
{
    MutexLock send_lock(send_mutex_);
    MutexLock lock(recv_mutex_);
    if (fd_ >= 0) {
        close(fd_);
        fd_ = -1;
    }
    if (listen_fd_ >= 0) {
        close(listen_fd_);
        listen_fd_ = -1;
    }
    connected_ = false;
    if (!socket_path_.empty() && is_server_) {
        common::UnixSocketUtils::DeleteSocketFile(socket_path_);
    }
}

bool DomainSocket::SetSocketTimeout(int fd, int timeout_ms)
{
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) return false;
    if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) return false;
    return true;
}

}  // namespace ohos::adapter::new_ipc
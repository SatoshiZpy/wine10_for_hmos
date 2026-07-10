// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/common/unix_socket_utils.h"

#include <cerrno>
#include <cstring>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <unistd.h>

#include "ohos/adapter/common/logging.h"

namespace ohos::adapter::common {

int UnixSocketUtils::CreateServerSocket(const std::string& path)
{
    DeleteSocketFile(path);

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd == -1) {
        LOGE("UnixSocketUtils::CreateServerSocket socket failed, errno=%{public}d", errno);
        return -1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);

    if (bind(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == -1) {
        LOGE("UnixSocketUtils::CreateServerSocket bind failed, errno=%{public}d", errno);
        close(fd);
        return -1;
    }

    if (listen(fd, 5) == -1) {
        LOGE("UnixSocketUtils::CreateServerSocket listen failed, errno=%{public}d", errno);
        close(fd);
        return -1;
    }

    LOGI("UnixSocketUtils::CreateServerSocket path=%{public}s, fd=%{public}d", path.c_str(), fd);
    return fd;
}

int UnixSocketUtils::CreateClientSocket(const std::string& path)
{
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd == -1) {
        LOGE("UnixSocketUtils::CreateClientSocket socket failed, errno=%{public}d", errno);
        return -1;
    }

    EnableReceiveProcessId(fd);

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);

    if (connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == -1) {
        LOGE("UnixSocketUtils::CreateClientSocket connect failed, errno=%{public}d", errno);
        close(fd);
        return -1;
    }

    LOGI("UnixSocketUtils::CreateClientSocket path=%{public}s, fd=%{public}d", path.c_str(), fd);
    return fd;
}

bool UnixSocketUtils::EnableReceiveProcessId(int fd)
{
    const int enable = 1;
    return setsockopt(fd, SOL_SOCKET, SO_PASSCRED, &enable, sizeof(enable)) == 0;
}

bool UnixSocketUtils::SendMsgWithFds(int fd,
                                     const void* data,
                                     size_t length,
                                     const std::vector<int>& fds)
{
    struct msghdr msg = {};
    struct iovec iov;
    iov.iov_base = const_cast<void*>(data);
    iov.iov_len = length;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    std::vector<char> control_buf;
    if (!fds.empty()) {
        size_t control_len = CMSG_SPACE(sizeof(int) * fds.size());
        control_buf.resize(control_len);
        msg.msg_control = control_buf.data();
        msg.msg_controllen = static_cast<socklen_t>(control_len);

        struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;
        cmsg->cmsg_len = static_cast<socklen_t>(CMSG_LEN(sizeof(int) * fds.size()));
        memcpy(CMSG_DATA(cmsg), fds.data(), sizeof(int) * fds.size());
        msg.msg_controllen = cmsg->cmsg_len;
    }

    ssize_t ret = sendmsg(fd, &msg, MSG_NOSIGNAL);
    if (ret < 0) {
        LOGE("UnixSocketUtils::SendMsgWithFds sendmsg failed, errno=%{public}d", errno);
        return false;
    }
    if (static_cast<size_t>(ret) != length) {
        LOGE("UnixSocketUtils::SendMsgWithFds partial send, expected=%{public}zu, sent=%{public}zd",
             length, ret);
        return false;
    }
    return true;
}

ssize_t UnixSocketUtils::RecvMsgWithFdsAndPid(int fd,
                                               void* buffer,
                                               size_t length,
                                               std::vector<int>& fds,
                                               pid_t* out_pid)
{
    fds.clear();

    struct msghdr msg = {};
    struct iovec iov;
    iov.iov_base = buffer;
    iov.iov_len = length;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    const size_t kControlBufferSize =
        CMSG_SPACE(sizeof(int) * kMaxFileDescriptors) +
        CMSG_SPACE(sizeof(struct ucred));
    std::vector<char> control_buffer(kControlBufferSize);
    msg.msg_control = control_buffer.data();
    msg.msg_controllen = static_cast<socklen_t>(kControlBufferSize);

    ssize_t ret = recvmsg(fd, &msg, 0);
    if (ret == -1) {
        LOGE("UnixSocketUtils::RecvMsgWithFdsAndPid recvmsg failed, errno=%{public}d", errno);
        return -1;
    }

    int* wire_fds = nullptr;
    size_t wire_fds_len = 0;
    pid_t pid = -1;

    if (msg.msg_controllen > 0) {
        for (struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg); cmsg;
             cmsg = CMSG_NXTHDR(&msg, cmsg)) {
            const size_t payload_len = cmsg->cmsg_len - CMSG_LEN(0);
            if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS) {
                wire_fds = reinterpret_cast<int*>(CMSG_DATA(cmsg));
                wire_fds_len = payload_len / sizeof(int);
            }
            if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_CREDENTIALS) {
                pid = reinterpret_cast<struct ucred*>(CMSG_DATA(cmsg))->pid;
            }
        }
    }

    if ((msg.msg_flags & MSG_TRUNC) || (msg.msg_flags & MSG_CTRUNC)) {
        if (msg.msg_flags & MSG_CTRUNC) {
            LOGE("UnixSocketUtils::RecvMsgWithFdsAndPid MSG_CTRUNC, buffer len=%{public}d",
                 msg.msg_controllen);
        }
        for (size_t i = 0; i < wire_fds_len; ++i) {
            close(wire_fds[i]);
        }
        errno = EMSGSIZE;
        return -1;
    }

    if (wire_fds) {
        for (size_t i = 0; i < wire_fds_len; ++i) {
            fds.push_back(wire_fds[i]);
        }
    }

    if (out_pid) {
        *out_pid = pid;
    }
    return ret;
}

void UnixSocketUtils::DeleteSocketFile(const std::string& path)
{
    unlink(path.c_str());
}

}  // namespace ohos::adapter::common
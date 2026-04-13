// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/multiprocess/unix_domain_socket_helper.h"

#include <cerrno>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <unistd.h>

#include "ohos/adapter/common/logging.h"
#include "ohos/adapter/multiprocess/scoped_socket.h"

namespace ohos::adapter::multiprocess {
const size_t UnixDomainSocketHelper::kMaxFileDescriptors = 16;

bool UnixDomainSocketHelper::EnableReceiveProcessId(int fd) {
  const int enable = 1;
  return setsockopt(fd, SOL_SOCKET, SO_PASSCRED, &enable, sizeof(enable)) == 0;
}

bool UnixDomainSocketHelper::SendMsg(int fd,
                                     const void* buf,
                                     size_t length,
                                     const std::vector<int>& fds) {
  struct msghdr msg = {};
  struct iovec iov = {const_cast<void*>(buf), length};
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;

  char* control_buffer = nullptr;
  if (fds.size()) {
    const size_t control_len = CMSG_SPACE(sizeof(int) * fds.size());
    control_buffer = new char[control_len];

    struct cmsghdr* cmsg;
    msg.msg_control = control_buffer;
    msg.msg_controllen = static_cast<socklen_t>(control_len);
    cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;

    cmsg->cmsg_len = static_cast<socklen_t>(CMSG_LEN(sizeof(int) * fds.size()));

    memcpy(CMSG_DATA(cmsg), &fds[0], sizeof(int) * fds.size());
    msg.msg_controllen = cmsg->cmsg_len;
  }

  const int flags = MSG_NOSIGNAL;
  const auto r = sendmsg(fd, &msg, flags);
  const bool ret = static_cast<ssize_t>(length) == r;
  if (!ret) {
    LOGE("SendMsg error, errno=%{public}d, Reason: %{public}s",
         errno, strerror(errno));
  }
  delete[] control_buffer;
  return ret;
}

int UnixDomainSocketHelper::RecvMsg(int fd,
                                    void* buf,
                                    size_t length,
                                    std::vector<int>& fds) {
  return UnixDomainSocketHelper::RecvMsgWithPid(fd, buf, length, fds, nullptr);
}

int UnixDomainSocketHelper::RecvMsgWithPid(int fd,
                                           void* buf,
                                           size_t length,
                                           std::vector<int>& fds,
                                           pid_t* pid) {
  return UnixDomainSocketHelper::RecvMsgWithFlags(fd, buf, length, 0, fds, pid);
}

int UnixDomainSocketHelper::RecvMsgWithFlags(int fd,
                                             void* buf,
                                             size_t length,
                                             int flags,
                                             std::vector<int>& fds,
                                             pid_t* out_pid) {
  fds.clear();

  struct msghdr msg = {};
  struct iovec iov = {buf, length};
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;

  const size_t kControlBufferSize =
      CMSG_SPACE(sizeof(int) * kMaxFileDescriptors) +
      CMSG_SPACE(sizeof(struct ucred));
  char control_buffer[kControlBufferSize];
  msg.msg_control = control_buffer;
  msg.msg_controllen = sizeof(control_buffer);

  const int r = recvmsg(fd, &msg, flags);
  if (r == -1) {
    LOGE("RecvMsg error, errno=%{public}d, Reason: %{public}s",
         errno, strerror(errno));
    return r;
  }

  int* wire_fds = nullptr;
  size_t wire_fds_len = 0;
  int pid = -1;

  if (msg.msg_controllen > 0) {
    for (struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg); cmsg;
         cmsg = CMSG_NXTHDR(&msg, cmsg)) {
      const size_t payload_len = cmsg->cmsg_len - CMSG_LEN(0);
      if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS) {
        wire_fds = reinterpret_cast<int*>(CMSG_DATA(cmsg));
        wire_fds_len = payload_len / sizeof(int);
      }

      if (cmsg->cmsg_level == SOL_SOCKET &&
          cmsg->cmsg_type == SCM_CREDENTIALS) {
        pid = reinterpret_cast<struct ucred*>(CMSG_DATA(cmsg))->pid;
      }
    }
  }

  if ((msg.msg_flags & MSG_TRUNC) || (msg.msg_flags & MSG_CTRUNC)) {
    if (msg.msg_flags & MSG_CTRUNC) {
      LOGE("recvmsg returned MSG_CTRUNC flag, buffer len is %d",
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
  return 0;
}

ScopedSocket UnixDomainSocketHelper::CreateServerUnixDomainSocket(
    const std::string& path) {
  DeleteFile(path.c_str());
  const char* LOG_PREFIX = "ServerProcess";
  int server_socket = socket(AF_UNIX, SOCK_STREAM, 0);
  if (server_socket == -1) {
    LOGE("%{public}s socket error, errno=%{public}d, Reason: %{public}s",
         LOG_PREFIX, errno, strerror(errno));
    return ScopedSocket(-1);
  }

  sockaddr_un addr{};
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);

  if (bind(server_socket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) ==
      -1) {
    LOGE("%{public}s bind error, errno=%{public}d, Reason: %{public}s",
         LOG_PREFIX, errno, strerror(errno));
    close(server_socket);
    return ScopedSocket(-1);
  }

  if (listen(server_socket, 1) == -1) {
    LOGE("%{public}s listen error, errno=%{public}d, Reason: %{public}s",
         LOG_PREFIX, errno, strerror(errno));
    close(server_socket);
    return ScopedSocket(-1);
  }
  return ScopedSocket(server_socket);
}

ScopedSocket UnixDomainSocketHelper::CreateClientUnixDomainSocket(
    const std::string& path) {
  const char* LOG_PREFIX = "ChildProcess";
  int remote_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (remote_fd == -1) {
    LOGE("%{public}s socket error, errno=%{public}d, Reason: %{public}s",
         LOG_PREFIX, errno, strerror(errno));
    return ScopedSocket(-1);
  }
  UnixDomainSocketHelper::EnableReceiveProcessId(remote_fd);
  sockaddr_un addr{};
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
  unsigned length = strlen(addr.sun_path) + sizeof(addr.sun_family);
  int ret = connect(remote_fd, static_cast<const sockaddr*>((sockaddr*)&addr),
                    length);
  if (ret == -1) {
    LOGE(
        "%{public}s socket connect failed, errno=%{public}d, Reason: "
        "%{public}s",
        LOG_PREFIX, errno, strerror(errno));
    return ScopedSocket(-1);
  }
  return ScopedSocket(remote_fd);
}

void UnixDomainSocketHelper::DeleteFile(const std::string& path) {
  unlink(path.c_str());
}
}  // namespace ohos::adapter::multiprocess

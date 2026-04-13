// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_MULTIPROCESS_UNIXDOMAINSOCKETHELPER_H_
#define OHOS_ADAPTER_MULTIPROCESS_UNIXDOMAINSOCKETHELPER_H_

#include <cstddef>
#include <string>
#include <vector>

#include "ohos/adapter/multiprocess/scoped_socket.h"
namespace ohos::adapter::multiprocess {
class UnixDomainSocketHelper {
 public:
  static const size_t kMaxFileDescriptors;

  static ScopedSocket CreateServerUnixDomainSocket(const std::string& path);

  static ScopedSocket CreateClientUnixDomainSocket(const std::string& path);

  // Use to enable receiving process IDs in RecvMsgWithPid.  Should be called on
  // the receiving socket (i.e., the socket passed to RecvMsgWithPid). Returns
  // true if successful.
  static bool EnableReceiveProcessId(int fd);

  // Use recvmsg to read a message and an array of file descriptors. Returns
  // -1 on failure. Note: will read, at most, |kMaxFileDescriptors| descriptors.
  static int RecvMsg(int fd, void* msg, size_t length, std::vector<int>& fds);

  // Same as RecvMsg above, but also returns the sender's process ID (as seen
  // from the caller's namespace).  However, before using this function to
  // receive process IDs, EnableReceiveProcessId() should be called on the
  // receiving socket.
  static int RecvMsgWithPid(int fd,
                            void* msg,
                            size_t length,
                            std::vector<int>& fds,
                            pid_t* pid);

  // Use sendmsg to write the given msg and include a vector of file
  // descriptors. Returns true if successful.
  static bool SendMsg(int fd,
                      const void* msg,
                      size_t length,
                      const std::vector<int>& fds);

  static void DeleteFile(const std::string& path);

  static const int KRecvMsgFailed = -1;
 private:
  // Similar to RecvMsg, but allows to specify |flags| for recvmsg(2).
  static int RecvMsgWithFlags(int fd,
                              void* msg,
                              size_t length,
                              int flags,
                              std::vector<int>& fds,
                              pid_t* pid);
};
}  // namespace ohos::adapter::multiprocess
#endif  // OHOS_ADAPTER_MULTIPROCESS_UNIXDOMAINSOCKETHELPER_H_

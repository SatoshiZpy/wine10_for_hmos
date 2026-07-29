// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_COMMON_UNIX_SOCKET_UTILS_H_
#define OHOS_ADAPTER_COMMON_UNIX_SOCKET_UTILS_H_

#include <cstddef>
#include <string>
#include <vector>

namespace ohos::adapter::common {

class UnixSocketUtils {
public:
    static constexpr size_t kMaxFileDescriptors = 16;

    static int CreateServerSocket(const std::string& path);

    static int CreateClientSocket(const std::string& path);

    static bool EnableReceiveProcessId(int fd);

    static bool SendMsgWithFds(int fd,
                               const void* data,
                               size_t length,
                               const std::vector<int>& fds);

    static ssize_t RecvMsgWithFdsAndPid(int fd,
                                        void* buffer,
                                        size_t length,
                                        std::vector<int>& fds,
                                        pid_t* out_pid = nullptr);

    static void DeleteSocketFile(const std::string& path);
};

}  // namespace ohos::adapter::common

#endif  // OHOS_ADAPTER_COMMON_UNIX_SOCKET_UTILS_H_
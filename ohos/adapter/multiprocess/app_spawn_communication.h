// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_MULTIPROCESS_APP_SPAWN_COMMUNICATION_H_
#define OHOS_ADAPTER_MULTIPROCESS_APP_SPAWN_COMMUNICATION_H_

#include "ohos/adapter/multiprocess/scoped_socket.h"

#include <memory>
#include <string>
#include <vector>

#include "ohos/adapter/export.h"

namespace ohos::adapter::multiprocess {

class ADAPTER_EXPORT_API AppSpawnCommunication {
 public:
  using FdRemapVector = std::vector<std::pair<int, int>>;
  // RequestCommandline is used in the child process
  // commands
  static bool RequestCommandline(std::vector<std::string>& commands);
  static FdRemapVector GetFdIdsRemap();
  static void SetFdIdsRemap(FdRemapVector& fds);
  // 1. Build commands response
  // 2. Start socket listen
  bool PrepareHandleRequest(const std::vector<std::string>& commands,
                            const FdRemapVector& fds);

  // Accept Request in blocking mode;
  // when recvice request, commands will pass to child process
  bool HandleRequestAfterChildProcStart(pid_t child_pid);
  ~AppSpawnCommunication();

 private:
  static std::vector<std::string> Split(const std::string& commands,
                                        const std::string& separator);
  static std::vector<int> GetRemapFd(const std::vector<std::string>& commands);

  bool PrepareResponseData(const std::vector<std::string>& commands,
                           const FdRemapVector& fds);
  bool PrepareCommunication();

  int GetCmdLinesSize() const { return this->commandslines_.size(); }
  const void* GetCmdLinesData() const { return this->commandslines_.data(); }

  ScopedSocket GetClientSocket() {
    return ScopedSocket(this->server_socket_->Accept());
  }

  std::string commandslines_;
  std::vector<int> share_fds_;
  std::unique_ptr<ScopedSocket> server_socket_;
};
}  // namespace ohos::adapter::multiprocess
#endif  // OHOS_ADAPTER_MULTIPROCESS_APP_SPAWN_COMMUNICATION_H_

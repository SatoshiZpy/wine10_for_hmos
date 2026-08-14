// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_CHILD_PROCESS_STARTER_H
#define OHOS_ADAPTER_CHILD_PROCESS_STARTER_H

#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>
#include "ohos/adapter/export.h"

namespace ohos::adapter::multiprocess {

class ADAPTER_EXPORT_API ChildProcessStarter {
 public:
  using StartCallbackFunc = std::function<void(int32_t)>;

  static ChildProcessStarter& GetInstance();

  int StartLegacyChildProcess(const std::vector<std::string>& command,
                              const std::vector<std::pair<int, int>>& fds);

  int StartIsolateChildProcess(const std::vector<std::string>& command,
                               const std::vector<std::pair<int, int>>& fds,
                               const std::string& entry_point = "");

  int StartGpuProcess(const std::vector<std::string>& command,
                      const std::vector<std::pair<int, int>>& fds);

  void CreateProcess(const std::string& processKey,
                     const std::string& sock_path);

  void KillProcess();

 private:
   ChildProcessStarter() = default;
  ~ChildProcessStarter() = default;

  int StartChildProcess(std::function<void(StartCallbackFunc)> startFunc,
                        const std::vector<std::string>& command,
                        const std::vector<std::pair<int, int>>& fds);

  std::mutex mutex_;
};

}  // namespace ohos::adapter::multiprocess

#endif  // OHOS_ADAPTER_CHILD_PROCESS_STARTER_H

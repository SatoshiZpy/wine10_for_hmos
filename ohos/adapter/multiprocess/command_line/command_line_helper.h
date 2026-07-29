// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_MULTIPROCESS_COMMAND_LINE_COMMAND_LINE_HELPER_H_
#define OHOS_ADAPTER_MULTIPROCESS_COMMAND_LINE_COMMAND_LINE_HELPER_H_

#include <string>
#include <vector>

namespace ohos::adapter::multiprocess {
class CommandLineHelper {
 public:
  using FdRemapVector = std::vector<std::pair<int, int>>;
  static std::string BuildCommands(
      const std::vector<std::string>& commands,
      const std::vector<std::string>& extra_commands = {});

  static std::vector<std::string> Split(const std::string& src, const std::string& separator);

  static std::string BuildSharedFDCommand(const FdRemapVector& fds, std::vector<int>& share_fds);

  static void SetCommandLine(int process_type,
    const std::vector<std::string>& args,
    const std::vector<std::string>& env);

  static std::string GetEnvString();

  static bool IsMainProcess();

  static int32_t GetMainProcessId();

  static std::string GetProcessName();

  static int GetProcessType();
};
}  // namespace ohos::adapter::multiprocess
#endif  // OHOS_ADAPTER_MULTIPROCESS_COMMAND_LINE_COMMAND_LINE_HELPER_H_

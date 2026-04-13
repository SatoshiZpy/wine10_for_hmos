// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_MULTIPROCESS_COMMAND_LINE_H
#define OHOS_ADAPTER_MULTIPROCESS_COMMAND_LINE_H

#include <map>
#include <string>
#include <vector>

#include "command_line_switches.h"

namespace ohos::adapter::multiprocess {
class CommandLine {
 public:
  static bool InitForCurrentProcess(const std::vector<std::string>& args);
  static void ResetForCurrentProcess();

  static CommandLine* ForCurrentProcess();

  bool HasSwitch(const char* switch_key);

 private:
  void ParseCommandLine(const std::vector<std::string>& args);

  bool ParseSwitch(const std::string& arg,
                   std::string* switch_key,
                   std::string* switch_val);

  void AppendSwitch(const std::string& switch_key,
                    const std::string& switch_val);

  std::size_t GetSwitchPrefixLength(const std::string& arg);

  explicit CommandLine(const std::vector<std::string>& args);
  ~CommandLine() = default;

  std::map<std::string, std::string> switches_;
  static CommandLine* current_process_command_line_;
};
}  // namespace ohos::adapter::multiprocess

#endif  // OHOS_ADAPTER_MULTIPROCESS_COMMAND_LINE_H

// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "command_line.h"

namespace ohos::adapter::multiprocess {

static const std::string kSwitchPrefixes[] = {"--", "-"};

static const std::string kSwitchValueSeparator = "=";

CommandLine* CommandLine::current_process_command_line_ = nullptr;

bool CommandLine::InitForCurrentProcess(const std::vector<std::string>& args) {
  if (current_process_command_line_) {
    return false;
  }
  current_process_command_line_ = new CommandLine(args);
  return true;
}

void CommandLine::ResetForCurrentProcess() {
  if (current_process_command_line_ == nullptr) {
    return;
  }

  delete current_process_command_line_;
  current_process_command_line_ = nullptr;
}

CommandLine* CommandLine::ForCurrentProcess() {
  return current_process_command_line_;
}

CommandLine::CommandLine(const std::vector<std::string>& args) {
  ParseCommandLine(args);
}

void CommandLine::ParseCommandLine(const std::vector<std::string>& args) {
  for (const std::string& arg : args) {
    std::string switch_key;
    std::string switch_val;
    if (ParseSwitch(arg, &switch_key, &switch_val)) {
      AppendSwitch(switch_key, switch_val);
    }
  }
}

bool CommandLine::ParseSwitch(const std::string& arg,
                              std::string* switch_key,
                              std::string* switch_val) {
  switch_key->clear();
  switch_val->clear();
  std::size_t prefix_len = GetSwitchPrefixLength(arg);
  if (prefix_len == 0 || prefix_len == arg.length()) {
    return false;
  }

  std::size_t equals_position = arg.find(kSwitchValueSeparator);
  if (equals_position == std::string::npos) {
    *switch_key = arg.substr(prefix_len);
  } else {
    *switch_key = arg.substr(prefix_len, equals_position - prefix_len);
    *switch_val = arg.substr(equals_position + 1);
  }
  return true;
}

std::size_t CommandLine::GetSwitchPrefixLength(const std::string& arg) {
  for (const std::string& prefix : kSwitchPrefixes) {
    if (arg.substr(0, prefix.length()) == prefix) {
      return prefix.length();
    }
  }
  return 0;
}

void CommandLine::AppendSwitch(const std::string& switch_key,
                               const std::string& switch_val) {
  switches_[switch_key] = switch_val;
}

bool CommandLine::HasSwitch(const char* switch_key) {
  if (switch_key == nullptr) {
    return false;
  }
  return switches_.contains(std::string(switch_key));
}
}  // namespace ohos::adapter::multiprocess

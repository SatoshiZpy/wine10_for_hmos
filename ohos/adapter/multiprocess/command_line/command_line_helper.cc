// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "command_line_helper.h"

#include <unistd.h>
#include <sstream>
#include "ohos/adapter/common/logging.h"

namespace ohos::adapter::multiprocess {
const std::string kCommandSeparator = "#";
const std::string kFdRemapCommand = "--fds_to_remap=";
const std::string kEnvCommand = "--env=";
const std::string kEnvSeparator = ";";
const std::string kFdSeparator = ",";

static std::vector<std::string> wrapp_args_ = {};
static std::vector<std::string> wrapp_env_ = {};
static std::string env_string_ = "";
static int32_t g_pid_ = -1;

std::string CommandLineHelper::BuildCommands(
    const std::vector<std::string>& commands,
    const std::vector<std::string>& extra_commands) {
  const size_t min_commands_size = 1;
  if (commands.size() < min_commands_size) {
    return "";
  }
  std::stringstream argv_ss;
  for (size_t i = 0; i < commands.size() - 1; ++i) {
    argv_ss << commands[i] << kCommandSeparator;
  }
  argv_ss << commands[commands.size() - 1];
  if (!extra_commands.empty()) {
    argv_ss << kCommandSeparator;
    for (size_t i = 0; i < extra_commands.size() - 1; ++i) {
      argv_ss << extra_commands[i] << kCommandSeparator;
    }
    argv_ss << extra_commands[extra_commands.size() - 1];
  }
  if (!env_string_.empty()) {
    argv_ss << kCommandSeparator << env_string_;
  }

  return argv_ss.str();
}

std::vector<std::string> CommandLineHelper::Split(
    const std::string& src,
    const std::string& separator) {
  const size_t limit = 64;
  std::vector<std::string> res;
  if (src.empty()) {
    return res;
  }
  size_t start = 0;
  size_t end = 0;
  while (start < src.size() && (limit == 0 || res.size() < limit)) {
    end = src.find(separator, start);
    if (end == std::string::npos) {
      res.push_back(src.substr(start));
      return res;
    }
    res.push_back(src.substr(start, end - start));
    start = end + separator.length();
  }
  return res;
}

std::string CommandLineHelper::BuildSharedFDCommand(
    const FdRemapVector& fds,
    std::vector<int>& share_fds) {
  if (fds.empty()) {
    LOGD("no input fds");
    return "";
  }
  std::string shared_fd_command = kFdRemapCommand;
  for (auto& [rawfd, remap_fd] : fds) {
    LOGI("HandleRequest fd %{public}d => %{public}d", rawfd, remap_fd);
    share_fds.push_back(rawfd);
    shared_fd_command += std::to_string(remap_fd) + kFdSeparator;
  }
  if (!shared_fd_command.empty()) {
    shared_fd_command.pop_back();
  }
  return shared_fd_command;
}

void CommandLineHelper::WrapCommands(const std::vector<std::string>& args,
  const std::vector<std::string>& env) {
    wrapp_args_ = args;
    wrapp_env_ = env;
    g_pid_ = getpid();

    if (!env.empty()) {
      std::string env_command = kEnvCommand;
      for (auto& env_value : env) {
        env_command += env_value + kEnvSeparator;
      }
      if (!env_command.empty()) {
        env_command.pop_back();
      }
      env_string_ = env_command;
    }
    LOGD("WrapCommands env value: %{public}s", env_string_.c_str());
}

std::string CommandLineHelper::GetEnvString() {
  return env_string_;
}

bool CommandLineHelper::IsMainProcess() {
  return g_pid_ > 0;
}

int32_t CommandLineHelper::GetMainProcessId() {
  return g_pid_;
}
}  // namespace ohos::adapter::multiprocess

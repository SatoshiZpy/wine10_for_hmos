// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "command_line_helper.h"

#include <fcntl.h>
#include <unistd.h>
#include <sstream>

#include "ohos/adapter/common/constants.h"
#include "ohos/adapter/common/logging.h"

namespace ohos::adapter::multiprocess {
const std::string kCommandSeparator = "#";
const std::string kFdRemapCommand = "--fds_to_remap=";
const std::string kEnvCommand = "--env=";
const std::string kEnvSeparator = ";";
const std::string kFdSeparator = ",";

static std::vector<std::string> command_line_ = {};
static std::string env_line_ = "";
static std::string process_name_ = "";
static int g_process_type_ = -1;
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
  if (!env_line_.empty()) {
    argv_ss << kCommandSeparator << env_line_;
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

void CommandLineHelper::SetCommandLine(int process_type,
  const std::vector<std::string>& args,
  const std::vector<std::string>& env) {
    command_line_ = args;
    g_process_type_ = process_type;
    g_pid_ = getpid();

    std::string full_name;
    int fd = open("/proc/self/cmdline", O_RDONLY);
    if (fd >= 0) {
        char buf[256] = {};
        ssize_t n = read(fd, buf, sizeof(buf) - 1);
        close(fd);
        if (n > 0) { full_name = buf; }
    }
    size_t colon_pos = full_name.rfind(':');
    if (colon_pos != std::string::npos) {
        process_name_ = full_name.substr(colon_pos + 1);
    } else {
        process_name_ = "";
    }
    LOGI("SetCommandLine process_name: %{public}s, full_name: %{public}s",
         process_name_.c_str(), full_name.c_str());

    for (size_t i = 0; i < args.size(); i++) {
      LOGI("SetCommandLine arg[%{public}zu]: %{public}s", i, args[i].c_str());
    }
    for (size_t i = 0; i < env.size(); i++) {
      LOGI("SetCommandLine env[%{public}zu]: %{public}s", i, env[i].c_str());
    }

    if (!env.empty()) {
      std::string env_command = kEnvCommand;
      for (auto& env_value : env) {
        env_command += env_value + kEnvSeparator;
      }
      if (!env_command.empty()) {
        env_command.pop_back();
      }
      env_line_ = env_command;
    }
}

std::string CommandLineHelper::GetEnvString() {
  return env_line_;
}

bool CommandLineHelper::IsMainProcess() {
  return g_pid_ > 0 && g_process_type_ == ProcessType::kMainProcess;
}

int32_t CommandLineHelper::GetMainProcessId() {
  return g_pid_;
}

std::string CommandLineHelper::GetProcessName() {
  return process_name_;
}

int CommandLineHelper::GetProcessType() {
  return g_process_type_;
}
}  // namespace ohos::adapter::multiprocess

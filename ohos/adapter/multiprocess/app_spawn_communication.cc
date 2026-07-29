// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app_spawn_communication.h"

#include <unistd.h>

#include <algorithm>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "ohos/adapter/common/logging.h"
#include "ohos/adapter/common/unix_socket_utils.h"
#include "ohos/adapter/multiprocess/command_line/command_line_helper.h"
#include "ohos/adapter/multiprocess/scoped_socket.h"

namespace ohos::adapter::multiprocess {
namespace {
AppSpawnCommunication::FdRemapVector g_fd_id_remap;
}

enum class RequstCommandType {
  kRequestUnknown = -1,
  kRequestLength = 1,
  kRequestData,
  kRequestEnd
};

const std::vector<RequstCommandType> RequstCommandList = {
  RequstCommandType::kRequestLength,
  RequstCommandType::kRequestData,
  RequstCommandType::kRequestEnd
};

const int kDefaultBufferSize = 16;
const std::string kFdRemapCommand = "--fds_to_remap=";
const std::string kFdSeparator = ",";
const std::string kCommandSeparator = "#";
const std::string kDomainSocketPath = "/data/storage/el2/base/files/start.sock";

int RecvRequest(int fd, pid_t pid) {
  char buff[8] = {0};
  std::vector<int> ret_fds;
  pid_t ret_pid = -1;

  if (common::UnixSocketUtils::RecvMsgWithFdsAndPid(fd,
                                                   buff,
                                                   sizeof(buff),
                                                   ret_fds,
                                                   &ret_pid) < 0) {
    LOGE("RecvMsgWithPid failed, pid = %{public}d, ret_pid = %{public}d, sizeof(buff) = %{public}lu",
         pid, ret_pid, sizeof(buff));
    return -1;
  }

  if (pid != ret_pid) {
    LOGE("ret_pid error, pid = %{public}d, ret_pid = %{public}d",
         pid, ret_pid);
    return -1;
  }

  return std::stoi(buff);
}

void FixInitClientFdForCrashDumpHandler(
    std::vector<std::string>& cmds,
    const AppSpawnCommunication::FdRemapVector& id_fds) {
  std::string init_fd;
  for (auto& [id, fd] : id_fds) {
    // refer to content/public/common/content_descriptors.h
    // id 0, is for crashdump
    if (id == 0) {
      init_fd = std::to_string((int)fd);
      break;
    }
  }
  for (int i = cmds.size() - 1; i >= 0; i--) {
    auto cmd = cmds[i];
    if (cmd.npos != cmd.find("--initial-client-fd=")) {
      std::string rep("--initial-client-fd=");
      rep.append(init_fd);
      cmds[i] = rep;
      return;
    }
  }
}

bool AppSpawnCommunication::PrepareHandleRequest(
    const std::vector<std::string>& commands, const FdRemapVector& fds) {
  if (!PrepareResponseData(commands, fds)) {
    LOGE("%{public}s, PrepareResponseData failed", __FUNCTION__);
    return false;
  }
  return PrepareCommunication();
}

bool AppSpawnCommunication::PrepareResponseData(
    const std::vector<std::string>& commands, const FdRemapVector& fds) {
  std::vector<int> share_fds;
  auto shared_fd_command = CommandLineHelper::BuildSharedFDCommand(fds, share_fds);
  auto commandlines = CommandLineHelper::BuildCommands(commands, {shared_fd_command});
  if (commandlines.size() >
      static_cast<size_t>(std::numeric_limits<int>::max())) {
    LOGE("send data length error");
    return false;
  }
  this->share_fds_ = share_fds;
  this->commandslines_ = commandlines;
  return true;
}

bool AppSpawnCommunication::PrepareCommunication() {
  int server_fd = common::UnixSocketUtils::CreateServerSocket(kDomainSocketPath);
  if (server_fd < 0) {
    LOGE("%{public}s, create server socket failed", __FUNCTION__);
    return false;
  }
  if (!common::UnixSocketUtils::EnableReceiveProcessId(server_fd)) {
    LOGE("EnableReceiveProcessId Failed");
    close(server_fd);
    return false;
  }
  this->server_socket_ = std::make_unique<ScopedSocket>(server_fd);
  return true;
}

bool AppSpawnCommunication::HandleRequestAfterChildProcStart(pid_t child_pid) {
  ScopedSocket cs = this->GetClientSocket();
  auto commandType = RequstCommandType::kRequestUnknown;
  bool send_msg_result = false;
  do {
    commandType = (RequstCommandType)RecvRequest(cs.GetSocket(), child_pid);
    switch (commandType) {
      case RequstCommandType::kRequestLength: {
        // step 1: Handle Requst Data Length Command and Send Data Length
        std::string length = std::to_string(this->GetCmdLinesSize());
        send_msg_result = common::UnixSocketUtils::SendMsgWithFds(cs.GetSocket(),
                                                                  length.data(),
                                                                  length.size(),
                                                                  {});
        if (!send_msg_result) {
          LOGE("[%{public}s#%{public}s], send request length failed",
               __FILE__, __FUNCTION__);
        }
        break;
      }
      case RequstCommandType::kRequestData:
        // step 2: Handle Requst Data Command and Send Data
        send_msg_result = common::UnixSocketUtils::SendMsgWithFds(cs.GetSocket(),
                                                                  this->GetCmdLinesData(),
                                                                  this->GetCmdLinesSize(),
                                                                  this->share_fds_);
        if (!send_msg_result) {
          LOGE("[%{public}s#%{public}s], send request data failed",
               __FILE__, __FUNCTION__);
        }
        break;
      case RequstCommandType::kRequestUnknown:
        LOGE("[%{public}s#%{public}s] Unknow Request: %{public}d", __FILE__,
             __FUNCTION__, static_cast<int>(commandType));
        return false;
      default:
        break;
    }
  } while (commandType != RequstCommandType::kRequestEnd);
  return send_msg_result;
}

AppSpawnCommunication::~AppSpawnCommunication() {
  if (this->server_socket_) {
    this->server_socket_->Close();
  }
  common::UnixSocketUtils::DeleteSocketFile(kDomainSocketPath);
}

bool AppSpawnCommunication::RequestCommandline(
    std::vector<std::string>& commands) {
  int remote_fd =
      common::UnixSocketUtils::CreateClientSocket(kDomainSocketPath);
  if (remote_fd < 0) {
    LOGE("CreateClientSocket failed");
    return false;
  }
  ScopedSocket remote_socket(remote_fd);

  size_t mem_length = kDefaultBufferSize;
  std::vector<int> fds;
  for (size_t i = 0; i < RequstCommandList.size(); i++) {
    auto command = RequstCommandList[i];
    auto request = std::to_string((int)command);
    std::vector<char> mem_buf(mem_length);
    common::UnixSocketUtils::SendMsgWithFds(remote_socket.GetSocket(),
                                            request.data(),
                                            request.size(),
                                            {});
    ssize_t recv_result = -1;
    switch (command) {
      case RequstCommandType::kRequestLength:
        recv_result = common::UnixSocketUtils::RecvMsgWithFdsAndPid(
                                                      remote_socket.GetSocket(),
                                                      &mem_buf[0],
                                                      mem_length,
                                                      fds);
        if (recv_result < 0) {
          LOGE("receivce data length error, recvMsg failed");
          remote_socket.Close();
          return false;
        }
        mem_length = std::stoi(&mem_buf[0]) + 1;
        if (mem_length > static_cast<size_t>(std::numeric_limits<int>::max())) {
          LOGE("receivce data length error");
          remote_socket.Close();
          return false;
        }
        break;
      case RequstCommandType::kRequestData: {
        recv_result = common::UnixSocketUtils::RecvMsgWithFdsAndPid(
                                                      remote_socket.GetSocket(),
                                                      &mem_buf[0],
                                                      mem_length,
                                                      fds);
        if (recv_result < 0) {
          LOGE("receivce data error, recvMsg failed");
          remote_socket.Close();
          return false;
        }
        LOGI("receivce commandlines length: %{public}zu, data: %{public}s",
              mem_length,
              &mem_buf[0]);
        commands = Split(&mem_buf[0], kCommandSeparator);
        auto fd_map_ids = GetRemapFd(commands);
        if (fds.size() != fd_map_ids.size()) {
          LOGE("RequestCommandline fd size not fit, %{public}lu "
               "%{public}lu",
               fds.size(),
               fd_map_ids.size());
          remote_socket.Close();
          return false;
        }
        for (size_t j = 0; j < fds.size(); j++) {
          g_fd_id_remap.push_back(std::make_pair(fd_map_ids[j], fds[j]));
        }
        FixInitClientFdForCrashDumpHandler(commands, g_fd_id_remap);
        break;
      }
      default:
        break;
    }
  }
  remote_socket.Close();
  return true;
}

std::vector<std::string> AppSpawnCommunication::Split(
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

std::vector<int> AppSpawnCommunication::GetRemapFd(
    const std::vector<std::string>& commands) {
  std::string fd_string;
  for (size_t i = 0; i < commands.size(); i++) {
    auto command = commands[i];
    if (command.starts_with(kFdRemapCommand)) {
      fd_string = command.substr(kFdRemapCommand.size(),
                                 command.size() - kFdRemapCommand.size());
    }
  }
  LOGD("fd string %{public}s", fd_string.c_str());
  auto fs_strs = Split(fd_string, kFdSeparator);
  std::vector<int> res;
  for (auto& fd_str : fs_strs) {
    if (fd_str.empty()) {
      continue;
    }
    res.push_back(std::stoi(fd_str));
  }
  return res;
}

AppSpawnCommunication::FdRemapVector AppSpawnCommunication::GetFdIdsRemap() {
  return g_fd_id_remap;
}

void AppSpawnCommunication::SetFdIdsRemap(FdRemapVector& fds) {
  g_fd_id_remap = fds;
}
}  // namespace ohos::adapter::multiprocess

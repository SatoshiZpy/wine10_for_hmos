// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_childprocess_args_wrapper.h"
#include <cstdio>
#include "ohos/adapter/common/logging.h"
#include "ohos/adapter/multiprocess/command_line/command_line_helper.h"

namespace ohos::adapter::multiprocess {

NativeChildProcess_Args NativeChildProcessArgsWrapper::Build(
    const std::string& params,
    const std::vector<std::pair<int32_t, int32_t>>& fds) {
  NativeChildProcess_Args args;
  args.entryParams = new (std::nothrow) char[params.size() + 1]{0};
  if (args.entryParams != nullptr) {
    std::copy(params.c_str(), params.c_str() + params.size(), args.entryParams);
  }
  if (fds.empty()) {
    args.fdList.head = nullptr;
  }
  auto tail = args.fdList.head;
  for (size_t i = 0; i < fds.size(); i++) {
    auto fd = fds[i].first;
    LOGD("NativeChildProcessArgsWrapper::Build, FD %{public}d => %{public}d",
         fd, fds[i].second);
    NativeChildProcess_Fd* current = new (std::nothrow) NativeChildProcess_Fd;
    if (current == nullptr) {
      break;
    }
    const size_t fd_name_size = 16;
    char* fd_name = new (std::nothrow) char[fd_name_size]{0};
    if (fd_name == nullptr) {
      break;
    }
    sprintf(fd_name, "%d", fds[i].second);
    current->fdName = fd_name;
    current->fd = fd;
    current->next = nullptr;
    if (tail != nullptr) {
      tail->next = current;
    }
    if (args.fdList.head == nullptr) {
      args.fdList.head = current;
    }
    tail = current;
  }

  return args;
}

void NativeChildProcessArgsWrapper::Parse(
    NativeChildProcess_Args args,
    std::vector<std::string>& commands,
    std::vector<std::pair<int32_t, int32_t>>& fds) {
  commands.clear();
  if (args.entryParams == nullptr) {
    return;
  }

  auto tail = args.fdList.head;
  auto current = tail;

  while (current != nullptr) {
    int fd = current->fd;
    LOGD("NativeChildProcessArgsWrapper::Parse %{public}d => %{public}s", fd, current->fdName);
    fds.push_back({std::stoi(current->fdName), fd});
    current = current->next;
  }
}

void NativeChildProcessArgsWrapper::Free(NativeChildProcess_Args& args) {
  if (args.entryParams != nullptr) {
    delete (args.entryParams);
    args.entryParams = nullptr;
  }

  auto tail = args.fdList.head;
  auto current = tail;
  while (current != nullptr) {
    auto next = current->next;
    delete (current);
    current = next;
  }
}
}  // namespace ohos::adapter::multiprocess

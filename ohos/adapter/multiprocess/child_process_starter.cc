// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "child_process_starter.h"
#include <future>

#include <IPCKit/ipc_kit.h>
#include <sys/types.h>
#include <unistd.h>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include "aki/jsbind.h"
#include "ohos/adapter/aki_hook/aki_hook.h"
#include "ohos/adapter/common/logging.h"
#include "ohos/adapter/common/trace.h"
#include "ohos/adapter/multiprocess/app_spawn_communication.h"
#include "ohos/adapter/multiprocess/command_line/command_line_helper.h"
#include "ohos/adapter/multiprocess/isolate_process/isolate_process_helper.h"
#include "ohos/adapter/multiprocess/isolate_process/native_child_process_library.h"
#include "ohos/adapter/multiprocess/child_process/child_process_helper.h"

namespace ohos::adapter::multiprocess {

namespace {

const int kInvalidPid = -1;

void LogStartChildProcessFixesCommands(
    const std::vector<std::string>& commands) {
  for (auto& command : commands) {
    LOGI("StartChildProcess command %{public}s", command.c_str());
  }
}
}  // namespace

ChildProcessStarter& ChildProcessStarter::GetInstance() {
  static ChildProcessStarter instance;
  return instance;
}

int ChildProcessStarter::StartChildProcess(
    std::function<void(StartCallbackFunc)> start_func,
    const std::vector<std::string>& commands,
    const std::vector<std::pair<int, int>>& fds) {
  LogStartChildProcessFixesCommands(commands);

  std::promise<int32_t> child_pid;
  StartCallbackFunc set_child_pid = [&child_pid](int32_t pid) -> void {
    child_pid.set_value(pid);
  };

  // Must establish socket listen before start child process
  // if not, child process request will be "Connection failed"
  AppSpawnCommunication appspawn_server;
  if (appspawn_server.PrepareHandleRequest(commands, fds)) {
    start_func(set_child_pid);
    int cpid = child_pid.get_future().get();
    appspawn_server.HandleRequestAfterChildProcStart(cpid);
    return cpid;
  } else {
    LOGE("appspawn_server.PrepareHandleRequest failed");
  }
  return kInvalidPid;
}

int ChildProcessStarter::StartLegacyChildProcess(
    const std::vector<std::string>& commands,
    const std::vector<std::pair<int, int>>& fds) {
  TRACE_EVENT_0("ChildProcessStarter::StartLegacyChildProcess");

  auto start_child_process_func = ohos::adapter::GetJSFunction(
      "WineChildProcessStarter.StartChildProcess");
  if (!start_child_process_func) {
    LOGE("WineChildProcessStarter.StartChildProcess not defined");
    return kInvalidPid;
  }

  std::function<void(StartCallbackFunc)> start_func =
      [start_child_process_func](StartCallbackFunc callback) {
        start_child_process_func->Invoke<void>(callback);
      };

  return StartChildProcess(start_func, commands, fds);
}

int ChildProcessStarter::StartIsolateChildProcess(
    const std::vector<std::string>& command,
    const std::vector<std::pair<int, int>>& fds,
    const std::string& entry_point) {
#if defined(ENABLE_START_ISOLATE_PROCESS)
  TRACE_EVENT_0("ChildProcessStarter::StartIsolateChildProcess");
  return IsolateProcessHelper::StartChildProcess(command, fds, entry_point,
                                                 true);
#else
  LOGW("fallback to StartLegacyChildProcess");
  return StartLegacyChildProcess(command, fds);
#endif
}

int ChildProcessStarter::StartGpuProcess(
    const std::vector<std::string>& commands,
    const std::vector<std::pair<int, int>>& fds) {
  std::lock_guard<std::mutex> lock(mutex_);
  TRACE_EVENT_0("ChildProcessStarter::StartGpuProcess");
  LogStartChildProcessFixesCommands(commands);

  return ChildProcessHelper::CreateProcess(commands, fds);
}

void ChildProcessStarter::CreateProcess(
    const std::string& processKey,
    const std::string& sock_path) {
  LOGI("ChildProcessStarter::CreateProcess key=%{public}s, "
       "sock=%{public}s (empty impl)",
       processKey.c_str(), sock_path.c_str());

  auto func = ohos::adapter::GetJSFunction("WineChildProcessStarter.CreateProcess");
  if (!func) {
    LOGE("WineChildProcessStarter.CreateProcess not defined");
    return;
  }
  func->Invoke<void>(processKey, sock_path);
}

void ChildProcessStarter::KillProcess() {
  auto func = ohos::adapter::GetJSFunction("WineChildProcessStarter.KillProcess");
  if (!func) {
    LOGE("WineChildProcessStarter.KillProcess not defined");
    return;
  }
  func->Invoke<void>();
}

} // namespace ohos::adapter::multiprocess

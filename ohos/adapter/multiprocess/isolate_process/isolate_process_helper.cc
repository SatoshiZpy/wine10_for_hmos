// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "isolate_process_helper.h"

#include "ohos/adapter/common/logging.h"
#include "ohos/adapter/common/trace.h"

#include "ohos/adapter/multiprocess/child_process_starter.h"
#include "ohos/adapter/multiprocess/command_line/command_line_helper.h"
#include "ohos/adapter/multiprocess/isolate_process/native_child_process_configs_wrapper.h"
#include "ohos/adapter/multiprocess/isolate_process/native_child_process_library.h"
#include "ohos/adapter/multiprocess/isolate_process/native_childprocess_args_wrapper.h"

namespace ohos::adapter::multiprocess {

static constexpr const char *kDefaultEntryPoint = "libadapter.so:IsolateMain";
static NativeChildProcessLibrary nativeChildProcessLibrary;

int32_t IsolateProcessHelper::StartChildProcess(
    const std::vector<std::string> &command,
    const std::vector<std::pair<int, int>> &fds, const std::string &entry_point,
    bool is_isolated_process) {
  TRACE_EVENT_0("IsolateProcessHelper::StartChildProcess");

  int32_t pid = -1;
  
  std::vector<int> share_fds;
  auto shared_fd_command = CommandLineHelper::BuildSharedFDCommand(fds, share_fds);
  auto command_line = CommandLineHelper::BuildCommands(command, {shared_fd_command});
  NativeChildProcess_Args args =
      NativeChildProcessArgsWrapper::Build(command_line, fds);

  NativeChildProcess_Options options{.isolationMode =
                                         NCP_ISOLATION_MODE_NORMAL};

#if defined(SUPPORT_ISOLATED_MODE)
  if (is_isolated_process) {
    options.isolationMode = NCP_ISOLATION_MODE_ISOLATED;
  }
#else
  if (is_isolated_process) {
    LOGW("Isolation mode requested but not supported");
  }
#endif

  auto entry_point_str = entry_point.empty() ? kDefaultEntryPoint : entry_point;
  LOGI("IsolateProcessHelper::StartChildProcess commands: %{public}s, "
       "%{public}s",
       command_line.c_str(), entry_point_str.c_str());
  int32_t errCode = 0;
  if (nativeChildProcessLibrary.SupportsStartChildProcessWithConfigs() &&
      !command.empty()) {
    NativeChildProcessConfigsWrapper configs(nativeChildProcessLibrary);
    nativeChildProcessLibrary.SetProcessName(configs.Get(), command[0].c_str());
    nativeChildProcessLibrary.StartNativeChildProcessWithConfigs(
        entry_point_str.c_str(), args, configs.Get(), &pid);
  } else {
    nativeChildProcessLibrary.StartNativeChildProcess(entry_point_str.c_str(),
                                                      args, options, &pid);
  }

  if (errCode != 0) {
    LOGE("OH_Ability_StartNativeChildProcess failed, errcode: %{public}d",
         errCode);
  }
  NativeChildProcessArgsWrapper::Free(args);
  LOGI("IsolateProcessHelper::StartChildProcess pid:%{public}d", pid);
  return pid;
}
} // namespace ohos::adapter::multiprocess

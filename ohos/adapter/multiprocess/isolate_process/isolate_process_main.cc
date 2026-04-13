// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/multiprocess/isolate_process/isolate_process_main.h"

#include <thread>

#include "ohos/adapter/common/constants.h"
#include "ohos/adapter/common/logging.h"
#include "ohos/adapter/common/trace.h"
#include "ohos/adapter/multiprocess/app_spawn_communication.h"
#include "ohos/adapter/multiprocess/command_line/command_line_helper.h"
#include "ohos/adapter/multiprocess/isolate_process/isolate_process_helper.h"
#include "ohos/adapter/multiprocess/isolate_process/native_childprocess_args_wrapper.h"
#include "ohos/adapter/web_entry/web_entry.h"

using namespace std::chrono_literals;
using namespace ohos::adapter;
using namespace ohos::adapter::multiprocess;

void IsolateMain(NativeChildProcess_Args args) {
  if (args.entryParams == nullptr) {
    return;
  }
  {
    TRACE_EVENT_0("IsolateMain");
  }
  LOGD("IsolateMain %{public}s", args.entryParams);
  std::vector<std::string> commandlines =
      CommandLineHelper::Split(args.entryParams, "#");
  if (commandlines.size() == 0) {
    return;
  }
  for (const auto& commandline : commandlines) {
    LOGD("IsolateMain %{public}s", commandline.c_str());
  }
  std::vector<std::string> commands;
  std::vector<std::pair<int32_t, int32_t>> fds;
  NativeChildProcessArgsWrapper::Parse(args, commands, fds);
  AppSpawnCommunication::SetFdIdsRemap(fds);

  std::string process_loader = commandlines[0];
  int process_type = process_loader == "wineserver" ? ProcessType::kServerProcess :
                                                      ProcessType::kChildProcess;
  int ret = web_entry::RunIsolateProcessType(process_type, commandlines);
  LOGI("IsolateMain exited with return: %{public}d", ret);
}

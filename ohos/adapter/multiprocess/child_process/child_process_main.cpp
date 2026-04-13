/*
 * Copyright (c) 2023-2026 Huawei Device Co., Ltd.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "ohos/adapter/multiprocess/child_process/child_process_main.h"

#include <unistd.h>

#include "ohos/adapter/common/constants.h"
#include "ohos/adapter/common/logging.h"
#include "ohos/adapter/multiprocess/app_spawn_communication.h"
#include "ohos/adapter/multiprocess/child_process/child_process.h"
#include "ohos/adapter/multiprocess/child_process/child_process_stub.h"
#include "ohos/adapter/web_entry/web_entry.h"

using namespace ohos::adapter;
using namespace ohos::adapter::multiprocess;

OHIPCRemoteStub* NativeChildProcess_OnConnect()
{
    LOGI("NativeChildProcess_OnConnect");
    return ChildProcess::Instance().GetChildProcessStub()->Get();
}

void ADAPTER_EXPORT_API NativeChildProcess_MainProc()
{
    LOGI("NativeChildProcess_MainProc: Create ChildProcess ok, pid = %{public}d.", getpid());
    auto childProcessStub = ChildProcess::Instance().GetChildProcessStub();
    childProcessStub->WaitReady();
    std::vector<std::string> commandlines = childProcessStub->GetCommands();
    std::vector<std::pair<int, int>> share_fds = childProcessStub->GetFds();

    for (const auto& commandline : commandlines) {
        LOGI("NativeChildProcess_MainProc commandline: %{public}s", commandline.c_str());
    }

    for (const auto& [rawfd, remap_fd] : share_fds) {
        LOGI("NativeChildProcess_MainProc fd %{public}d => %{public}d", rawfd, remap_fd);
    }

    if (commandlines.empty()) {
        LOGE("NativeChildProcess_MainProc: receive command from parent process "
             "failed, exit.");
        return;
    }
    AppSpawnCommunication::SetFdIdsRemap(share_fds);

    std::string process_loader = commandlines[0];
    int process_type = process_loader == "wineserver" ? ProcessType::kServerProcess : ProcessType::kChildProcess;
    int ret = web_entry::RunIsolateProcessType(process_type, commandlines);
    LOGI("NativeChildProcess_MainProc exited with return: %{public}d", ret);
}
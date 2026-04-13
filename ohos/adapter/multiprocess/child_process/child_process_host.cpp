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

#include "ohos/adapter/multiprocess/child_process/child_process_host.h"

#include "ohos/adapter/multiprocess/child_process/child_process_helper.h"
#include "ohos/adapter/multiprocess/command_line/command_line_helper.h"

namespace ohos::adapter::multiprocess {

ChildProcessHost& ChildProcessHost::Instance()
{
    static ChildProcessHost instance;
    return instance;
}

void ChildProcessHost::OnProcessCreated(int pid, OHIPCRemoteProxy * remoteProxy)
{
    LOGI("pid %{public}d", pid);
    std::vector<std::shared_ptr<StubHolder>> processStubs = {processManagerStub_};
    std::shared_ptr<ChildProcessProxy> proxy = std::make_shared<ChildProcessProxy>(remoteProxy, processStubs);
    
    childProcesses_.insert({pid, std::move(proxy)});
}

void ChildProcessHost::OnProcessDestroyed(int pid)
{
    LOGI("pid %{public}d", pid);
    childProcesses_.erase(pid);
    childProcessInfos_.erase(pid);
}

bool ChildProcessHost::SetProcessInfo(int32_t pid, const ChildProcessInfo& info)
{
    childProcessInfos_.insert({pid, info});
    const auto& proxy = childProcesses_[pid];
    if (proxy) {
        std::vector<std::string> commands = info.cmdline;
        std::string envString = CommandLineHelper::GetEnvString();
        if (!envString.empty()) {
            commands.push_back(envString);
        }
        proxy->SetProcessInfo(commands, info.fds);
    } else {
        LOGW("proxy is null");
        return false;
    }

    return true;
}

bool ChildProcessHost::SetWindowInfo(int32_t pid, int32_t widget_id, void* window)
{
    const auto& proxy = childProcesses_[pid];
    if (proxy) {
        proxy->SetWindowInfo(widget_id, window);
    } else {
        LOGW("proxy is null");
        return false;
    }

    return true;
}

bool ChildProcessHost::SendInputEvent(int pid, const input_event &event)
{
    auto it = childProcesses_.find(pid);
    if (it == childProcesses_.end()) {
        LOGE("proxy is null");
        return false;
    }

    const auto& proxy = it->second;
    int ret = proxy->SendInputEvent(event);
    if (ret != ERR_OK) {
        LOGE("SendInputEvent failed, widget_id: %{public}d, event type : %{public}d, ret : %{public}d",
             event.widget_id, event.type, ret);
        return false;
    }
    return true;
}

} // namespace ohos::adapter::multiprocess
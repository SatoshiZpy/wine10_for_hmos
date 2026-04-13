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

#ifndef OHOS_ADAPTER_MULTIPROCESS_CHILD_PROCESS_CHILD_PROCESS_HOST_H_
#define OHOS_ADAPTER_MULTIPROCESS_CHILD_PROCESS_CHILD_PROCESS_HOST_H_

#include <map>
#include <memory>

#include "ohos/adapter/common/logging.h"
#include "ohos/adapter/multiprocess/child_process/child_process_proxy.h"
#include "ohos/adapter/multiprocess/child_process/process_manager_stub.h"

namespace ohos::adapter::multiprocess {

class ChildProcessHost {
public:
    struct ChildProcessInfo {
    public:
        std::vector<std::string> cmdline;
        std::vector<std::pair<int, int>> fds;
    };
    static ChildProcessHost& Instance();

    void EchoAll() {
        LOGI("%{public}lu", childProcesses_.size());
        for(const auto &[pid, proxy]: childProcesses_) {
            LOGI("%{public}d", pid);
            proxy->Echo("hello");
        }
    }

    bool SetProcessInfo(int32_t pid, const ChildProcessInfo& info);

    bool SetWindowInfo(int32_t pid, int32_t widget_id, void* window);

    void OnProcessCreated(int pid, OHIPCRemoteProxy* proxy);

    void OnProcessDestroyed(int pid);

    bool SendInputEvent(int pid, const input_event& event);

private:
    std::map<int, std::shared_ptr<ChildProcessProxy>> childProcesses_;
    std::map<int, ChildProcessInfo> childProcessInfos_;
    std::shared_ptr<ProcessManagerStub> processManagerStub_ = std::make_shared<ProcessManagerStub>();
};
} // namespace ohos::adapter::multiprocess

#endif // OHOS_ADAPTER_MULTIPROCESS_CHILD_PROCESS_CHILD_PROCESS_HOST_H_
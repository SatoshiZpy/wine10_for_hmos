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

#include "ohos/adapter/multiprocess/child_process/child_process_proxy.h"

#include "ohos/adapter/common/logging.h"
#include "ohos/adapter/ipc/native_ipc.h"
#include "ohos/adapter/multiprocess/child_process/native_window_parcel.h"

namespace ohos::adapter::multiprocess {

ChildProcessProxy::ChildProcessProxy(OHIPCRemoteProxy* remote, std::vector<std::shared_ptr<StubHolder>> processStubs)
    : IRemoteProxy(remote), processStubs_(processStubs)
{
}


int ChildProcessProxy::SetProcessInfo(const std::vector<std::string>& commands,
                                      const std::vector<std::pair<int, int>>& fds)
{
    NativeIpcParcel data;

    data.Write<int32_t>(commands.size());
    for (const auto& cmd : commands) {
        data.Write(cmd);
    }

    data.Write<int32_t>(fds.size());
    for (const auto& [fd, fdRemap] : fds) {
        FileDescriptor fileDesc = fd;
        data.Write(fileDesc);
        data.Write(fdRemap);
    }

    data.Write<int32_t>(processStubs_.size());
    for(const auto& processStub: processStubs_) {        
        int ret = data.Write(processStub->Get());
    }
    NativeIpcParcel reply;
    int result = SendRequest(COMMAND_SET_PROCESS_INFO, data, reply);
    return result;
}

int ChildProcessProxy::SetWindowInfo(int32_t widget_id, void* window)
{
    NativeIpcParcel data;

    data.Write<int32_t>(widget_id);
    int ret = NativeWindowWriteToParcel(window, data.Get());
    if (ret != 0) {
        LOGE("Write native window into data parcel error: %{public}d", ret);
    }
    NativeIpcParcel reply;
    int result = SendRequest(COMMAND_SET_WINDOW_INFO, data, reply);
    return result;
}

int ChildProcessProxy::SendInputEvent(const input_event& event)
{
    NativeIpcParcel data;
    ipc::InputEventParam::ToParcel(data, event);
    NativeIpcParcel reply;
    int result = SendRequest(COMMAND_SEND_INPUT_EVENT, data, reply);
    return result;
}

ChildProcessProxy::~ChildProcessProxy()
{
    LOGI("~ChildProcessProxy");
}

int ChildProcessProxy::Echo(const std::string& str)
{
    LOGI("%{public}s", str.c_str());
    NativeIpcParcel data;
    data.Write(str);
    NativeIpcParcel reply;
    int result = SendRequest(COMMAND_ECHO, data, reply);
    return result;
}
} // namespace ohos::adapter::multiprocess

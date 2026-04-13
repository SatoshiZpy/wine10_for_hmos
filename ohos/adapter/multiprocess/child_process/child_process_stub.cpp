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

#include "ohos/adapter/multiprocess/child_process/child_process_stub.h"

#include <unistd.h>
#include <IPCKit/ipc_cparcel.h>

#include "ohos/adapter/common/logging.h"
#include "ohos/adapter/multiprocess/child_process/native_window_parcel.h"
#include "ohos/adapter/xcomponent/adapter/input_event_callback_adapter.h"
#include "ohos/adapter/xcomponent/adapter/window_adapter.h"

#include <chrono>
#include <future>
#include <memory>

using namespace std::chrono_literals;
namespace ohos::adapter::multiprocess {

int ChildProcessStub::OnRemoteRequest(uint32_t code, NativeIpcParcel data, NativeIpcParcel reply)
{
    switch (code) {
        case COMMAND_SET_PROCESS_INFO: {
            std::vector<std::string> commands;
            std::vector<std::pair<int, int>> fds;
            int32_t size = 0;
            data.Read(size);
            for (int32_t i = 0; i < size; i++) {
                std::string str;
                data.Read(str);
                commands.push_back(str);
            }
            size = 0;
            data.Read(size);
            for (int32_t i = 0; i < size; i++) {
                FileDescriptor fd;
                data.Read(fd);
                int32_t fdRemap = 0;
                data.Read(fdRemap);
                fds.push_back({fd.fd, fdRemap});
            }

            int32_t processProxySize = 0;
            data.Read(processProxySize);
            for (int32_t i = 0; i < processProxySize; i++) {
                OHIPCRemoteProxy* proxy = nullptr;
                data.Read(proxy);
                std::shared_ptr<RemoteHolder> remote = std::make_shared<RemoteHolder>(proxy);
                processProxy.push_back(remote);
            }
            return SetProcessInfo(commands, fds);
        }
        case COMMAND_ECHO: {
            std::string str;
            data.Read(str);
            return Echo(str);
        }
        case COMMAND_SET_WINDOW_INFO: {
            int32_t widget_id = 0;
            data.Read(widget_id);

            void* window = nullptr;
            int ret = NativeWindowReadFromParcel(data.Get(), &window);
            if (ret != 0 || window == nullptr) {
                LOGE("Read native window from parcel error: %{public}d", ret);
            }
            return SetWindowInfo(widget_id, window);
        }
        case COMMAND_SEND_INPUT_EVENT: {
            input_event event;
            ipc::InputEventParam::FromParcel(data, event);
            return SendInputEvent(event);
        }
    }
    return 0;
}

int ChildProcessStub::SetProcessInfo(const std::vector<std::string>& commands,
                                     const std::vector<std::pair<int, int>>& fds)
{
    commands_ = commands;
    fds_ = fds;
    isReady_.set_value(true);
    return 0;
}

int ChildProcessStub::SetWindowInfo(int32_t widget_id, void* window)
{
    if (widget_id <= 0) {
        LOGE("SetWindowInfo invalid widget_id");
        return -1;
    }
    OHNativeWindow* nativewindow = (OHNativeWindow*)window;
    if (nativewindow == nullptr) {
        LOGE("SetWindowInfo invalid window");
        return -1;
    }
    xcomponent::WindowAdapter::GetInstance().AddWindow(widget_id, nativewindow, getpid());
    return 0;
}

int ChildProcessStub::SendInputEvent(const input_event& event)
{
    xcomponent::InputEventCallbackAdapter::GetInstance().OnInputEventCallBack(event);
    return 0;
}

void ChildProcessStub::WaitReady()
{
    static std::shared_future<bool> ready = isReady_.get_future();
    ready.wait_for(5s);
}

std::vector<std::string> ChildProcessStub::GetCommands()
{
    return commands_;
}

std::vector<std::pair<int, int>> ChildProcessStub::GetFds()
{
    return fds_;
}

int ChildProcessStub::Echo(const std::string& str) 
{
    LOGI("%{public}s", str.c_str());
    return 0;
}
} // namespace ohos::adapter::multiprocess

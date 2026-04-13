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

#ifndef OHOS_ADAPTER_MULTIPROCESS_CHILD_PROCESS_PROCESS_MANAGER_PROXY_H_
#define OHOS_ADAPTER_MULTIPROCESS_CHILD_PROCESS_PROCESS_MANAGER_PROXY_H_

#include "ohos/adapter/multiprocess/child_process/iprocess_manager.h"

namespace ohos::adapter::multiprocess {
class ProcessManagerProxy : public IRemoteProxy<IProcessManager> {
public:
    ProcessManagerProxy(OHIPCRemoteProxy* remote) : IRemoteProxy<IProcessManager>(remote) {}

    int CreateProcess(const std::vector<std::string>& commands, const std::vector<std::pair<int, int>>& fds) override
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
        NativeIpcParcel reply;
        int result = SendRequest(COMMAND_CREATE_PROCESS, data, reply);
        if (result != ERR_OK) {
            return result;
        }
        int ret = 0;
        reply.Read(ret);
        return ret;
    }

    int KillProcess(int32_t pid) override {
        NativeIpcParcel data;
        data.Write<int32_t>(pid);

        NativeIpcParcel reply;
        int result = SendRequest(COMMAND_KILL_PROCESS, data, reply);
        if (result != ERR_OK) {
            return result;
        }
        int ret = 0;
        reply.Read(ret);
        return ret;
    }

    int CreateWindow(const ipc::WindowParam& param) override
    {
        NativeIpcParcel data;
        param.ToParcel(data);

        NativeIpcParcel reply;
        int result = SendRequest(COMMAND_CREATE_WINDOW, data, reply);
        if (result != ERR_OK) {
            return result;
        }
        int ret = 0;
        reply.Read(ret);
        return ret;
    }

    int UpdateWindow(int32_t widget_id, const ipc::WindowParam& param) override
    {
        NativeIpcParcel data;
        data.Write<int32_t>(widget_id);
        param.ToParcel(data);

        NativeIpcParcel reply;
        int result = SendRequest(COMMAND_UPDATE_WINDOW, data, reply);
        if (result != ERR_OK) {
            return result;
        }
        int ret = 0;
        reply.Read(ret);
        return ret;
    }

    int SetWindowBounds(int32_t widget_id, const WindowRect& rect) override
    {
        NativeIpcParcel data;
        data.Write<int32_t>(widget_id);
        data.Write<int32_t>(rect.left);
        data.Write<int32_t>(rect.top);
        data.Write<int32_t>(rect.width);
        data.Write<int32_t>(rect.height);

        NativeIpcParcel reply;
        int result = SendRequest(COMMAND_SET_BOUNDS, data, reply);
        if (result != ERR_OK) {
            return result;
        }
        int ret = 0;
        reply.Read(ret);
        return ret;
    }

    int SetWindowVisible(int widget_id, int visible) override
    {
        NativeIpcParcel data;
        data.Write<int32_t>(widget_id);
        data.Write<int32_t>(visible);

        NativeIpcParcel reply;
        int result = SendRequest(COMMAND_SET_VISIBLE, data, reply);
        if (result != ERR_OK) {
            return result;
        }
        int ret = 0;
        reply.Read(ret);
        return ret;
    }

    int CloseWindow(int32_t widget_id) override
    {
        NativeIpcParcel data;
        data.Write<int32_t>(widget_id);

        NativeIpcParcel reply;
        int result = SendRequest(COMMAND_CLOSE_WINDOW, data, reply);
        if (result != ERR_OK) {
            return result;
        }
        int ret = 0;
        reply.Read(ret);
        return ret;
    }

    int GetDisplay(ipc::DisplayParam& param) override
    {
        NativeIpcParcel data;
        NativeIpcParcel reply;
        int result = SendRequest(COMMAND_GET_DISPLAY, data, reply);
        if (result != ERR_OK) {
            return -1;
        }
        
        param = ipc::DisplayParam::FromParcel(reply);
        int ret = 0;
        reply.Read(ret);
        return ret;        
    }
};
} // namespace ohos::adapter::multiprocess
#endif // OHOS_ADAPTER_MULTIPROCESS_CHILD_PROCESS_PROCESS_MANAGER_PROXY_H_

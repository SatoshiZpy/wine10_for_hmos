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

#ifndef OHOS_ADAPTER_MULTIPROCESS_CHILD_PROCESS_PROCESS_MANAGER_STUB_H_
#define OHOS_ADAPTER_MULTIPROCESS_CHILD_PROCESS_PROCESS_MANAGER_STUB_H_

#include "ohos/adapter/multiprocess/child_process/child_process_helper.h"
#include "ohos/adapter/multiprocess/child_process/iprocess_manager.h"

namespace ohos::adapter::multiprocess {
class ProcessManagerStub : public IRemoteStub<IProcessManager> {
public:
    ProcessManagerStub() = default;

    int OnRemoteRequest(uint32_t code, NativeIpcParcel data, NativeIpcParcel reply) override
    {
        switch (code) {
            case COMMAND_CREATE_PROCESS: {
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
                int ret = CreateProcess(commands, fds);
                reply.Write(ret);
                return ERR_OK;
            }
            case COMMAND_KILL_PROCESS: {
                int32_t pid = 0;
                data.Read(pid);
                int ret = KillProcess(pid);
                reply.Write(ret);
                return ERR_OK;
            }
            case COMMAND_CREATE_WINDOW: {
                auto param = ipc::WindowParam::FromParcel(data);
                int ret = CreateWindow(param);
                reply.Write(ret);
                return ERR_OK;
            }
            case COMMAND_SET_BOUNDS: {
                int32_t widget_id = 0;
                data.Read(widget_id);
                WindowRect rect;
                data.Read(rect.left);
                data.Read(rect.top);
                data.Read(rect.width);
                data.Read(rect.height);
                int ret = SetWindowBounds(widget_id, rect);
                reply.Write(ret);
                return ERR_OK;
            }
            case COMMAND_SET_VISIBLE: {
                int32_t widget_id = 0;
                data.Read(widget_id);
                int32_t visible = 0;
                data.Read(visible);
                int ret = SetWindowVisible(widget_id, visible);
                reply.Write(ret);
                return ERR_OK;
            }
            case COMMAND_UPDATE_WINDOW: {
                int32_t widget_id = 0;
                data.Read(widget_id);
                auto param = ipc::WindowParam::FromParcel(data);
                int ret = UpdateWindow(widget_id, param);
                reply.Write(ret);
                return ERR_OK;
            }
            case COMMAND_CLOSE_WINDOW: {
                int32_t widget_id = 0;
                data.Read(widget_id);
                int ret = CloseWindow(widget_id);
                reply.Write(ret);
                return ERR_OK;
            }
            case COMMAND_GET_DISPLAY: {
                ipc::DisplayParam param;
                int ret = GetDisplay(param);
                param.ToParcel(reply);
                reply.Write(ret);
                return ERR_OK;
            }
            case COMMAND_OPEN_URL: {
                std::string url;
                data.Read(url);
                int ret = OpenUrl(url);
                reply.Write(ret);
                return ERR_OK;
            }
        }
        return ERR_OK;
    }

    int CreateProcess(const std::vector<std::string>& cmds, const std::vector<std::pair<int, int>>& fds) override
    {
        return ChildProcessHelper::CreateProcess(cmds, fds);
    }

    int KillProcess(int32_t pid) override
    {
        return ChildProcessHelper::KillProcess(pid);
    }

    int CreateWindow(const ipc::WindowParam& param) override
    {
        return ChildProcessHelper::CreateWindow(param);
    }

    int SetWindowBounds(int32_t widget_id, const WindowRect& rect) override
    {
        return ChildProcessHelper::SetWindowBounds(widget_id, rect);
    }

    int SetWindowVisible(int widget_id, int visible) override
    {
        return ChildProcessHelper::SetWindowVisible(widget_id, visible);
    }

    int UpdateWindow(int32_t widget_id, const ipc::WindowParam& param) override
    {
        return ChildProcessHelper::UpdateWindow(widget_id, param);
    }

    int CloseWindow(int32_t widget_id) override
    {
        return ChildProcessHelper::CloseWindow(widget_id);
    }

    int GetDisplay(ipc::DisplayParam& param) override
    {
        return ChildProcessHelper::GetDisplay(param);
    }

    int OpenUrl(const std::string& url) override
    {
        return ChildProcessHelper::OpenUrl(url);
    }
};
} // namespace ohos::adapter::multiprocess
#endif // OHOS_ADAPTER_MULTIPROCESS_CHILD_PROCESS_PROCESS_MANAGER_STUB_H_
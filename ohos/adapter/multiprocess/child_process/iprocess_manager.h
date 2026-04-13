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

#ifndef OHOS_ADAPTER_MULTIPROCESS_CHILD_PROCESS_IPROCESS_H_
#define OHOS_ADAPTER_MULTIPROCESS_CHILD_PROCESS_IPROCESS_H_

#include <cstdint>
#include <string>

#include "ohos/adapter/common/constants.h"
#include "ohos/adapter/ipc/display_param.h"
#include "ohos/adapter/ipc/window_param.h"

namespace ohos::adapter::multiprocess {
class IProcessManager {
public:
    inline static std::string INTERFACE_DESCRIPTOR = "wine.processmanager";
    virtual ~IProcessManager() = default;
    virtual int CreateProcess(const std::vector<std::string>& cmds, const std::vector<std::pair<int, int>>& fds) = 0;
    virtual int KillProcess(int32_t pid) = 0;
    virtual int CreateWindow(const ipc::WindowParam& param) = 0;
    virtual int UpdateWindow(int32_t widget_id, const ipc::WindowParam& param) = 0;
    virtual int SetWindowBounds(int32_t widget_id, const WindowRect& rect) = 0;
    virtual int SetWindowVisible(int widget_id, int visible) = 0;
    virtual int CloseWindow(int32_t widget_id) = 0;
    virtual int GetDisplay(ipc::DisplayParam& display) = 0;
};

enum IProcessManagerIpcCode : uint32_t {
    COMMAND_CREATE_PROCESS = MIN_TRANSACTION_ID,
    COMMAND_KILL_PROCESS,
    COMMAND_CREATE_WINDOW,
    COMMAND_SET_BOUNDS,
    COMMAND_SET_VISIBLE,
    COMMAND_UPDATE_WINDOW,
    COMMAND_CLOSE_WINDOW,
    COMMAND_GET_DISPLAY,
};
} // namespace ohos::adapter::multiprocess
#endif //OHOS_ADAPTER_MULTIPROCESS_CHILD_PROCESS_IPROCESS_H_
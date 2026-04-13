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

#ifndef OHOS_ADAPTER_MULTIPROCESS_CHILD_PROCESS_ICHILD_PROCESS_H_
#define OHOS_ADAPTER_MULTIPROCESS_CHILD_PROCESS_ICHILD_PROCESS_H_

#include <memory>

#include "ohos/adapter/include/input_event_callback.h"
#include "ohos/adapter/ipc/input_event_callback_param.h"
#include "ohos/adapter/ipc/native_ipc.h"

namespace ohos::adapter::multiprocess {
class IChildProcess {
public:
    inline static std::string INTERFACE_DESCRIPTOR = "wine.childprocess";
    virtual ~IChildProcess() = default;

    virtual int SetProcessInfo(const std::vector<std::string>& commands,
                               const std::vector<std::pair<int, int>>& fds) = 0;
    virtual int Echo(const std::string& str) =0;
    virtual int SetWindowInfo(int32_t widget_id, void* window) = 0;
    virtual int SendInputEvent(const input_event& event) = 0;
};

enum IChildProcessIpcCode : uint32_t {
    COMMAND_SET_PROCESS_INFO = MIN_TRANSACTION_ID,
    COMMAND_ECHO,
    COMMAND_SET_WINDOW_INFO,
    COMMAND_SEND_INPUT_EVENT,
};

} // namespace ohos::adapter::multiprocess
#endif // OHOS_ADAPTER_MULTIPROCESS_CHILD_PROCESS_ICHILD_PROCESS_H_
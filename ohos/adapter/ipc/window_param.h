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

#ifndef OHOS_ADAPTER_IPC_WINDOW_PARAM_H_
#define OHOS_ADAPTER_IPC_WINDOW_PARAM_H_

#include "ohos/adapter/ipc/native_ipc.h"

namespace ohos::adapter::ipc {

struct WindowParam {
    int32_t ability_type;
    int32_t window_type;
    int32_t left;
    int32_t top;
    int32_t width;
    int32_t height;
    int32_t parent_id;
    int32_t hwnd;
    int32_t owner_pid;
    int64_t features;
    int64_t window_state;

    static WindowParam FromParcel(const multiprocess::NativeIpcParcel& parcel) {
        WindowParam param;
        parcel.Read(param.ability_type);
        parcel.Read(param.window_type);
        parcel.Read(param.left);
        parcel.Read(param.top);
        parcel.Read(param.width);
        parcel.Read(param.height);
        parcel.Read(param.parent_id);
        parcel.Read(param.hwnd);
        parcel.Read(param.owner_pid);
        parcel.Read(param.features);
        parcel.Read(param.window_state);
        return param;
    }

    bool ToParcel(multiprocess::NativeIpcParcel& parcel) const {
        parcel.Write(ability_type);
        parcel.Write(window_type);
        parcel.Write(left);
        parcel.Write(top);
        parcel.Write(width);
        parcel.Write(height);
        parcel.Write(parent_id);
        parcel.Write(hwnd);
        parcel.Write(owner_pid);
        parcel.Write(features);
        parcel.Write(window_state);
        return true;
    }
};
} // namespace ohos::adapter::ipc

#endif // OHOS_ADAPTER_IPC_WINDOW_PARAM_H_

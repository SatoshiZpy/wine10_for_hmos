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

#ifndef OHOS_ADAPTER_IPC_DISPLAY_PARAM_H_
#define OHOS_ADAPTER_IPC_DISPLAY_PARAM_H_

#include "ohos/adapter/ipc/native_ipc.h"

namespace ohos::adapter::ipc {

struct DisplayParam {
    int32_t id = -1;
    std::string name;
    int32_t left = 0;
    int32_t top = 0;
    int32_t width = 0;
    int32_t height = 0;
    float density_dpi = 0.0;
    float x_dpi = 0.0;
    float y_dpi = 0.0;

    static DisplayParam FromParcel(const multiprocess::NativeIpcParcel& parcel) {
        DisplayParam param;
        parcel.Read(param.id);
        parcel.Read(param.name);
        parcel.Read(param.left);
        parcel.Read(param.top);
        parcel.Read(param.width);
        parcel.Read(param.height);
        parcel.Read(param.density_dpi);
        parcel.Read(param.x_dpi);
        parcel.Read(param.y_dpi);
        return param;
    }

    bool ToParcel(multiprocess::NativeIpcParcel& parcel) const {
        parcel.Write(id);
        parcel.Write(name);
        parcel.Write(left);
        parcel.Write(top);
        parcel.Write(width);
        parcel.Write(height);
        parcel.Write(density_dpi);
        parcel.Write(x_dpi);
        parcel.Write(y_dpi);
        return true;
    }
};
} // namespace ohos::adapter::ipc

#endif // OHOS_ADAPTER_IPC_DISPLAY_PARAM_H_

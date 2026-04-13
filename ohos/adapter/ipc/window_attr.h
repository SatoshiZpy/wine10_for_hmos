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

#ifndef OHOS_ADAPTER_IPC_WINDOW_ATTR_H_
#define OHOS_ADAPTER_IPC_WINDOW_ATTR_H_


#include "ohos/adapter/ipc/native_ipc.h"

namespace ohos::adapter::ipc {

struct WindowAttr {
    int32_t width;
    int32_t height;
    int32_t format;
    int32_t stride;
    int32_t swapInterval;
    int32_t timeout;
    int32_t colorGamut;
    int32_t transform;
    int32_t sourceType;
    int64_t usage;

    static WindowAttr FromParcel(const multiprocess::NativeIpcParcel& parcel) {
        WindowAttr attr;
        parcel.Read(attr.width);
        parcel.Read(attr.height);
        parcel.Read(attr.format);
        parcel.Read(attr.stride);
        parcel.Read(attr.swapInterval);
        parcel.Read(attr.timeout);
        parcel.Read(attr.colorGamut);
        parcel.Read(attr.transform);
        parcel.Read(attr.sourceType);
        parcel.Read(attr.usage);
        return attr;
    }

    bool ToParcel(multiprocess::NativeIpcParcel& parcel) const {
        parcel.Write(width);
        parcel.Write(height);
        parcel.Write(format);
        parcel.Write(stride);
        parcel.Write(swapInterval);
        parcel.Write(timeout);
        parcel.Write(colorGamut);
        parcel.Write(transform);
        parcel.Write(sourceType);
        parcel.Write(usage);
        return true;
    }
};
} // namespace ohos::adapter::ipc

#endif // OHOS_ADAPTER_IPC_WINDOW_ATTR_H_

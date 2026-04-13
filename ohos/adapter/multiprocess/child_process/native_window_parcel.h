// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_MULTIPROCESS_NATIVE_WINDOW_PARCEL_H
#define OHOS_ADAPTER_MULTIPROCESS_NATIVE_WINDOW_PARCEL_H

#include "ohos/adapter/ipc/native_ipc.h"

namespace ohos::adapter::multiprocess {

int NativeWindowAttrToParcel(void* window, OHIPCParcel* data_parcel);

int NativeWindowAttrFromParcel(const OHIPCParcel* data_parcel, void* window);

int NativeWindowWriteToParcel(void* window, OHIPCParcel* data_parcel);

int NativeWindowReadFromParcel(const OHIPCParcel* data_parcel, void** window);

}  // namespace ohos::adapter::multiprocess

#endif  // OHOS_ADAPTER_MULTIPROCESS_NATIVE_WINDOW_PARCEL_H

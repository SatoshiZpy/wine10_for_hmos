// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_NEW_IPC_IPC_PARCEL_H_
#define OHOS_ADAPTER_NEW_IPC_IPC_PARCEL_H_

#include <cstdint>
#include <string>
#include <vector>

#include "ohos/adapter/new_ipc/shared_memory.h"

namespace ohos::adapter::new_ipc {

class IpcParcel {
public:
    explicit IpcParcel(SharedMemory& shm);
    ~IpcParcel() = default;

    bool WriteInt8(int8_t value);
    bool WriteInt16(int16_t value);
    bool WriteInt32(int32_t value);
    bool WriteInt64(int64_t value);
    bool WriteFloat(float value);
    bool WriteDouble(double value);
    bool WriteString(const std::string& value);
    bool WriteRaw(const void* data, uint32_t length);
    bool WriteBool(bool value);

    bool ReadInt8(int8_t& value);
    bool ReadInt16(int16_t& value);
    bool ReadInt32(int32_t& value);
    bool ReadInt64(int64_t& value);
    bool ReadFloat(float& value);
    bool ReadDouble(double& value);
    bool ReadString(std::string& value);
    bool ReadRaw(void* buffer, uint32_t& length);
    bool ReadBool(bool& value);

    bool HasData() const;

private:
    SharedMemory& shm_;
};

}  // namespace ohos::adapter::new_ipc

#endif  // OHOS_ADAPTER_NEW_IPC_IPC_PARCEL_H_
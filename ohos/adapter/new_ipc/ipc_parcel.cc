// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/new_ipc/ipc_parcel.h"

#include "ohos/adapter/common/logging.h"

namespace ohos::adapter::new_ipc {

IpcParcel::IpcParcel(SharedMemory& shm) : shm_(shm) {}

bool IpcParcel::WriteInt8(int8_t value) { return shm_.WriteRaw(&value, sizeof(value)); }
bool IpcParcel::WriteInt16(int16_t value) { return shm_.WriteRaw(&value, sizeof(value)); }
bool IpcParcel::WriteInt32(int32_t value) { return shm_.WriteInt32(value); }
bool IpcParcel::WriteInt64(int64_t value) { return shm_.WriteInt64(value); }
bool IpcParcel::WriteFloat(float value) { return shm_.WriteFloat(value); }
bool IpcParcel::WriteDouble(double value) { return shm_.WriteDouble(value); }
bool IpcParcel::WriteString(const std::string& value) { return shm_.WriteString(value); }
bool IpcParcel::WriteRaw(const void* data, uint32_t length) { return shm_.WriteRaw(data, length); }
bool IpcParcel::WriteBool(bool value) { int8_t v = value ? 1 : 0; return shm_.WriteRaw(&v, sizeof(v)); }

bool IpcParcel::ReadInt8(int8_t& value) { uint32_t len = sizeof(int8_t); return shm_.ReadRaw(&value, len); }
bool IpcParcel::ReadInt16(int16_t& value) { uint32_t len = sizeof(int16_t); return shm_.ReadRaw(&value, len); }
bool IpcParcel::ReadInt32(int32_t& value) { return shm_.ReadInt32(value); }
bool IpcParcel::ReadInt64(int64_t& value) { return shm_.ReadInt64(value); }
bool IpcParcel::ReadFloat(float& value) { return shm_.ReadFloat(value); }
bool IpcParcel::ReadDouble(double& value) { return shm_.ReadDouble(value); }
bool IpcParcel::ReadString(std::string& value) { return shm_.ReadString(value); }
bool IpcParcel::ReadRaw(void* buffer, uint32_t& length) { return shm_.ReadRaw(buffer, length); }
bool IpcParcel::ReadBool(bool& value)
{
    int8_t v = 0;
    uint32_t len = sizeof(int8_t);
    if (!shm_.ReadRaw(&v, len)) return false;
    value = (v != 0);
    return true;
}
bool IpcParcel::HasData() const { return shm_.HasDataToRead(); }

}  // namespace ohos::adapter::new_ipc
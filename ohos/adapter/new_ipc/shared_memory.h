// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_NEW_IPC_SHARED_MEMORY_H_
#define OHOS_ADAPTER_NEW_IPC_SHARED_MEMORY_H_

#include <cstdint>
#include <string>
#include <vector>

#include "ohos/adapter/common/mutex.h"

namespace ohos::adapter::new_ipc {

enum class SharedMemoryType : uint8_t {
    INT32   = 1,
    INT64   = 2,
    FLOAT   = 3,
    DOUBLE  = 4,
    STRING  = 5,
    RAW     = 6,
};

struct SharedMemoryHeader {
    uint32_t total_size;
    uint32_t write_offset;
    uint32_t read_offset;
    uint32_t entry_count;
};

struct SharedMemoryEntryHeader {
    SharedMemoryType type;
    uint8_t reserved;
    uint16_t padding;
    uint32_t data_length;
};

constexpr uint32_t SHARED_MEMORY_HEADER_SIZE = sizeof(SharedMemoryHeader);
constexpr uint32_t SHARED_MEMORY_ENTRY_HEADER_SIZE = sizeof(SharedMemoryEntryHeader);
constexpr uint32_t SHARED_MEMORY_DEFAULT_SIZE = 4096;
constexpr uint32_t SHARED_MEMORY_MAX_SIZE = 1024 * 1024;

class SharedMemory {
public:
    SharedMemory();
    ~SharedMemory();

    bool Create(uint32_t size = SHARED_MEMORY_DEFAULT_SIZE);
    bool Attach(int fd);
    void Detach();
    void Close();

    int GetFd() const;
    uint32_t GetSize() const;
    void* GetData() const;
    SharedMemoryHeader* GetHeader() const;

    bool WriteInt32(int32_t value);
    bool WriteInt64(int64_t value);
    bool WriteFloat(float value);
    bool WriteDouble(double value);
    bool WriteString(const std::string& value);
    bool WriteRaw(const void* data, uint32_t length);

    bool ReadInt32(int32_t& value);
    bool ReadInt64(int64_t& value);
    bool ReadFloat(float& value);
    bool ReadDouble(double& value);
    bool ReadString(std::string& value);
    bool ReadRaw(void* buffer, uint32_t& length);

    uint32_t GetWriteOffset() const;
    uint32_t GetReadOffset() const;
    uint32_t GetAvailableWriteSpace() const;
    uint32_t GetAvailableReadData() const;
    bool HasDataToRead() const;

    void ResetReadWrite();
    void SetReadOffset(uint32_t offset);
    void ResetAll();

    bool IsCreated() const { return created_; }
    bool IsAttached() const { return attached_; }

private:
    bool WriteEntry(SharedMemoryType type, const void* data, uint32_t length);
    bool ReadEntry(SharedMemoryEntryHeader& header_out, void* data, uint32_t& length_out);

    int fd_ = -1;
    void* mapped_ = nullptr;
    uint32_t mapped_size_ = 0;
    bool created_ = false;
    bool attached_ = false;
    Mutex mutex_;
};

}  // namespace ohos::adapter::new_ipc

#endif  // OHOS_ADAPTER_NEW_IPC_SHARED_MEMORY_H_
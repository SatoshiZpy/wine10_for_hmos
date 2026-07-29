// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/new_ipc/shared_memory.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "ohos/adapter/common/logging.h"

#ifndef __OHOS__
#include <linux/ashmem.h>
#else
#define ASHMEM_SET_SIZE     _IOW(0x77, 3, size_t)
#define ASHMEM_GET_SIZE     _IOR(0x77, 4, size_t)
#endif

namespace ohos::adapter::new_ipc {

SharedMemory::SharedMemory() = default;

SharedMemory::~SharedMemory()
{
    Detach();
    Close();
}

bool SharedMemory::Create(uint32_t size)
{
    MutexLock lock(mutex_);
    if (size == 0 || size > SHARED_MEMORY_MAX_SIZE) {
        LOGE("SharedMemory::Create invalid size %{public}u", size);
        return false;
    }
    if (fd_ >= 0) {
        LOGE("SharedMemory::Create already has fd");
        return false;
    }

    int ashmem_fd = open("/dev/ashmem", O_RDWR);
    if (ashmem_fd < 0) {
        LOGE("SharedMemory::Create open /dev/ashmem failed, errno=%{public}d", errno);
        return false;
    }

    if (ioctl(ashmem_fd, ASHMEM_SET_SIZE, static_cast<size_t>(size)) < 0) {
        LOGE("SharedMemory::Create ASHMEM_SET_SIZE failed, errno=%{public}d", errno);
        close(ashmem_fd);
        return false;
    }

    fd_ = ashmem_fd;
    mapped_size_ = size;
    created_ = true;

    void* addr = mmap(nullptr, mapped_size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
    if (addr == MAP_FAILED) {
        LOGE("SharedMemory::Create mmap failed, errno=%{public}d", errno);
        close(fd_);
        fd_ = -1;
        created_ = false;
        return false;
    }
    mapped_ = addr;

    SharedMemoryHeader* header = static_cast<SharedMemoryHeader*>(mapped_);
    memset(mapped_, 0, mapped_size_);
    header->total_size = mapped_size_;
    header->write_offset = SHARED_MEMORY_HEADER_SIZE;
    header->read_offset = SHARED_MEMORY_HEADER_SIZE;
    header->entry_count = 0;

    LOGI("SharedMemory::Create fd=%{public}d, size=%{public}u", fd_, mapped_size_);
    return true;
}

bool SharedMemory::Attach(int fd)
{
    MutexLock lock(mutex_);
    if (fd < 0) {
        LOGE("SharedMemory::Attach invalid fd %{public}d", fd);
        return false;
    }
    if (fd_ >= 0) {
        LOGE("SharedMemory::Attach already has fd");
        return false;
    }

#ifdef __OHOS__
    size_t size = 0;
    if (ioctl(fd, ASHMEM_GET_SIZE, &size) < 0) {
        LOGE("SharedMemory::Attach ASHMEM_GET_SIZE failed, errno=%{public}d", errno);
        return false;
    }
    if (size == 0 || size > SHARED_MEMORY_MAX_SIZE) {
        LOGE("SharedMemory::Attach invalid size %{public}zu", size);
        return false;
    }
    mapped_size_ = static_cast<uint32_t>(size);
#else
    struct stat st;
    if (fstat(fd, &st) < 0) {
        LOGE("SharedMemory::Attach fstat failed, errno=%{public}d", errno);
        return false;
    }
    mapped_size_ = static_cast<uint32_t>(st.st_size);
#endif

    fd_ = dup(fd);
    if (fd_ < 0) {
        LOGE("SharedMemory::Attach dup failed, errno=%{public}d", errno);
        return false;
    }

    void* addr = mmap(nullptr, mapped_size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
    if (addr == MAP_FAILED) {
        LOGE("SharedMemory::Attach mmap failed, errno=%{public}d", errno);
        close(fd_);
        fd_ = -1;
        return false;
    }
    mapped_ = addr;
    attached_ = true;

    LOGI("SharedMemory::Attach fd=%{public}d, size=%{public}u", fd_, mapped_size_);
    return true;
}

void SharedMemory::Detach()
{
    if (mapped_ && mapped_ != MAP_FAILED) {
        munmap(mapped_, mapped_size_);
        mapped_ = nullptr;
    }
}

void SharedMemory::Close()
{
    if (fd_ >= 0) {
        close(fd_);
        fd_ = -1;
    }
    created_ = false;
    attached_ = false;
    mapped_size_ = 0;
}

int SharedMemory::GetFd() const { return fd_; }
uint32_t SharedMemory::GetSize() const { return mapped_size_; }
void* SharedMemory::GetData() const { return mapped_; }
SharedMemoryHeader* SharedMemory::GetHeader() const
{
    if (!mapped_) return nullptr;
    return static_cast<SharedMemoryHeader*>(mapped_);
}

bool SharedMemory::WriteEntry(SharedMemoryType type, const void* data, uint32_t length)
{
    if (!mapped_) {
        LOGE("SharedMemory::WriteEntry not mapped");
        return false;
    }
    SharedMemoryHeader* header = GetHeader();
    uint32_t total_needed = SHARED_MEMORY_ENTRY_HEADER_SIZE + length;
    if (header->write_offset + total_needed > mapped_size_) {
        LOGE("SharedMemory::WriteEntry out of space, offset=%{public}u, needed=%{public}u, size=%{public}u",
             header->write_offset, total_needed, mapped_size_);
        return false;
    }

    uint8_t* base = static_cast<uint8_t*>(mapped_) + header->write_offset;
    SharedMemoryEntryHeader* entry = reinterpret_cast<SharedMemoryEntryHeader*>(base);
    entry->type = type;
    entry->reserved = 0;
    entry->padding = 0;
    entry->data_length = length;
    if (length > 0 && data) {
        memcpy(base + SHARED_MEMORY_ENTRY_HEADER_SIZE, data, length);
    }
    header->write_offset += total_needed;
    header->entry_count++;
    return true;
}

bool SharedMemory::ReadEntry(SharedMemoryEntryHeader& header_out, void* data, uint32_t& length_out)
{
    if (!mapped_) {
        LOGE("SharedMemory::ReadEntry not mapped");
        return false;
    }
    SharedMemoryHeader* shm_header = GetHeader();
    if (shm_header->read_offset >= shm_header->write_offset) {
        return false;
    }
    if (shm_header->read_offset + SHARED_MEMORY_ENTRY_HEADER_SIZE > shm_header->write_offset) {
        LOGE("SharedMemory::ReadEntry header out of bounds");
        return false;
    }

    uint8_t* base = static_cast<uint8_t*>(mapped_) + shm_header->read_offset;
    SharedMemoryEntryHeader* entry = reinterpret_cast<SharedMemoryEntryHeader*>(base);
    header_out = *entry;

    uint32_t entry_total = SHARED_MEMORY_ENTRY_HEADER_SIZE + entry->data_length;
    if (shm_header->read_offset + entry_total > shm_header->write_offset) {
        LOGE("SharedMemory::ReadEntry data out of bounds");
        return false;
    }

    if (data && length_out >= entry->data_length) {
        memcpy(data, base + SHARED_MEMORY_ENTRY_HEADER_SIZE, entry->data_length);
    }
    length_out = entry->data_length;

    shm_header->read_offset += entry_total;
    shm_header->entry_count--;
    return true;
}

bool SharedMemory::WriteInt32(int32_t value)
{
    MutexLock lock(mutex_);
    return WriteEntry(SharedMemoryType::INT32, &value, sizeof(int32_t));
}

bool SharedMemory::WriteInt64(int64_t value)
{
    MutexLock lock(mutex_);
    return WriteEntry(SharedMemoryType::INT64, &value, sizeof(int64_t));
}

bool SharedMemory::WriteFloat(float value)
{
    MutexLock lock(mutex_);
    return WriteEntry(SharedMemoryType::FLOAT, &value, sizeof(float));
}

bool SharedMemory::WriteDouble(double value)
{
    MutexLock lock(mutex_);
    return WriteEntry(SharedMemoryType::DOUBLE, &value, sizeof(double));
}

bool SharedMemory::WriteString(const std::string& value)
{
    MutexLock lock(mutex_);
    return WriteEntry(SharedMemoryType::STRING, value.c_str(), static_cast<uint32_t>(value.size() + 1));
}

bool SharedMemory::WriteRaw(const void* data, uint32_t length)
{
    MutexLock lock(mutex_);
    return WriteEntry(SharedMemoryType::RAW, data, length);
}

bool SharedMemory::ReadInt32(int32_t& value)
{
    MutexLock lock(mutex_);
    SharedMemoryEntryHeader header;
    uint32_t len = sizeof(int32_t);
    if (!ReadEntry(header, &value, len)) return false;
    if (header.type != SharedMemoryType::INT32) return false;
    return true;
}

bool SharedMemory::ReadInt64(int64_t& value)
{
    MutexLock lock(mutex_);
    SharedMemoryEntryHeader header;
    uint32_t len = sizeof(int64_t);
    if (!ReadEntry(header, &value, len)) return false;
    if (header.type != SharedMemoryType::INT64) return false;
    return true;
}

bool SharedMemory::ReadFloat(float& value)
{
    MutexLock lock(mutex_);
    SharedMemoryEntryHeader header;
    uint32_t len = sizeof(float);
    if (!ReadEntry(header, &value, len)) return false;
    if (header.type != SharedMemoryType::FLOAT) return false;
    return true;
}

bool SharedMemory::ReadDouble(double& value)
{
    MutexLock lock(mutex_);
    SharedMemoryEntryHeader header;
    uint32_t len = sizeof(double);
    if (!ReadEntry(header, &value, len)) return false;
    if (header.type != SharedMemoryType::DOUBLE) return false;
    return true;
}

bool SharedMemory::ReadString(std::string& value)
{
    MutexLock lock(mutex_);
    SharedMemoryEntryHeader header;
    uint32_t len = 0;
    if (!ReadEntry(header, nullptr, len)) return false;
    if (header.type != SharedMemoryType::STRING) return false;
    if (len == 0) {
        value = "";
        return true;
    }
    std::vector<char> buf(len);
    uint32_t copied = len;
    memcpy(buf.data(),
            static_cast<uint8_t*>(mapped_) + GetHeader()->read_offset - len, len);
    value = std::string(buf.data(), len > 0 && buf[len - 1] == '\0' ? len - 1 : len);
    return true;
}

bool SharedMemory::ReadRaw(void* buffer, uint32_t& length)
{
    MutexLock lock(mutex_);
    SharedMemoryEntryHeader header;
    uint32_t len = 0;
    if (!ReadEntry(header, nullptr, len)) return false;
    if (header.type != SharedMemoryType::RAW) return false;
    if (len > length) {
        LOGE("SharedMemory::ReadRaw buffer too small, need=%{public}u have=%{public}u", len, length);
        length = len;
        return false;
    }
    memcpy(buffer, static_cast<uint8_t*>(mapped_) + GetHeader()->read_offset - len, len);
    length = len;
    return true;
}

uint32_t SharedMemory::GetWriteOffset() const
{
    if (!mapped_) return 0;
    return GetHeader()->write_offset;
}

uint32_t SharedMemory::GetReadOffset() const
{
    if (!mapped_) return 0;
    return GetHeader()->read_offset;
}

uint32_t SharedMemory::GetAvailableWriteSpace() const
{
    if (!mapped_) return 0;
    return mapped_size_ - GetHeader()->write_offset;
}

uint32_t SharedMemory::GetAvailableReadData() const
{
    if (!mapped_) return 0;
    return GetHeader()->write_offset - GetHeader()->read_offset;
}

bool SharedMemory::HasDataToRead() const
{
    if (!mapped_) return false;
    return GetHeader()->read_offset < GetHeader()->write_offset;
}

void SharedMemory::ResetReadWrite()
{
    MutexLock lock(mutex_);
    if (!mapped_) return;
    SharedMemoryHeader* header = GetHeader();
    header->write_offset = SHARED_MEMORY_HEADER_SIZE;
    header->read_offset = SHARED_MEMORY_HEADER_SIZE;
    header->entry_count = 0;
}

void SharedMemory::ResetAll()
{
    MutexLock lock(mutex_);
    if (!mapped_) return;
    memset(mapped_, 0, mapped_size_);
    SharedMemoryHeader* header = GetHeader();
    header->total_size = mapped_size_;
    header->write_offset = SHARED_MEMORY_HEADER_SIZE;
    header->read_offset = SHARED_MEMORY_HEADER_SIZE;
    header->entry_count = 0;
}

}  // namespace ohos::adapter::new_ipc
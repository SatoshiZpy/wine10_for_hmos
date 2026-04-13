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

#ifndef OHOS_ADAPTER_MULTIPROCESS_GPU_NATIVE_IPC_PARCEL_H_
#define OHOS_ADAPTER_MULTIPROCESS_GPU_NATIVE_IPC_PARCEL_H_

#include <IPCKit/ipc_cremote_object.h>
#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include <IPCKit/ipc_cparcel.h>
#include <IPCKit/ipc_kit.h>
#include <native_window/external_window.h>

#include "ohos/adapter/common/logging.h"

namespace ohos::adapter::multiprocess {

constexpr int ERR_OK = 0;
constexpr int MIN_TRANSACTION_ID = 0x1;
constexpr int MAX_TRANSACTION_ID = 0x00ffffff;

#define FAILED(errCode) ((errCode) != ERR_OK)

template <class T>
struct RemoveCvRef {
    using type = std::remove_cv_t<std::remove_reference_t<T>>;
};

template <class T>
using RemoveCvRefType = typename RemoveCvRef<T>::type;

class RemoteHolder;
class NativeIpcParcel;

class StubHolder;

struct FileDescriptor {
    FileDescriptor() = default;
    FileDescriptor(int filefd) : fd(filefd) {}
    int fd = 0;
};

struct InterfaceToken {
    std::string token;
};

static constexpr int MAX_MEMORY_SIZE = 204800;

static void* LocalMemoryAllocator(int32_t len)
{
    if (len < 0 || len > MAX_MEMORY_SIZE) {
        return nullptr;
    }
    void* buffer = malloc(len);
    if (buffer == nullptr) {
        return nullptr;
    }
    memset(buffer, 0, len);
    return buffer;
}

class NativeIpcParcel {
public:
    NativeIpcParcel(OHIPCParcel* data, bool isDelete) noexcept : data_parcel_(data, Deleter(isDelete)) {}

    explicit NativeIpcParcel(OHIPCParcel* data) noexcept : data_parcel_(data, Deleter(true)) {}

    NativeIpcParcel() noexcept
    {
        auto ipcptr = OH_IPCParcel_Create();
        data_parcel_ = std::shared_ptr<OHIPCParcel>(ipcptr, Deleter(true));
    }

    NativeIpcParcel(const NativeIpcParcel& other) noexcept : data_parcel_(other.data_parcel_) {}

    NativeIpcParcel(NativeIpcParcel&& other) noexcept : data_parcel_(std::move(other.data_parcel_)) {}

    NativeIpcParcel& operator=(const NativeIpcParcel& other)
    {
        if (this != &other) {
            data_parcel_ = other.data_parcel_;
        }
        return *this;
    }

    NativeIpcParcel& operator=(NativeIpcParcel&& other)
    {
        if (this != &other) {
            data_parcel_ = std::move(other.data_parcel_);
        }
        return *this;
    }

    struct Deleter {
        Deleter(bool isDelete) : isNeedDeleter(isDelete) {}

        bool isNeedDeleter = true;

        void operator()(OHIPCParcel* ipcparcel) const
        {
            if (isNeedDeleter && ipcparcel) {
                OH_IPCParcel_Destroy(ipcparcel);
            }
        }
    };

    virtual ~NativeIpcParcel() {}

    template <class T>
    int Write(T value)
    {
        int result = ERR_OK;
        using ValueType = RemoveCvRefType<decltype(value)>;
        if constexpr (std::is_same_v<ValueType, int8_t>) {
            result = OH_IPCParcel_WriteInt8(Get(), value);
        } else if constexpr (std::is_same_v<ValueType, int16_t>) {
            result = OH_IPCParcel_WriteInt16(Get(), value);
        } else if constexpr (std::is_same_v<ValueType, int32_t> || std::is_same_v<ValueType, int>) {
            result = OH_IPCParcel_WriteInt32(Get(), value);
        } else if constexpr (std::is_same_v<ValueType, int64_t>) {
            result = OH_IPCParcel_WriteInt64(Get(), value);
        } else if constexpr (std::is_same_v<ValueType, float>) {
            result = OH_IPCParcel_WriteFloat(Get(), value);
        } else if constexpr (std::is_same_v<ValueType, double>) {
            result = OH_IPCParcel_WriteDouble(Get(), value);
        } else if constexpr (std::is_same_v<ValueType, std::string>) {
            result = OH_IPCParcel_WriteString(Get(), value.c_str());
        } else if constexpr (std::is_same_v<ValueType, FileDescriptor>) {
            result = OH_IPCParcel_WriteFileDescriptor(Get(), value.fd);
        } else if constexpr (std::is_same_v<ValueType, std::vector<uint8_t>>) {
            result = OH_IPCParcel_WriteBuffer(Get(), value.data(), value.size());
        } else if constexpr (std::is_same_v<ValueType, InterfaceToken>) {
            result = OH_IPCParcel_WriteInterfaceToken(Get(), value.token.data(), value.token.size());
        } else if constexpr (std::is_same_v<ValueType, OHIPCRemoteStub *>) {
            result = OH_IPCParcel_WriteRemoteStub(Get(), value);
        } else if constexpr (std::is_same_v<ValueType, OHNativeWindow*>) {
            result = OH_NativeWindow_WriteToParcel(value, Get());
        } else {
            result = value.ToParcel();
        }
        if (result != ERR_OK) {
            LOGE("%{public}s failed, result=%{public}d", __PRETTY_FUNCTION__, result);
        } else {
            LOGD("%{public}s success, result=%{public}d", __PRETTY_FUNCTION__, result);
        }
        return result;
    }

    template <class T>
    int Read(T& value) const
    {
        int result = ERR_OK;
        using ValueType = RemoveCvRefType<decltype(value)>;
        if constexpr (std::is_same_v<ValueType, int8_t>) {
            result = OH_IPCParcel_ReadInt8(Get(), &value);
        } else if constexpr (std::is_same_v<ValueType, int16_t>) {
            result = OH_IPCParcel_ReadInt16(Get(), &value);
        } else if constexpr (std::is_same_v<ValueType, int32_t> || std::is_same_v<ValueType, int>) {
            result = OH_IPCParcel_ReadInt32(Get(), &value);
        } else if constexpr (std::is_same_v<ValueType, int64_t>) {
            result = OH_IPCParcel_ReadInt64(Get(), &value);
        } else if constexpr (std::is_same_v<ValueType, float>) {
            result = OH_IPCParcel_ReadFloat(Get(), &value);
        } else if constexpr (std::is_same_v<ValueType, double>) {
            result = OH_IPCParcel_ReadDouble(Get(), &value);
        } else if constexpr (std::is_same_v<ValueType, std::string>) {
            const char* str = OH_IPCParcel_ReadString(Get());
            if (str) {
                value = str;
                result = ERR_OK;
            } else {
                result = -1;
            }
        } else if constexpr (std::is_same_v<ValueType, FileDescriptor>) {
            result = OH_IPCParcel_ReadFileDescriptor(Get(), &(value.fd));
        } else if constexpr (std::is_same_v<ValueType, std::vector<uint8_t>>) {
            int32_t len = 0;
            const uint8_t* buff = OH_IPCParcel_ReadBuffer(Get(), len);
            if (buff) {
                value = std::vector<uint8_t>(buff, buff + len);
            }
        } else if constexpr (std::is_same_v<ValueType, InterfaceToken>) {
            char* token = nullptr;
            int32_t len = 0;
            int ret = OH_IPCParcel_ReadInterfaceToken(Get(), (char**)&token, &len, LocalMemoryAllocator);
            if (token != nullptr) {
                value.token = std::string(token, len);
                free(token);
            }

            result = ret;
        } else if constexpr (std::is_same_v<ValueType, OHIPCRemoteProxy*>) {
            value = OH_IPCParcel_ReadRemoteProxy(Get());
            LOGD("%{public}s OH_IPCParcel_ReadRemoteProxy=%{public}lu", __PRETTY_FUNCTION__, (int64_t)value);
        } else if constexpr (std::is_same_v<ValueType, OHNativeWindow*>) {
            result = OH_NativeWindow_ReadFromParcel(Get(), &value);
        }
        if (result != ERR_OK) {
            LOGE("%{public}s failed, result=%{public}d", __PRETTY_FUNCTION__, result);
        } else {
            LOGD("%{public}s success, result=%{public}d", __PRETTY_FUNCTION__, result);
        }
        return result;
    }

    OHIPCParcel* Get() const
    {
        return data_parcel_.get();
    }

private:
    std::shared_ptr<OHIPCParcel> data_parcel_{nullptr};
};

class RemoteHolder : public std::enable_shared_from_this<RemoteHolder> {
public:
    RemoteHolder() = default;

    RemoteHolder(OHIPCRemoteProxy* remote) : remote_(remote)
    {
    }

    virtual ~RemoteHolder()
    {
        if (remote_) {
            OH_IPCRemoteProxy_Destroy(remote_);
        }
    }

    int SendRequest(uint32_t code, const NativeIpcParcel& data, const NativeIpcParcel& reply)
    {
        if (!remote_) {
            LOGE("SendRequest failed, remote is null");
            return -1;
        }
        return OH_IPCRemoteProxy_SendRequest(remote_, code, data.Get(), reply.Get(), nullptr);
    }

    bool IsRemoteDead()
    {
        return OH_IPCRemoteProxy_IsRemoteDead(remote_);
    }

    OHIPCRemoteProxy* Get()
    {
        return remote_;
    }

    std::string GetInterfaceToken()
    {
        char* token = nullptr;
        int32_t len = 0;
        OH_IPCRemoteProxy_GetInterfaceDescriptor(remote_, &token, &len, LocalMemoryAllocator);
        if (token != nullptr) {
            std::string tokenStr(token, len);
            return tokenStr;
        }
        return "";
    }

private:
    OHIPCRemoteProxy* remote_ = nullptr;
};

class StubHolder {
public:
    StubHolder() = default;
    StubHolder(const std::string& descriptor) : descriptor(descriptor)
    {
        remote_ = OH_IPCRemoteStub_Create(descriptor.c_str(), OnIpcRemoteRequest, OnIpcRemoteDestroy, this);
    };
    StubHolder(OHIPCRemoteStub* remote) : remote_(remote) {}
    
    // 禁用复制构造函数
    StubHolder(const StubHolder&) = delete;
    
    // 禁用复制赋值操作符
    StubHolder& operator=(const StubHolder&) = delete;
    
    // 移动构造函数
    StubHolder(StubHolder&& other) noexcept 
        : remote_(other.remote_)
        , descriptor(std::move(other.descriptor))
    {
        // 将源对象的 remote_ 设置为 nullptr，防止析构时重复释放
        other.remote_ = nullptr;
    }
    
    // 移动赋值操作符
    StubHolder& operator=(StubHolder&& other) noexcept
    {
        if (this != &other) {
            // 释放当前对象的资源
            if (remote_) {
                OH_IPCRemoteStub_Destroy(remote_);
            }
            
            // 转移资源
            remote_ = other.remote_;
            descriptor = std::move(other.descriptor);
            
            // 将源对象的 remote_ 设置为 nullptr，防止析构时重复释放
            other.remote_ = nullptr;
        }
        return *this;
    }

    static int OnIpcRemoteRequest(uint32_t code, const OHIPCParcel* data, OHIPCParcel* reply, void* userData)
    {
        StubHolder* ptr = static_cast<StubHolder*>(userData);
        if (ptr != nullptr) {
            NativeIpcParcel parcelData(const_cast<OHIPCParcel*>(data), false);
            NativeIpcParcel replyData(const_cast<OHIPCParcel*>(reply), false);
            ptr->OnRemoteRequest(code, parcelData, replyData);
        }
        return ERR_OK;
    }

    static void OnIpcRemoteDestroy(void* userData)
    {
        StubHolder* holder = static_cast<StubHolder*>(userData);
        if (holder) {
            LOGD("StubHolder destroyed, descriptor: %{public}s", holder->descriptor.c_str());
        }
    }

    virtual ~StubHolder()
    {
        if (remote_) {
            OH_IPCRemoteStub_Destroy(remote_);
        }
    }

    virtual int OnRemoteRequest(uint32_t code, NativeIpcParcel data, NativeIpcParcel reply) {
        return 0;
    }

    OHIPCRemoteStub* Get()
    {
        return remote_;
    }

private:
    OHIPCRemoteStub* remote_ = nullptr;
    std::string descriptor;
};

template <typename INTERFACE>
class IRemoteProxy
    : public RemoteHolder
    , public INTERFACE {
public:
    explicit IRemoteProxy(OHIPCRemoteProxy* remote) : RemoteHolder(remote) {}
    ~IRemoteProxy() override
    {
        LOGI("~IRemoteProxy %{public}ld", (int64_t)this);
    };
};

template <typename INTERFACE>
class IRemoteStub
    : public StubHolder
    , public INTERFACE {
public:
    IRemoteStub()
        : StubHolder(INTERFACE::INTERFACE_DESCRIPTOR){

          };
    IRemoteStub(OHIPCRemoteStub* remote) : StubHolder(remote) {}

    IRemoteStub(StubHolder&& remote) : StubHolder(std::move(remote)) {}
};
} // namespace ohos::adapter::multiprocess
#endif // OHOS_ADAPTER_MULTIPROCESS_GPU_NATIVE_IPC_PARCEL_H_

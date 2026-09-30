// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_NEW_IPC_IPC_PROXY_STUB_H_
#define OHOS_ADAPTER_NEW_IPC_IPC_PROXY_STUB_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "ohos/adapter/new_ipc/ipc_channel.h"
#include "ohos/adapter/new_ipc/ipc_types.h"

namespace ohos::adapter::new_ipc {

using IpcRequestHandler = std::function<int(uint32_t code, IpcParcel& data, IpcParcel& reply, const std::vector<int>& fds)>;

class IpcProxy {
public:
    explicit IpcProxy(IpcChannel& channel);
    ~IpcProxy() = default;

    bool SendRequest(uint32_t code, IpcParcel& data, IpcParcel& reply);

    IpcChannel& GetChannel() { return channel_; }

private:
    IpcChannel& channel_;
};

class IpcStub {
public:
    explicit IpcStub(const std::string& descriptor);
    ~IpcStub() = default;

    void SetRequestHandler(IpcRequestHandler handler);
    int OnRemoteRequest(uint32_t code, IpcParcel& data, IpcParcel& reply,
                        const std::vector<int>& fds = {});

    const std::string& GetDescriptor() const { return descriptor_; }

private:
    std::string descriptor_;
    IpcRequestHandler handler_;
};

/* Point-to-point proxy: requests from the embedding process to the window
 * owner process (the client surface trio plus diagnostics). */
class ProcessManagerProxy {
public:
    explicit ProcessManagerProxy(IpcChannel& channel);
    ~ProcessManagerProxy() = default;

    int CreateClientSurface(ClientSurfaceParam& param, uint64_t& surface_id);
    int SetClientSurfaceBounds(int32_t client_widget_id, int32_t off_x,
                               int32_t off_y, int32_t width, int32_t height);
    int DestroyClientSurface(int32_t client_widget_id);
    int GetPid(int32_t& pid);
    /* GDI sharing: the fd is passed to the owner process with the request */
    int AttachRemoteGdi(RemoteGdiParam& param, int fd);
    int RemoteGdiFlush(int32_t client_widget_id, int32_t x, int32_t y,
                       int32_t w, int32_t h);
    int DestroyRemoteGdi(int32_t client_widget_id);
    int UpdateRemoteGdiBounds(int32_t client_widget_id, int32_t off_x,
                              int32_t off_y);
    int SetRemoteGdiVisible(int32_t client_widget_id, int32_t visible);

private:
    IpcChannel& channel_;
};

/* Stub shared by every wine process: creates/adjusts/destroys client
 * surfaces (XComponents) in its own pages and returns the surface id. */
class ProcessManagerStub {
public:
    ProcessManagerStub();
    ~ProcessManagerStub() = default;

    int OnRemoteRequest(uint32_t code, IpcParcel& data, IpcParcel& reply,
                        const std::vector<int>& fds = {});

    static constexpr const char* INTERFACE_DESCRIPTOR = "wine.processmanager.new_ipc";
};

}  // namespace ohos::adapter::new_ipc

#endif  // OHOS_ADAPTER_NEW_IPC_IPC_PROXY_STUB_H_

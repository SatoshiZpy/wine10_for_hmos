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

/* 点对点 proxy：嵌入方进程向窗口 owner 进程发起的请求（client
 * surface 三件套 + 诊断）。 */
class ProcessManagerProxy {
public:
    explicit ProcessManagerProxy(IpcChannel& channel);
    ~ProcessManagerProxy() = default;

    int CreateClientSurface(ClientSurfaceParam& param, uint64_t& surface_id);
    int SetClientSurfaceBounds(int32_t client_widget_id, int32_t off_x,
                               int32_t off_y, int32_t width, int32_t height);
    int DestroyClientSurface(int32_t client_widget_id);
    int GetPid(int32_t& pid);
    /* GDI 共享：fd 随请求附带传给 owner 进程 */
    int AttachRemoteGdi(RemoteGdiParam& param, int fd);
    int RemoteGdiFlush(int32_t client_widget_id, int32_t x, int32_t y,
                       int32_t w, int32_t h);
    int DestroyRemoteGdi(int32_t client_widget_id);
    int UpdateRemoteGdiBounds(int32_t client_widget_id, int32_t off_x,
                              int32_t off_y);

private:
    IpcChannel& channel_;
};

/* 每个 wine 进程通用 stub：在本进程页面里创建/调整/销毁 client
 * surface（XComponent），回传 surface id。 */
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

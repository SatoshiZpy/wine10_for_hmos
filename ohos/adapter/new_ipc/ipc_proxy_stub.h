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

class ProcessManagerProxy {
public:
    explicit ProcessManagerProxy(IpcChannel& channel);
    ~ProcessManagerProxy() = default;

    int CreateProcess(const std::vector<std::string>& commands,
                     const std::vector<std::pair<int, int>>& fds);
    int KillProcess(int32_t pid);
    int CreateWindow(const WindowParam& param);
    int SetWindowBounds(int32_t widget_id, const WindowRect& rect);
    int SetWindowVisible(int32_t widget_id, bool visible);
    int UpdateWindow(int32_t widget_id, const WindowParam& param);
    int CloseWindow(int32_t widget_id);
    int GetDisplay(DisplayParam& param);
    int OpenUrl(const std::string& url);
    int SetWindowTitle(int32_t widget_id, const std::string& title);
    int GetFdIdsRemap(std::vector<std::pair<int, int>>& fds);
    int GetPid(int32_t& pid);

private:
    IpcChannel& channel_;
};

class ProcessManagerStub {
public:
    ProcessManagerStub();
    ~ProcessManagerStub() = default;

    int OnRemoteRequest(uint32_t code, IpcParcel& data, IpcParcel& reply,
                        const std::vector<int>& fds = {});

    static constexpr const char* INTERFACE_DESCRIPTOR = "wine.processmanager.new_ipc";
};

class ChildProcessProxy {
public:
    explicit ChildProcessProxy(IpcChannel& channel);
    ~ChildProcessProxy() = default;

    int SetProcessInfo(const std::vector<std::string>& commands,
                      const std::vector<std::pair<int, int>>& fds);
    int SetWindowInfo(int32_t widget_id, const WindowAttr& attr);
    int SendInputEvent(const std::vector<int8_t>& event_data);

private:
    IpcChannel& channel_;
};

class ChildProcessStub {
public:
    ChildProcessStub();
    ~ChildProcessStub() = default;

    int OnRemoteRequest(uint32_t code, IpcParcel& data, IpcParcel& reply,
                        const std::vector<int>& fds = {});

    static constexpr const char* INTERFACE_DESCRIPTOR = "wine.childprocess.new_ipc";
};

}  // namespace ohos::adapter::new_ipc

#endif  // OHOS_ADAPTER_NEW_IPC_IPC_PROXY_STUB_H_

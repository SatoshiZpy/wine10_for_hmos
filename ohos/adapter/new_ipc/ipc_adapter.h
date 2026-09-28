// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_NEW_IPC_IPC_ADAPTER_H_
#define OHOS_ADAPTER_NEW_IPC_IPC_ADAPTER_H_

#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "ohos/adapter/common/mutex.h"
#include "ohos/adapter/new_ipc/domain_socket.h"
#include "ohos/adapter/new_ipc/ipc_channel.h"
#include "ohos/adapter/new_ipc/ipc_proxy_stub.h"

namespace ohos::adapter::new_ipc {

/* GDI 共享 overlay 回调：owner 进程的 wine 侧注册，stub 收到
 * attach/flush/destroy 时回调进 wine 完成合成（wine 侧自持锁与线程
 * 模型）。BLIT 的 dirty rect 经 param 的几何字段传递。 */
enum class RemoteGdiOp : int32_t { ADD = 0, BLIT = 1, REMOVE = 2, UPDATE = 3 };
using RemoteGdiOverlayCallback = void (*)(RemoteGdiOp op, int32_t id,
                                          const RemoteGdiParam* param,
                                          const void* pixels);
void SetRemoteGdiOverlayCallback(RemoteGdiOverlayCallback fn);

/* 每个进程一份的 IPC 服务端：以 pid 命名的 unix socket 常驻监听，
 * 等待其他 wine 进程在嵌入本进程窗口时发起连接。进程与窗口的嵌入
 * 关系是任意的（a 可以嵌入 c 的窗口，与谁 spawn 谁无关），因此
 * 每个进程各自持 server，连接按需发起；spawn 链路仍走 ipc_lite。 */
class IpcServer {
public:
    static IpcServer& Instance();

    /* 在 <socket_dir>/wine_ipc_<pid>.sock 上启动常驻监听；幂等 */
    bool Start();
    void Stop();

    static std::string SocketPathForPid(int32_t pid);

private:
    IpcServer();
    ~IpcServer();
    IpcServer(const IpcServer&) = delete;
    IpcServer& operator=(const IpcServer&) = delete;

    void AcceptLoop();

    DomainSocket server_socket_;
    std::map<int32_t, std::unique_ptr<IpcChannel>> channels_;
    ProcessManagerStub stub_;
    Mutex mutex_;
    std::thread accept_thread_;
    std::atomic<bool> accepting_{false};
    bool server_started_ = false;
    int32_t next_channel_id_ = 1;
};

/* 每个进程一份的 IPC 客户端管理器：按 owner pid 连接对端进程的
 * IpcServer 并缓存通道；client surface 三件套经点对点通道转发到
 * 窗口 owner 进程（在其页面里创建 XComponent 并回传 surface id）。 */
class PeerManager {
public:
    static PeerManager& Instance();

    int CreateClientSurface(ClientSurfaceParam& param, uint64_t& surface_id);
    int SetClientSurfaceBounds(int32_t client_widget_id, int32_t off_x,
                               int32_t off_y, int32_t width, int32_t height);
    int DestroyClientSurface(int32_t client_widget_id);
    int AttachRemoteGdi(RemoteGdiParam& param, int fd);
    int RemoteGdiFlush(int32_t client_widget_id, int32_t x, int32_t y,
                       int32_t w, int32_t h);
    int DestroyRemoteGdi(int32_t client_widget_id);
    int UpdateRemoteGdiBounds(int32_t client_widget_id, int32_t off_x,
                              int32_t off_y);

    /* 本地 client_widget_id -> owner pid 路由表查询；false 表示该
     * surface 不是本进程经 IPC 创建的（应走本地直调） */
    bool GetSurfaceOwner(int32_t client_widget_id, int32_t& owner_pid);

private:
    PeerManager() = default;
    ~PeerManager() = default;
    PeerManager(const PeerManager&) = delete;
    PeerManager& operator=(const PeerManager&) = delete;

    IpcChannel* GetPeerChannel(int32_t pid);
    void DropPeer(int32_t pid);

    Mutex mutex_;
    std::unordered_map<int32_t, std::unique_ptr<IpcChannel>> peers_;
    /* client_widget_id -> owner pid（本进程发起的跨进程嵌入：client
     * surface 与 GDI 共享面共用，id 命名空间一致） */
    std::unordered_map<int32_t, int32_t> surface_routes_;
    ProcessManagerStub stub_;
};

class IpcHelper {
public:
    /* local implementations the stubs dispatch to; every wine process runs
     * the same code against its own page (windows it owns) */
    static int GetDisplay(DisplayParam& param);
    static int OpenUrl(const std::string& url);
    static int CreateClientSurface(ClientSurfaceParam& param,
                                   uint64_t& surface_id);
    static int SetClientSurfaceBounds(int32_t client_widget_id, int32_t off_x,
                                      int32_t off_y, int32_t width,
                                      int32_t height);
    static int DestroyClientSurface(int32_t client_widget_id);
    /* owner 侧 GDI 共享：mmap 像素并注册 overlay 回调（合成由 wine 侧
     * 注册的 remote_gdi_overlay_fn 完成） */
    static int AttachRemoteGdi(RemoteGdiParam& param, int fd);
    static int RemoteGdiFlush(int32_t client_widget_id, int32_t x, int32_t y,
                              int32_t w, int32_t h);
    static int DestroyRemoteGdi(int32_t client_widget_id);
    static int UpdateRemoteGdiBounds(int32_t client_widget_id, int32_t off_x,
                                     int32_t off_y);
};

}  // namespace ohos::adapter::new_ipc

#endif  // OHOS_ADAPTER_NEW_IPC_IPC_ADAPTER_H_

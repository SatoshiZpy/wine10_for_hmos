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
#include <utility>
#include <vector>

#include "ohos/adapter/common/mutex.h"
#include "ohos/adapter/new_ipc/domain_socket.h"
#include "ohos/adapter/new_ipc/ipc_channel.h"
#include "ohos/adapter/new_ipc/ipc_proxy_stub.h"

namespace ohos::adapter::new_ipc {

/* GDI shared-overlay callback: registered by the wine side of the owner
 * process; the stub calls back into wine on attach/flush/destroy to
 * composite (wine keeps its own locking and threading model).  The BLIT
 * dirty rect is passed through the param geometry fields. */
enum class RemoteGdiOp : int32_t { ADD = 0, BLIT = 1, REMOVE = 2, UPDATE = 3, VISIBLE = 4 };
using RemoteGdiOverlayCallback = void (*)(RemoteGdiOp op, int32_t id,
                                          const RemoteGdiParam* param,
                                          const void* pixels);
void SetRemoteGdiOverlayCallback(RemoteGdiOverlayCallback fn);

/* One IPC server per process: a persistent unix socket listener named
 * after the pid, waiting for other wine processes to connect when they
 * embed a window of this process.  Process/window embedding is arbitrary
 * (a may embed a window of c, regardless of who spawned whom), so every
 * process holds its own server and connections are made on demand; the
 * spawn chain still goes through ipc_lite. */
class IpcServer {
public:
    static IpcServer& Instance();

    /* Start the persistent listener on <socket_dir>/wine_ipc_<pid>.sock; idempotent */
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

/* One IPC client manager per process: connects to the peer process's
 * IpcServer by owner pid and caches channels; the client surface trio is
 * relayed over the point-to-point channel to the window owner process
 * (which creates the XComponent in its page and returns the surface id). */
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
    int SetRemoteGdiVisible(int32_t client_widget_id, int32_t visible);

    /* Local client_widget_id -> owner pid route lookup; false means the
     * surface was not created through IPC by this process (call locally) */
    bool GetSurfaceOwner(int32_t client_widget_id, int32_t& owner_pid);

private:
    PeerManager() = default;
    ~PeerManager() = default;
    PeerManager(const PeerManager&) = delete;
    PeerManager& operator=(const PeerManager&) = delete;

    IpcChannel* GetPeerChannel(int32_t pid);
    void DropPeer(int32_t pid);

    /* Connect to the owner by surface route and run a proxy member call;
     * on failure DropPeer so the next call reconnects */
    template <typename ProxyFn, typename... Args>
    int CallSurfaceOwner(int32_t client_widget_id, ProxyFn fn, Args&&... args)
    {
        int32_t owner_pid = 0;
        if (!GetSurfaceOwner(client_widget_id, owner_pid)) return -1;
        IpcChannel* channel = GetPeerChannel(owner_pid);
        if (!channel) return -1;
        ProcessManagerProxy proxy(*channel);
        int ret = (proxy.*fn)(std::forward<Args>(args)...);
        if (ret != 0) DropPeer(owner_pid);
        return ret;
    }

    Mutex mutex_;
    std::unordered_map<int32_t, std::unique_ptr<IpcChannel>> peers_;
    /* client_widget_id -> owner pid (cross-process embeddings initiated
     * by this process: client surfaces and GDI shared surfaces share one
     * id namespace) */
    std::unordered_map<int32_t, int32_t> surface_routes_;
    ProcessManagerStub stub_;
};

class IpcHelper {
public:
    /* Local stub implementations: every wine process runs the same code
     * against the window pages it owns */
    static int GetDisplay(DisplayParam& param);
    static int OpenUrl(const std::string& url);
    static int CreateClientSurface(ClientSurfaceParam& param,
                                   uint64_t& surface_id);
    static int SetClientSurfaceBounds(int32_t client_widget_id, int32_t off_x,
                                      int32_t off_y, int32_t width,
                                      int32_t height);
    static int DestroyClientSurface(int32_t client_widget_id);
    /* Owner-side GDI sharing: mmap the pixels and register the overlay
     * (compositing is done by the wine side via remote_gdi_overlay_fn) */
    static int AttachRemoteGdi(RemoteGdiParam& param, int fd);
    static int RemoteGdiFlush(int32_t client_widget_id, int32_t x, int32_t y,
                              int32_t w, int32_t h);
    static int DestroyRemoteGdi(int32_t client_widget_id);
    static int UpdateRemoteGdiBounds(int32_t client_widget_id, int32_t off_x,
                                     int32_t off_y);
    static int SetRemoteGdiVisible(int32_t client_widget_id, int32_t visible);
};

}  // namespace ohos::adapter::new_ipc

#endif  // OHOS_ADAPTER_NEW_IPC_IPC_ADAPTER_H_

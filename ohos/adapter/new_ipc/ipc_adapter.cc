// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/new_ipc/ipc_adapter.h"

#include <unistd.h>

#include <sys/mman.h>

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cinttypes>
#include <cstdlib>
#include <thread>
#include <utility>
#include <vector>

#include "ohos/adapter/common/logging.h"
#include "ohos/adapter/external_protocol/external_protocol_adapter.h"
#include "ohos/adapter/screen/screen_adapter.h"
#include "ohos_adapter.h"

namespace ohos::adapter::new_ipc {

/* Socket directory shared by every wine process (hap-level files, next
 * to WINEPREFIX): sandboxed children see a different app-level el2 view
 * than the main process, so the app directory must not be hardcoded.
 * WineLoaderAdapter passes it through WINE_IPC_DIR; every process derives
 * any peer's socket path from it plus the pid. */
static const char* ipc_socket_dir(void)
{
    static const char* dir = nullptr;
    if (!dir)
    {
        dir = getenv("WINE_IPC_DIR");
        if (!dir || !dir[0]) dir = "/data/storage/el2/base/haps/entry/files";
    }
    return dir;
}

std::string IpcServer::SocketPathForPid(int32_t pid)
{
    return std::string(ipc_socket_dir()) + "/wine_ipc_" +
           std::to_string(pid) + ".sock";
}

IpcServer& IpcServer::Instance()
{
    static IpcServer instance;
    return instance;
}

/* Channel request handler: read the request code from shm, dispatch to
 * the stub and reply.  The requesting proxy resets the cursor before
 * writing, so read it directly here; resetting would void the request
 * (read==write yields code=0). */
static IpcChannelCallback MakeStubHandler(IpcChannel* channel,
                                          ProcessManagerStub* stub)
{
    return [channel, stub](const IpcMessage& msg) {
        if (msg.type != IpcMessageType::SYNC_REQUEST || !msg.shm) return;
        IpcParcel data(*msg.shm);
        int32_t code = 0;
        data.ReadInt32(code);
        IpcParcel reply(*msg.shm);
        stub->OnRemoteRequest(static_cast<uint32_t>(code), data, reply,
                              msg.fds);
        channel->SendReply();
    };
}

IpcServer::IpcServer() = default;

IpcServer::~IpcServer()
{
    Stop();
}

bool IpcServer::Start()
{
    if (server_started_) return true;

    std::string path = SocketPathForPid(static_cast<int32_t>(getpid()));
    if (!server_socket_.Listen(path)) {
        LOGE("IpcServer::Start listen failed, path=%{public}s", path.c_str());
        return false;
    }

    server_started_ = true;
    accepting_ = true;
    accept_thread_ = std::thread(&IpcServer::AcceptLoop, this);
    LOGI("IpcServer::Start pid=%{public}d, path=%{public}s",
         static_cast<int>(getpid()), path.c_str());
    return true;
}

void IpcServer::Stop()
{
    if (!server_started_) return;
    accepting_ = false;
    /* Shutdown() before join(): wake the accept thread blocked in
     * accept() first -- 1) close() does not wake an accept() already
     * blocked in the kernel (Linux semantics), and 2) Accept() holds
     * send_mutex_ across the blocking call, so a direct Close() would
     * deadlock on that mutex.  Close() must run after the join. */
    server_socket_.Shutdown();
    if (accept_thread_.joinable()) accept_thread_.join();
    server_socket_.Close();
    {
        MutexLock lock(mutex_);
        for (auto& [id, channel] : channels_) {
            channel->StopListening();
            channel->Close();
        }
        channels_.clear();
    }
    unlink(SocketPathForPid(static_cast<int32_t>(getpid())).c_str());
    server_started_ = false;
    LOGI("IpcServer::Stop");
}

void IpcServer::AcceptLoop()
{
    LOGI("IpcServer::AcceptLoop enter");
    while (accepting_) {
        int client_fd = server_socket_.Accept();
        if (client_fd < 0) {
            if (!accepting_) break;
            LOGE("IpcServer::AcceptLoop accept failed, errno=%{public}d",
                 errno);
            continue;
        }

        /* Channel creation and shm exchange happen outside the lock:
         * accepts are serialized on this thread only, and the channel
         * table takes the lock briefly for insertion */
        auto channel = std::make_unique<IpcChannel>();
        if (!channel->ConnectWithFd(client_fd)) {
            LOGE("IpcServer::AcceptLoop ConnectWithFd failed");
            close(client_fd);
            continue;
        }
        if (!channel->ExchangeShmFd()) {
            LOGE("IpcServer::AcceptLoop ExchangeShmFd failed");
            channel->Close();
            continue;
        }

        ProcessManagerStub* stub = &stub_;
        channel->SetRequestHandler(MakeStubHandler(channel.get(), stub));
        channel->StartListening();

        {
            MutexLock lock(mutex_);
            channels_[next_channel_id_++] = std::move(channel);
        }
        LOGI("IpcServer::AcceptLoop peer connected, channel=%{public}d, "
             "fd=%{public}d", next_channel_id_ - 1, client_fd);
    }
    LOGI("IpcServer::AcceptLoop exit");
}

PeerManager& PeerManager::Instance()
{
    static PeerManager instance;
    return instance;
}

IpcChannel* PeerManager::GetPeerChannel(int32_t pid)
{
    MutexLock lock(mutex_);
    auto it = peers_.find(pid);
    if (it != peers_.end()) return it->second.get();

    auto channel = std::make_unique<IpcChannel>();
    if (!channel->ConnectToServer(IpcServer::SocketPathForPid(pid))) {
        LOGE("PeerManager::GetPeerChannel connect failed, pid=%{public}d",
             pid);
        return nullptr;
    }
    /* Client side: receive the owner's shm fd first, then start
     * listening (the order matters, otherwise the listen thread would
     * steal the SHM_FD message) */
    if (!channel->ExchangeShmFd()) {
        LOGE("PeerManager::GetPeerChannel ExchangeShmFd failed, "
             "pid=%{public}d", pid);
        channel->Close();
        return nullptr;
    }

    channel->SetRequestHandler(MakeStubHandler(channel.get(), &stub_));
    channel->StartListening();

    IpcChannel* raw = channel.get();
    peers_[pid] = std::move(channel);
    LOGI("PeerManager::GetPeerChannel connected, pid=%{public}d", pid);
    return raw;
}

void PeerManager::DropPeer(int32_t pid)
{
    MutexLock lock(mutex_);
    auto it = peers_.find(pid);
    if (it == peers_.end()) return;
    it->second->StopListening();
    it->second->Close();
    peers_.erase(it);
    LOGI("PeerManager::DropPeer pid=%{public}d", pid);
}

bool PeerManager::GetSurfaceOwner(int32_t client_widget_id,
                                  int32_t& owner_pid)
{
    MutexLock lock(mutex_);
    auto it = surface_routes_.find(client_widget_id);
    if (it == surface_routes_.end()) return false;
    owner_pid = it->second;
    return true;
}

int PeerManager::CreateClientSurface(ClientSurfaceParam& param,
                                     uint64_t& surface_id)
{
    int32_t owner_pid = param.owner_pid;
    if (owner_pid <= 0 || owner_pid == static_cast<int32_t>(getpid())) {
        LOGE("PeerManager::CreateClientSurface invalid owner_pid=%{public}d",
             owner_pid);
        return -1;
    }

    IpcChannel* channel = GetPeerChannel(owner_pid);
    if (!channel) return -1;

    ProcessManagerProxy proxy(*channel);
    int ret = proxy.CreateClientSurface(param, surface_id);
    if (ret != 0) {
        DropPeer(owner_pid);
        return ret;
    }

    {
        MutexLock lock(mutex_);
        surface_routes_[param.client_widget_id] = owner_pid;
    }
    LOGI("PeerManager::CreateClientSurface owner_pid=%{public}d, "
         "client=%{public}d, surface_id=%{public}" PRIu64,
         owner_pid, param.client_widget_id, surface_id);
    return 0;
}

int PeerManager::SetClientSurfaceBounds(int32_t client_widget_id,
                                        int32_t off_x, int32_t off_y,
                                        int32_t width, int32_t height)
{
    return CallSurfaceOwner(
        client_widget_id, &ProcessManagerProxy::SetClientSurfaceBounds,
        client_widget_id, off_x, off_y, width, height);
}

int PeerManager::DestroyClientSurface(int32_t client_widget_id)
{
    int32_t owner_pid = 0;
    if (!GetSurfaceOwner(client_widget_id, owner_pid)) return -1;

    {
        MutexLock lock(mutex_);
        surface_routes_.erase(client_widget_id);
    }

    IpcChannel* channel = GetPeerChannel(owner_pid);
    if (!channel) return 0;

    ProcessManagerProxy proxy(*channel);
    int ret = proxy.DestroyClientSurface(client_widget_id);
    if (ret != 0) DropPeer(owner_pid);
    return ret;
}

int PeerManager::AttachRemoteGdi(RemoteGdiParam& param, int fd)
{
    int32_t owner_pid = param.owner_pid;
    if (owner_pid <= 0 || owner_pid == static_cast<int32_t>(getpid())) {
        LOGE("PeerManager::AttachRemoteGdi invalid owner_pid=%{public}d",
             owner_pid);
        return -1;
    }

    IpcChannel* channel = GetPeerChannel(owner_pid);
    if (!channel) return -1;

    ProcessManagerProxy proxy(*channel);
    int ret = proxy.AttachRemoteGdi(param, fd);
    if (ret != 0) {
        DropPeer(owner_pid);
        return ret;
    }

    {
        MutexLock lock(mutex_);
        surface_routes_[param.client_widget_id] = owner_pid;
    }
    LOGI("PeerManager::AttachRemoteGdi owner_pid=%{public}d, id=%{public}d, "
         "%{public}dx%{public}d", owner_pid, param.client_widget_id,
         param.width, param.height);
    return 0;
}

int PeerManager::RemoteGdiFlush(int32_t client_widget_id, int32_t x,
                                int32_t y, int32_t w, int32_t h)
{
    return CallSurfaceOwner(client_widget_id,
                            &ProcessManagerProxy::RemoteGdiFlush,
                            client_widget_id, x, y, w, h);
}

int PeerManager::DestroyRemoteGdi(int32_t client_widget_id)
{
    int32_t owner_pid = 0;
    if (!GetSurfaceOwner(client_widget_id, owner_pid)) return -1;

    {
        MutexLock lock(mutex_);
        surface_routes_.erase(client_widget_id);
    }

    IpcChannel* channel = GetPeerChannel(owner_pid);
    if (!channel) return 0;

    ProcessManagerProxy proxy(*channel);
    int ret = proxy.DestroyRemoteGdi(client_widget_id);
    if (ret != 0) DropPeer(owner_pid);
    return ret;
}

int PeerManager::UpdateRemoteGdiBounds(int32_t client_widget_id,
                                       int32_t off_x, int32_t off_y)
{
    return CallSurfaceOwner(client_widget_id,
                            &ProcessManagerProxy::UpdateRemoteGdiBounds,
                            client_widget_id, off_x, off_y);
}

int PeerManager::SetRemoteGdiVisible(int32_t client_widget_id, int32_t visible)
{
    return CallSurfaceOwner(client_widget_id,
                            &ProcessManagerProxy::SetRemoteGdiVisible,
                            client_widget_id, visible);
}

int IpcHelper::GetDisplay(DisplayParam& param)
{
    Display default_display;
    if (!ScreenAdapter::GetInstance().GetDefaultDisplay(default_display)) {
        LOGE("GetDefaultDisplay failed");
        return -1;
    }
    param.id = default_display.id;
    param.name = default_display.name;
    param.left = default_display.avail_area.left;
    param.top = default_display.avail_area.top;
    param.width = default_display.avail_area.width;
    param.height = default_display.avail_area.height;
    param.density_dpi = default_display.density_dpi;
    param.x_dpi = default_display.x_dpi;
    param.y_dpi = default_display.y_dpi;
    return 0;
}

int IpcHelper::OpenUrl(const std::string& url)
{
    ExternalProtocolAdapter::GetInstance().OpenExternal(url);
    return 0;
}

int IpcHelper::CreateClientSurface(ClientSurfaceParam& param,
                                   uint64_t& surface_id)
{
    struct client_surface_param p = {};
    p.parent_widget_id = param.parent_widget_id;
    p.off_x = param.off_x;
    p.off_y = param.off_y;
    p.width = param.width;
    p.height = param.height;
    p.src_hwnd = param.src_hwnd;
    p.src_pid = param.src_pid;

    int client_widget_id = ::CreateClientSurface(&p);
    if (client_widget_id <= 0) return -1;

    ohos_native_window window = ::GetNativeWindow(client_widget_id);
    if (!window || OH_NativeWindow_GetSurfaceId(window, &surface_id) != 0) {
        LOGE("IpcHelper::CreateClientSurface: get surface id failed, "
             "widget=%{public}d", client_widget_id);
        ::DestroyClientSurface(client_widget_id);
        return -1;
    }

    param.client_widget_id = client_widget_id;
    LOGI("IpcHelper::CreateClientSurface: client=%{public}d, "
         "parent=%{public}d, surface_id=%{public}" PRIu64,
         client_widget_id, param.parent_widget_id, surface_id);
    return 0;
}

int IpcHelper::SetClientSurfaceBounds(int32_t client_widget_id, int32_t off_x,
                                      int32_t off_y, int32_t width,
                                      int32_t height)
{
    ::SetClientSurfaceBounds(client_widget_id, off_x, off_y, width, height);
    return 0;
}

int IpcHelper::DestroyClientSurface(int32_t client_widget_id)
{
    ::DestroyClientSurface(client_widget_id);
    return 0;
}

/* ---- owner-side GDI sharing ---- */

static RemoteGdiOverlayCallback g_overlay_callback = nullptr;

void SetRemoteGdiOverlayCallback(RemoteGdiOverlayCallback fn)
{
    g_overlay_callback = fn;
}

/* Overlay registry: id -> {param, mmap}.  The adapter owns the mapping
 * lifetime; compositing blits go back into wine through the callback
 * (wine's own threading and locking model) */
struct RemoteGdiOverlay {
    RemoteGdiParam param;
    void* pixels = nullptr;
    size_t map_size = 0;
    bool visible = true;
};
/* Transitional overlay (zombie) while a cross-process window rebuilds
 * its shared surface: a destroy request does not REMOVE/unmap
 * immediately, it is kept for at most ZOMBIE_GRACE_MS waiting for the
 * replacing attach:
 * 1) avoids the white flash of the host compositing without the overlay
 *    during the destroy->rebuild gap (~30ms);
 * 2) when the new attach arrives, the last frame is stretched from the
 *    zombie mapping to prefill the new buffer, closing the gap until
 *    the child's first frame.  A real window close shows the last frame
 *    lingering briefly (close-animation-like), reclaimed by the sweeper. */
struct RemoteGdiZombie {
    RemoteGdiParam param;
    void* pixels = nullptr;
    size_t map_size = 0;
    std::chrono::steady_clock::time_point born;
    std::chrono::steady_clock::time_point deadline;
};
static constexpr auto ZOMBIE_GRACE_MS = std::chrono::milliseconds(100);
static Mutex g_overlay_mutex;
static std::unordered_map<int32_t, RemoteGdiOverlay> g_overlays;
static std::unordered_map<int32_t, RemoteGdiZombie> g_overlay_zombies;

/* Nearest-neighbour stretch prefill (32bpp row-aligned buffers;
 * degenerates to a plain copy when sizes match) */
static void PrefillRemoteGdi(const RemoteGdiParam& src_param,
                             const void* src_bits,
                             const RemoteGdiParam& dst_param, void* dst_bits)
{
    int src_w = src_param.width, src_h = src_param.height;
    int dst_w = dst_param.width, dst_h = dst_param.height;
    if (src_w <= 0 || src_h <= 0 || dst_w <= 0 || dst_h <= 0) return;
    if (src_param.stride % 4 || dst_param.stride % 4) return;
    const uint32_t* src = static_cast<const uint32_t*>(src_bits);
    uint32_t* dst = static_cast<uint32_t*>(dst_bits);
    for (int y = 0; y < dst_h; y++) {
        const uint32_t* srow =
            src + (static_cast<size_t>(y) * src_h / dst_h) *
                      (src_param.stride / 4);
        uint32_t* drow =
            dst + static_cast<size_t>(y) * (dst_param.stride / 4);
        for (int x = 0; x < dst_w; x++) {
            drow[x] = srow[static_cast<size_t>(x) * src_w / dst_w];
        }
    }
}

/* Expired zombie reclaim: REMOVE first (invalidate the wine-side entry),
 * then unmap; the order must not be reversed */
static void SweepOverlayZombies()
{
    std::this_thread::sleep_for(ZOMBIE_GRACE_MS);
    std::vector<std::pair<int32_t, std::pair<void*, size_t>>> expired;
    {
        MutexLock lock(g_overlay_mutex);
        auto now = std::chrono::steady_clock::now();
        for (auto it = g_overlay_zombies.begin();
             it != g_overlay_zombies.end();) {
            if (now >= it->second.deadline) {
                expired.emplace_back(it->first, std::make_pair(
                                                    it->second.pixels,
                                                    it->second.map_size));
                it = g_overlay_zombies.erase(it);
            } else {
                ++it;
            }
        }
    }
    for (auto& e : expired) {
        if (g_overlay_callback) {
            g_overlay_callback(RemoteGdiOp::REMOVE, e.first, nullptr,
                               nullptr);
        }
        if (e.second.first) munmap(e.second.first, e.second.second);
        LOGI("SweepOverlayZombies expired id=%{public}d", e.first);
    }
}

int IpcHelper::AttachRemoteGdi(RemoteGdiParam& param, int fd)
{
    if (fd < 0 || param.width <= 0 || param.height <= 0 || param.stride <= 0) {
        LOGE("IpcHelper::AttachRemoteGdi invalid param or fd");
        return -1;
    }
    size_t size = static_cast<size_t>(param.stride) * param.height;
    /* PROT_WRITE is only for prefilling the previous frame; the child
     * starts drawing after its attach returns, no race */
    void* pixels = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (pixels == MAP_FAILED) {
        LOGE("IpcHelper::AttachRemoteGdi mmap failed, errno=%{public}d",
             errno);
        return -1;
    }
    close(fd);

    /* The shared surface id is assigned by the owner (unique enough:
     * callers carry their own pid, and the overlay table is keyed in
     * this process, so a simple increment works) */
    static std::atomic<int32_t> next_overlay_id{1};
    if (param.client_widget_id <= 0) {
        param.client_widget_id = next_overlay_id++;
    }

    /* Prefill from the newest zombie under the same parent, then reclaim
     * it (with several children on one parent take the newest: a
     * mismatch costs at most one frame, overwritten by the child's first
     * frame) */
    RemoteGdiZombie zombie{};
    bool replace = false;
    {
        MutexLock lock(g_overlay_mutex);
        g_overlays[param.client_widget_id] = {param, pixels, size};
        auto zit = g_overlay_zombies.begin();
        auto best = g_overlay_zombies.end();
        while (zit != g_overlay_zombies.end()) {
            if (zit->second.param.parent_widget_id ==
                    param.parent_widget_id &&
                zit->first != param.client_widget_id &&
                (best == g_overlay_zombies.end() ||
                 zit->second.born > best->second.born)) {
                best = zit;
            }
            ++zit;
        }
        if (best != g_overlay_zombies.end()) {
            PrefillRemoteGdi(best->second.param, best->second.pixels, param,
                             pixels);
            zombie = best->second;
            g_overlay_zombies.erase(best);
            replace = true;
        }
    }
    if (g_overlay_callback) {
        g_overlay_callback(RemoteGdiOp::ADD, param.client_widget_id, &param,
                           pixels);
    }
    if (replace) {
        /* REMOVE after ADD: seamless replacement of the wine-side entry */
        if (g_overlay_callback) {
            g_overlay_callback(RemoteGdiOp::REMOVE, zombie.param.client_widget_id,
                               nullptr, nullptr);
        }
        if (zombie.pixels) munmap(zombie.pixels, zombie.map_size);
        LOGI("IpcHelper::AttachRemoteGdi replaced zombie id=%{public}d",
             zombie.param.client_widget_id);
    }
    LOGI("IpcHelper::AttachRemoteGdi id=%{public}d, parent=%{public}d, "
         "%{public}dx%{public}d", param.client_widget_id,
         param.parent_widget_id, param.width, param.height);
    return 0;
}

int IpcHelper::RemoteGdiFlush(int32_t client_widget_id, int32_t x, int32_t y,
                              int32_t w, int32_t h)
{
    RemoteGdiParam param;
    void* pixels = nullptr;
    {
        MutexLock lock(g_overlay_mutex);
        auto it = g_overlays.find(client_widget_id);
        if (it == g_overlays.end()) return -1;
        param = it->second.param;
        pixels = it->second.pixels;
    }

    if (g_overlay_callback) {
        /* The dirty rect rides in the param geometry fields */
        param.off_x = x;
        param.off_y = y;
        param.width = w;
        param.height = h;
        g_overlay_callback(RemoteGdiOp::BLIT, client_widget_id, &param,
                           pixels);
    }
    return 0;
}

int IpcHelper::DestroyRemoteGdi(int32_t client_widget_id)
{
    {
        MutexLock lock(g_overlay_mutex);
        auto it = g_overlays.find(client_widget_id);
        if (it == g_overlays.end()) return 0;
        auto now = std::chrono::steady_clock::now();
        g_overlay_zombies[client_widget_id] = {it->second.param,
                                               it->second.pixels,
                                               it->second.map_size, now,
                                               now + ZOMBIE_GRACE_MS};
        g_overlays.erase(it);
    }
    /* No immediate REMOVE/unmap: wait for a replacing attach to prefill
     * and reclaim, or for the sweeper to expire */
    std::thread(SweepOverlayZombies).detach();
    LOGI("IpcHelper::DestroyRemoteGdi id=%{public}d (zombie)", client_widget_id);
    return 0;
}

int IpcHelper::UpdateRemoteGdiBounds(int32_t client_widget_id,
                                     int32_t off_x, int32_t off_y)
{
    RemoteGdiParam param;
    {
        MutexLock lock(g_overlay_mutex);
        auto it = g_overlays.find(client_widget_id);
        if (it == g_overlays.end()) return -1;
        it->second.param.off_x = off_x;
        it->second.param.off_y = off_y;
        param = it->second.param;
    }
    /* Sync the wineohos-side overlay table (op=UPDATE, offset only) */
    if (g_overlay_callback) {
        g_overlay_callback(RemoteGdiOp::UPDATE, client_widget_id, &param,
                           nullptr);
    }
    LOGI("IpcHelper::UpdateRemoteGdiBounds id=%{public}d off=(%{public}d,"
         "%{public}d)", client_widget_id, off_x, off_y);
    return 0;
}

int IpcHelper::SetRemoteGdiVisible(int32_t client_widget_id, int32_t visible)
{
    RemoteGdiParam param;
    {
        MutexLock lock(g_overlay_mutex);
        auto it = g_overlays.find(client_widget_id);
        if (it == g_overlays.end()) return -1;
        it->second.visible = visible != 0;
        /* Zombies have no visibility: the destroy/rebuild paths do not
         * go through this entry */
        param = it->second.param;
    }
    /* visible rides in param.off_x (callback signature reuse, op=4 only) */
    param.off_x = visible;
    if (g_overlay_callback) {
        g_overlay_callback(RemoteGdiOp::VISIBLE, client_widget_id, &param,
                           nullptr);
    }
    LOGI("IpcHelper::SetRemoteGdiVisible id=%{public}d visible=%{public}d",
         client_widget_id, visible);
    return 0;
}

}  // namespace ohos::adapter::new_ipc

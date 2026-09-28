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

/* 每个 wine 进程共享的 socket 目录（hap 级 files，WINEPREFIX 同级）：
 * 隔离子进程的 app 级 el2 视图与主进程不同，不能硬编码 app 目录，
 * 由 WineLoaderAdapter 经 WINE_IPC_DIR 传入，所有进程据此 + pid 推导
 * 任意对端的 socket path */
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
    server_socket_.Close();
    if (accept_thread_.joinable()) accept_thread_.join();
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

        /* 通道创建与 shm 交换在锁外：accept 只会由本线程串行触发，
         * 通道表仅 insert 时短暂持锁 */
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
        IpcChannel* ch_ptr = channel.get();
        channel->SetRequestHandler([ch_ptr, stub](const IpcMessage& msg) {
            if (msg.type != IpcMessageType::SYNC_REQUEST || !msg.shm) return;
            /* 请求方 proxy 写请求前已归位游标，此处直接读；重置会作废
             * 请求（read==write 时读到 code=0） */
            IpcParcel data(*msg.shm);
            int32_t code = 0;
            data.ReadInt32(code);
            IpcParcel reply(*msg.shm);
            stub->OnRemoteRequest(static_cast<uint32_t>(code), data, reply,
                                  msg.fds);
            ch_ptr->SendReply();
        });
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
    /* client 侧：先收 owner 发来的 shm fd，再开监听（顺序不可反，
     * 否则 listen 线程会抢走 SHM_FD 消息） */
    if (!channel->ExchangeShmFd()) {
        LOGE("PeerManager::GetPeerChannel ExchangeShmFd failed, "
             "pid=%{public}d", pid);
        channel->Close();
        return nullptr;
    }

    ProcessManagerStub* stub = &stub_;
    IpcChannel* ch_ptr = channel.get();
    channel->SetRequestHandler([ch_ptr, stub](const IpcMessage& msg) {
        if (msg.type != IpcMessageType::SYNC_REQUEST || !msg.shm) return;
        IpcParcel data(*msg.shm);
        int32_t code = 0;
        data.ReadInt32(code);
        IpcParcel reply(*msg.shm);
        stub->OnRemoteRequest(static_cast<uint32_t>(code), data, reply,
                              msg.fds);
        ch_ptr->SendReply();
    });
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
    int32_t owner_pid = 0;
    if (!GetSurfaceOwner(client_widget_id, owner_pid)) return -1;

    IpcChannel* channel = GetPeerChannel(owner_pid);
    if (!channel) return -1;

    ProcessManagerProxy proxy(*channel);
    int ret = proxy.SetClientSurfaceBounds(client_widget_id, off_x, off_y,
                                           width, height);
    if (ret != 0) DropPeer(owner_pid);
    return ret;
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
    int32_t owner_pid = 0;
    if (!GetSurfaceOwner(client_widget_id, owner_pid)) return -1;

    IpcChannel* channel = GetPeerChannel(owner_pid);
    if (!channel) return -1;

    ProcessManagerProxy proxy(*channel);
    int ret = proxy.RemoteGdiFlush(client_widget_id, x, y, w, h);
    if (ret != 0) DropPeer(owner_pid);
    return ret;
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
    int32_t owner_pid = 0;
    if (!GetSurfaceOwner(client_widget_id, owner_pid)) return -1;

    IpcChannel* channel = GetPeerChannel(owner_pid);
    if (!channel) return -1;

    ProcessManagerProxy proxy(*channel);
    int ret = proxy.UpdateRemoteGdiBounds(client_widget_id, off_x, off_y);
    if (ret != 0) DropPeer(owner_pid);
    return ret;
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

/* ── owner 侧 GDI 共享 ─────────────────────────────────────────────── */

static RemoteGdiOverlayCallback g_overlay_callback = nullptr;

void SetRemoteGdiOverlayCallback(RemoteGdiOverlayCallback fn)
{
    g_overlay_callback = fn;
}

/* overlay 注册表：id -> {param, mmap}。adapter 持有映射生命周期，
 * 合成 blit 经回调进 wine（wine 侧自己的线程与锁模型） */
struct RemoteGdiOverlay {
    RemoteGdiParam param;
    void* pixels = nullptr;
    size_t map_size = 0;
};
/* 跨进程窗口重建共享面时的过渡 overlay（僵尸态）：销毁请求不立即
 * REMOVE/unmap，保留至多 ZOMBIE_GRACE_MS 等待替换 attach——
 * 1) 规避销毁→重建间隙（~30ms）内宿主合成丢 overlay 的白闪；
 * 2) 新 attach 到达时直接从僵尸映射把末帧拉伸预填进新 buffer，
 *    消除重建到子进程首帧之间的空白。窗口真实关闭时表现为末帧
 *    短暂残留（类关闭动画），到期由清扫线程回收 */
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

/* 最近邻拉伸预填（32bpp 行对齐缓冲；尺寸一致时退化为原样拷贝） */
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

/* 僵尸到期回收：先 REMOVE（wine 侧表项失效）再 unmap，顺序不可颠倒 */
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
    /* PROT_WRITE 仅为预填旧帧；子进程 attach 返回后才开始绘制，无竞争 */
    void* pixels = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (pixels == MAP_FAILED) {
        LOGE("IpcHelper::AttachRemoteGdi mmap failed, errno=%{public}d",
             errno);
        return -1;
    }
    close(fd);

    /* 共享面 id 由 owner 分配（跨进程唯一：发起方各自带 pid，但 overlay
     * 表在本进程键控，自增即可） */
    static std::atomic<int32_t> next_overlay_id{1};
    if (param.client_widget_id <= 0) {
        param.client_widget_id = next_overlay_id++;
    }

    /* 同 parent 下最新僵尸的末帧预填 + 替换回收（多子窗口同 parent 时
     * 取最新，误配最多一帧，子进程首帧即覆盖） */
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
        /* ADD 之后 REMOVE：wine 侧表项无缝替换 */
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
        /* dirty rect 经 param 的几何字段传递 */
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
    /* 不立即 REMOVE/unmap：等待替换 attach 预填回收，或到期清扫 */
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
    /* 同步 wineohos 侧 overlay 表（op=UPDATE，仅 off 生效） */
    if (g_overlay_callback) {
        g_overlay_callback(RemoteGdiOp::UPDATE, client_widget_id, &param,
                           nullptr);
    }
    LOGI("IpcHelper::UpdateRemoteGdiBounds id=%{public}d off=(%{public}d,"
         "%{public}d)", client_widget_id, off_x, off_y);
    return 0;
}

}  // namespace ohos::adapter::new_ipc

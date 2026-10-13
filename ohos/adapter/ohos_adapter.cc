// Copyright (c) 2025 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos_adapter.h"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/ashmem.h>

#include <stdint.h>
#include <cinttypes>
#include <cstdlib>
#include <cstring>
#include <new>
#include <unistd.h>
#include <napi/native_api.h>

#ifndef ASHMEM_SET_SIZE
#define ASHMEM_SET_SIZE     _IOW(0x77, 3, size_t)
#endif

#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <atomic>

#include <native_vsync/native_vsync.h>

#include "ohos/adapter/aki_hook/aki_hook.h"
#include "ohos/adapter/common/constants.h"
#include "ohos/adapter/common/logging.h"
#include "ohos/adapter/common/mutex.h"
#include "ohos/adapter/cursor/cursor.h"
#include "ohos/adapter/event/event_manager.h"
#include "ohos/adapter/watchdog/exit_watchdog.h"
#include "ohos/adapter/external_protocol/external_protocol_adapter.h"
#include "ohos/adapter/file_manager/file_manager_adapter.h"
#include "ohos/adapter/ime_adapter/input_method_adapter.h"
#include "ohos/adapter/ime_adapter/wine_ime_delegate.h"
#include "ohos/adapter/life_cycle/lifecycle_init.h"
#include "ohos/adapter/multiprocess/app_spawn_communication.h"
#include "ohos/adapter/multiprocess/command_line/command_line_helper.h"
#include "ohos/adapter/ipc_lite/ipc_lite_adapter.h"
#include "ohos/adapter/new_ipc/ipc_adapter.h"
#include "ohos/adapter/print/ohos_print_adapter.h"
#include "ohos/adapter/screen/screen_adapter.h"
#include "ohos/adapter/screenlock_monitor/screenlock_monitor_adapter.h"
#include "ohos/adapter/screenshot/screenshot_adapter.h"
#include "ohos/adapter/window/window_common.h"
#include "ohos/adapter/window/wine_window_manager.h"
#include "ohos/adapter/xcomponent/adapter/window_adapter.h"
#include "ohos/adapter/xcomponent/xcomponent_manager.h"

EXTERN_C_START

using namespace ohos::adapter;
using namespace ohos::adapter::life_cycle;
using namespace ohos::adapter::xcomponent;
using namespace ohos::adapter::window;
using namespace ohos::adapter::event;
using namespace ohos::adapter::multiprocess;

// Serializes native window creation; keep separate from g_process_spawn_mutex
// so a stalled spawn can never block window creation (and vice versa).
static Mutex g_window_mutex;
// Child-process spawning must not share the window mutex: StartNativeProcess
// blocks until the spawned process connects back, and holding the window mutex
// during that wait deadlocks CreateNativeWindow (seen as a frozen UI whenever
// a spawn stalls, e.g. cold start after first install).
static Mutex g_process_spawn_mutex;

static bool has_loaded_ = false;

static window_features_mask get_features_mask(const struct window_features* features)
{
    window_features_mask mask = 0;

    if (features && features->title_bar) {
        mask |= features_mask_for_type(WindowFeatures::WINDOW_TITLE_BAR);
    }

    if (features && features->close_button) {
        mask |= features_mask_for_type(WindowFeatures::WINDOW_CLOSE_BUTTON);
    }

    if (features && features->minimize_button) {
        mask |= features_mask_for_type(WindowFeatures::WINDOW_MINIMIZE_BUTTON);
    }

    if (features && features->maximize_button) {
        mask |= features_mask_for_type(WindowFeatures::WINDOW_MAXIMIZE_BUTTON);
    }

    if (features && features->resizable) {
        mask |= features_mask_for_type(WindowFeatures::WINDOW_RESIZABLE);
    }

    if (features && features->touchable) {
        mask |= features_mask_for_type(WindowFeatures::WINDOW_TOUCHABLE);
    }

    if (features && features->shadow) {
        mask |= features_mask_for_type(WindowFeatures::WINDOW_HAS_SHADOW);
    }

    if (features && features->prevents_app_activation) {
        mask |= features_mask_for_type(WindowFeatures::WINDOW_NO_ACTIVATION);
    }

    if (features && features->use_floating_window) {
        mask |= features_mask_for_type(WindowFeatures::USE_FLOATING_WINDOW);
    }

    if (features && features->use_dark_mode) {
        mask |= features_mask_for_type(WindowFeatures::USE_DARK_MODE);
    }

    if (features && features->is_modal) {
        mask |= features_mask_for_type(WindowFeatures::WINDOW_MODAL);
    }

    LOGD("get_features_mask: %{public}#lx", mask);
    return mask;
}

static window_state_mask get_window_state_mask(const struct window_state* state)
{
    window_state_mask mask = 0;

    if (state && state->disabled) {
        mask |= state_mask_for_type(WindowState::WINDOW_DISABLED);
    }

    if (state && state->no_foreground) {
        mask |= state_mask_for_type(WindowState::WINDOW_NO_FOREGROUND);
    }

    if (state && state->top_most) {
        mask |= state_mask_for_type(WindowState::WINDOW_TOP_MOST);
    }

    if (state && state->excluded_by_control) {
        mask |= state_mask_for_type(WindowState::WINDOW_EXCLUDED_BY_CONTROL);
    }

    if (state && state->minimized) {
        mask |= state_mask_for_type(WindowState::WINDOW_MINIMIZED);
    }

    if (state && state->maximized) {
        mask |= state_mask_for_type(WindowState::WINDOW_MAXIMIZED);
    }

    if (state && state->full_screen) {
        mask |= state_mask_for_type(WindowState::WINDOW_FULL_SCREEN);
    }

    if (state && state->split_mode) {
        mask |= state_mask_for_type(WindowState::WINDOW_SPLIT_MODE);
    }

    if (state && state->visible) {
        mask |= state_mask_for_type(WindowState::WINDOW_VISIBLE);
    }

    LOGD("get_window_state_mask: %{public}#lx", mask);
    return mask;
}

static NewWindowParam Builder(const struct window_param* param)
{
    NewWindowParam win_param;
    win_param.process_key = CommandLineHelper::GetProcessName();
    win_param.ability_type = (AbilityType)param->container;
    win_param.bounds = {param->bounds.left, param->bounds.top,
                        param->bounds.width, param->bounds.height};
    win_param.parent_id = param->parent_id;
    win_param.hwnd = param->hwnd;
    win_param.owner_pid = param->pid;
    win_param.feature_mask = get_features_mask(&param->features);
    win_param.window_state_mask = get_window_state_mask(&param->state);

    // The first window created must be the main window of the main process.
    if (WindowAdapter::GetInstance().GetWindowWidgetId() == USER_DEFAULT_WIDGET_ID &&
            IsMainProcess() &&
            !has_loaded_) {
        if (param->type != WindowInitType::kWindow ) {
            LOGW("first window must be kWindow, overriding type=%{public}d", param->type);
            win_param.window_type = WindowInitType::kWindow;
        }
        win_param.widget_id = USER_DEFAULT_WIDGET_ID;
        has_loaded_ = true;
    } else {
        win_param.window_type = param->type;
        win_param.widget_id = WindowAdapter::GetInstance().NextWindowWidgetId();
    }

    return win_param;
}

static void UpdateWindow(int widget_id, const NewWindowParam& param)
{
    auto window = WineWindowManager::GetInstance().GetWindow(widget_id);
    if (!window) {
        LOGE("UpdateWindow: window not exists, widgetId %{public}d", widget_id);
        return;
    }
    LOGI("UpdateWindow widgetId %{public}d", widget_id);
}

int CreateNativeWindow(const struct window_param* param)
{
    MutexLock lock_protect(g_window_mutex);
    if (!param) {
        LOGE("invalid param");
        return -1;
    }

    NewWindowParam win_param = Builder(param);
    int widget_id = -1;
    auto window = WineWindow::Create(win_param, &widget_id);
    if (!window) {
        LOGE("CreateWindow error");
        return -1;
    }
    LOGI("CreateNativeWindow widgetId %{public}d", widget_id);
    return widget_id;
}

void CloseNativeWindow(int widget_id)
{
    auto window = WineWindowManager::GetInstance().GetWindow(widget_id);
    if (!window) {
        LOGE("CloseNativeWindow: window not exists, widgetId %{public}d", widget_id);
        return;
    }
    LOGI("CloseNativeWindow widgetId %{public}d", widget_id);
    window->Close();
    WineWindowManager::GetInstance().RemoveWindow(widget_id);
}

int StartNativeProcess(int argc, char** argv, const struct launch_options* options)
{
    MutexLock lock_protect(g_process_spawn_mutex);
    if (!options) {
        LOGE("invalid launch_options");
        return -1;
    }
    LOGI("launch process type %{public}d", options->type);
    std::vector<std::string> command;
    for (int i = 0; i < argc && argv[i] != nullptr; ++i) {
        command.emplace_back(argv[i]);
    }
    std::vector<std::pair<int, int>> fds = {};
    for (size_t i = 0; i < MAX_SEND_FDS; i++) {
        int fd = options->fds[i];
        if (fd <= 0) {
            continue;
        }
        int id = options->id[i];
        fds.push_back(std::make_pair(fd, id));
    }
    int pid = ipc_lite::IpcLiteHelper::CreateProcess(command, fds);
    LOGI("launch process type %{public}d, %{public}d", options->type, pid);
    return pid;
}

void SetCustomCursor(int widget_id, int width, int height, int hotspot_x, int hotspot_y, const unsigned int* bits)
{
    if (!bits) {
        LOGE("invalid bits");
        return;
    }

    size_t buff_size = width * height * sizeof(unsigned int);
    std::shared_ptr<char[]> buff = std::make_shared<char[]>(buff_size);
    std::memcpy(buff.get(), bits, buff_size);
    CustomCusorInfo cursor_info{widget_id, width, height, hotspot_x, hotspot_y, buff};
    SetCustomCursor(cursor_info);
}

void SetCursorStyle(int widget_id, int cursor_type)
{
    // cursor_type < 0 means "fall back to the default arrow"; the JS side
    // treats this the same as kDefault. Clamp to the enum range so the
    // arkts side never sees an out-of-range value.
    OhosCursorType type = OhosCursorType::kDefault;
    if (cursor_type >= static_cast<int>(OhosCursorType::kMinValue) &&
        cursor_type <= static_cast<int>(OhosCursorType::kMaxValue)) {
        type = static_cast<OhosCursorType>(cursor_type);
    }
    SetCursor(widget_id, type);
}

void SetWindowBounds(int widget_id, const struct window_bounds* bounds)
{
    if (!bounds) {
        LOGE("invalid bounds");
        return;
    }

    auto window = WineWindowManager::GetInstance().GetWindow(widget_id);
    if (!window) {
        LOGE("SetWindowBounds: window not exists, widgetId %{public}d", widget_id);
        return;
    }
    LOGI("SetWindowBounds widgetId %{public}d, rect: %{public}d, %{public}d, %{public}d, %{public}d",
        widget_id, bounds->left, bounds->top, bounds->width, bounds->height);
    ::WindowRect rect = {bounds->left, bounds->top, bounds->width, bounds->height};
    window->SetBounds(rect);
}

void MaximizeWindow(int widget_id)
{
    auto window = WineWindowManager::GetInstance().GetWindow(widget_id);
    if (!window) {
        LOGE("MaximizeWindow: window not exists, widgetId %{public}d", widget_id);
        return;
    }
    LOGI("MaximizeWindow widgetId %{public}d", widget_id);
    window->Maximize();
}

void MinimizeWindow(int widget_id)
{
    auto window = WineWindowManager::GetInstance().GetWindow(widget_id);
    if (!window) {
        LOGE("MinimizeWindow: window not exists, widgetId %{public}d", widget_id);
        return;
    }
    LOGI("MinimizeWindow widgetId %{public}d", widget_id);
    window->Minimize();
}

void RestoreWindow(int widget_id)
{
    auto window = WineWindowManager::GetInstance().GetWindow(widget_id);
    if (!window) {
        LOGE("RestoreWindow: window not exists, widgetId %{public}d", widget_id);
        return;
    }
    LOGI("RestoreWindow widgetId %{public}d", widget_id);
    window->Restore();
}

void SetParentWindow(int widget_id, int parent_id)
{
    // todo: impl function
}

void SetWindowFeatures(int widget_id, const struct window_features* wf)
{
    // todo: impl function
}

void SetWindowState(int widget_id, const struct window_state* state)
{
    // todo: impl function
}

void SetWindowAlpha(int widget_id, float opacity)
{
    // todo: impl function
}

void SetWindowTitle(int widget_id, const char* title)
{
    auto window = WineWindowManager::GetInstance().GetWindow(widget_id);
    if (!window) {
        LOGE("SetWindowTitle: window not exists, widgetId %{public}d", widget_id);
        return;
    }
    LOGI("SetWindowTitle widgetId %{public}d", widget_id);
    window->SetTitle(std::string(title));
}

void SetWindowVisible(int widget_id, int visible)
{
    auto window = WineWindowManager::GetInstance().GetWindow(widget_id);
    if (!window) {
        LOGE("SetWindowVisible: window not exists, widgetId %{public}d", widget_id);
        return;
    }
    LOGI("SetWindowVisible widgetId %{public}d, visible %{public}d", widget_id, visible);
    visible ? window->Show() : window->Hide();
}

struct window_bounds GetInitialBounds()
{
    window_bounds bounds = {0};

    if (WindowAdapter::GetInstance().GetWindowWidgetId() > 1) {
        return bounds;
    }

    WindowRect window_rect = WindowAdapter::GetInstance().GetInitialBounds();
    bounds.left = window_rect.left;
    bounds.top = window_rect.top;
    bounds.width = window_rect.width;
    bounds.height = window_rect.height;
    return bounds;
}

ohos_native_window GetNativeWindow(int widget_id, int wait)
{
    auto window = WineWindowManager::GetInstance().GetWindow(widget_id);
    if (window) {
        return window->GetNativeWindow(wait != 0);
    }
    /* client surface XComponents register their OHNativeWindow directly in
     * WindowAdapter, they have no WineWindow */
    return WindowAdapter::GetInstance().GetWindow(widget_id);
}

/* LTPO frame-rate hint for Vulkan client surfaces.
 *
 * The DXVK swap chain presents directly on the client XComponent's
 * OHNativeWindow, so frame production is paced by how fast the compositor
 * releases buffers, which follows the panel refresh rate.  With no hint the
 * panel stays at 60Hz and the game is capped at ~60fps regardless of how
 * fast it renders (a pure render loop measures >400fps).  Keeping a vsync
 * client with an expected rate range of {60,120,120} active while a Vulkan
 * client surface exists makes the dynamic refresh policy lift the panel to
 * its top rate, so the swap chain can run at up to ~120fps.  This mirrors
 * the reference implementation, whose renderer sets the same range. */
static std::atomic<int> g_vulkan_surface_count{0};
static std::atomic<bool> g_vulkan_vsync_running{false};
static OH_NativeVSync* g_vulkan_vsync = nullptr;

static void vulkan_vsync_tick(long long timestamp, void* data)
{
    if (g_vulkan_vsync_running.load(std::memory_order_relaxed))
        OH_NativeVSync_RequestFrame(g_vulkan_vsync, vulkan_vsync_tick, nullptr);
}

static void vulkan_frame_rate_hint_start(void)
{
    if (g_vulkan_surface_count.fetch_add(1) != 0) return;

    static const char name[] = "WineVulkan";
    g_vulkan_vsync = OH_NativeVSync_Create(name, sizeof(name) - 1);
    if (!g_vulkan_vsync) {
        LOGE("vulkan frame rate hint: NativeVSync create failed");
        g_vulkan_surface_count.store(0);
        return;
    }
    OH_NativeVSync_ExpectedRateRange range = {60, 120, 120};
    int ret = OH_NativeVSync_SetExpectedFrameRateRange(g_vulkan_vsync, &range);
    if (ret != 0)
        LOGE("vulkan frame rate hint: set range failed, ret=%{public}d", ret);
    g_vulkan_vsync_running.store(true);
    OH_NativeVSync_RequestFrame(g_vulkan_vsync, vulkan_vsync_tick, nullptr);
    LOGI("vulkan frame rate hint started ({60,120,120})");
}

static void vulkan_frame_rate_hint_stop(void)
{
    if (g_vulkan_surface_count.fetch_sub(1) != 1) return;
    /* stop re-arming; the vsync connection goes idle.  The handle is kept
     * so it can be reused (and to avoid destroying it while a callback may
     * still be in flight). */
    g_vulkan_vsync_running.store(false);
    LOGI("vulkan frame rate hint stopped");
}

int CreateClientSurface(struct client_surface_param* param)
{
    if (!param || param->parent_widget_id <= 0) {
        LOGE("CreateClientSurface: invalid param");
        return -1;
    }
    param->client_widget_id = WindowAdapter::GetInstance().NextWindowWidgetId();

    auto promise = std::make_shared<std::promise<bool>>();
    auto future = promise->get_future();
    std::function<void(bool)> callback = [promise](bool success) {
        promise->set_value(success);
    };

    auto jsFunc = GetJSFunction("ClientSurface.Create");
    if (!jsFunc) {
        LOGE("CreateClientSurface: ClientSurface.Create not found");
        return -1;
    }
    jsFunc->Invoke<void>(param->parent_widget_id, param->client_widget_id,
                         param->off_x, param->off_y,
                         param->width, param->height, callback);

    auto status = future.wait_for(std::chrono::seconds(3));
    if (status != std::future_status::ready || !future.get()) {
        LOGE("CreateClientSurface: parent %{public}d has no page renderer",
             param->parent_widget_id);
        return -1;
    }

    /* the XComponent surface is created asynchronously, wait until it is
     * registered in WindowAdapter */
    for (int i = 0; i < 100; i++) {
        if (WindowAdapter::GetInstance().GetWindow(param->client_widget_id)) {
            LOGI("CreateClientSurface success, parent %{public}d, client %{public}d",
                 param->parent_widget_id, param->client_widget_id);
            vulkan_frame_rate_hint_start();
            return param->client_widget_id;
        }
        usleep(50 * 1000);
    }
    LOGE("CreateClientSurface timeout, parent %{public}d, client %{public}d",
         param->parent_widget_id, param->client_widget_id);
    return -1;
}

void SetClientSurfaceBounds(int client_widget_id, int off_x, int off_y,
                            int width, int height)
{
    auto jsFunc = GetJSFunction("ClientSurface.SetBounds");
    if (!jsFunc) {
        LOGE("SetClientSurfaceBounds: ClientSurface.SetBounds not found");
        return;
    }
    jsFunc->Invoke<void>(client_widget_id, off_x, off_y, width, height);
}

void DestroyClientSurface(int client_widget_id)
{
    vulkan_frame_rate_hint_stop();
    auto jsFunc = GetJSFunction("ClientSurface.Destroy");
    if (!jsFunc) {
        LOGE("DestroyClientSurface: ClientSurface.Destroy not found");
        return;
    }
    jsFunc->Invoke<void>(client_widget_id);
}

static int create_client_surface_and_get_surface_id(
    struct client_surface_param* param, uint64_t* surface_id)
{
    int client_widget_id = CreateClientSurface(param);
    if (client_widget_id <= 0) return -1;

    ohos_native_window window = GetNativeWindow(client_widget_id, 0);
    if (!window || OH_NativeWindow_GetSurfaceId(window, surface_id) != 0) {
        LOGE("get surface id failed, client widget %{public}d",
             client_widget_id);
        DestroyClientSurface(client_widget_id);
        return -1;
    }
    param->client_widget_id = client_widget_id;
    return 0;
}

int CreateRemoteClientSurface(struct client_surface_param* param,
                              uint64_t* surface_id)
{
    if (!param || !surface_id || param->parent_widget_id <= 0) {
        LOGE("CreateRemoteClientSurface: invalid param");
        return -1;
    }

    /* Process/window embedding is arbitrary (independent of the spawn
     * chain): connect point-to-point to whichever process owns the
     * foreign toplevel to request the surface */
    if (param->owner_pid > 0 && param->owner_pid != getpid()) {
        new_ipc::ClientSurfaceParam p;
        p.parent_widget_id = param->parent_widget_id;
        p.off_x = param->off_x;
        p.off_y = param->off_y;
        p.width = param->width;
        p.height = param->height;
        p.src_hwnd = param->src_hwnd;
        p.src_pid = param->src_pid;
        p.owner_pid = param->owner_pid;
        if (new_ipc::PeerManager::Instance().CreateClientSurface(
                p, *surface_id) != 0) {
            LOGE("CreateRemoteClientSurface: peer relay failed, "
                 "owner_pid=%{public}d", param->owner_pid);
            return -1;
        }
        param->client_widget_id = p.client_widget_id;
        LOGI("CreateRemoteClientSurface: client=%{public}d, "
             "surface_id=%{public}" PRIu64 ", owner_pid=%{public}d",
             param->client_widget_id, *surface_id, param->owner_pid);
        return 0;
    }

    return create_client_surface_and_get_surface_id(param, surface_id);
}

void SetRemoteClientSurfaceBounds(int client_widget_id, int off_x, int off_y,
                                  int width, int height)
{
    int32_t owner_pid = 0;
    if (new_ipc::PeerManager::Instance().GetSurfaceOwner(client_widget_id,
                                                         owner_pid)) {
        new_ipc::PeerManager::Instance().SetClientSurfaceBounds(
            client_widget_id, off_x, off_y, width, height);
        return;
    }
    SetClientSurfaceBounds(client_widget_id, off_x, off_y, width, height);
}

void DestroyRemoteClientSurface(int client_widget_id)
{
    int32_t owner_pid = 0;
    if (new_ipc::PeerManager::Instance().GetSurfaceOwner(client_widget_id,
                                                         owner_pid)) {
        new_ipc::PeerManager::Instance().DestroyClientSurface(
            client_widget_id);
        return;
    }
    DestroyClientSurface(client_widget_id);
}

/* C-side shared pixel mapping registry: released on destroy */
static std::unordered_map<int, std::pair<void*, size_t>> g_gdi_maps;
static std::mutex g_gdi_maps_mutex;

int CreateRemoteGdiSharedSurface(struct remote_gdi_param* param, void** pixels)
{
    if (!param || !pixels || param->parent_widget_id <= 0 ||
        param->owner_pid <= 0 || param->width <= 0 || param->height <= 0 ||
        param->owner_pid == getpid()) {
        LOGE("CreateRemoteGdiSharedSurface: invalid param");
        return -1;
    }
    param->stride = param->width * 4;
    *pixels = nullptr;

    size_t size = (size_t)param->stride * param->height;
    int fd = open("/dev/ashmem", O_RDWR);
    if (fd < 0) {
        LOGE("CreateRemoteGdiSharedSurface: open ashmem failed, errno=%{public}d", errno);
        return -1;
    }
    if (ioctl(fd, ASHMEM_SET_SIZE, size) < 0) {
        LOGE("CreateRemoteGdiSharedSurface: set size failed, errno=%{public}d", errno);
        close(fd);
        return -1;
    }
    void* map = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) {
        LOGE("CreateRemoteGdiSharedSurface: mmap failed, errno=%{public}d", errno);
        close(fd);
        return -1;
    }
    /* Fill with neutral grey to avoid garbage pixels in the first
     * composite */
    memset(map, 0xc0, size);

    new_ipc::RemoteGdiParam p;
    p.parent_widget_id = param->parent_widget_id;
    p.off_x = param->off_x;
    p.off_y = param->off_y;
    p.width = param->width;
    p.height = param->height;
    p.src_hwnd = param->src_hwnd;
    p.src_pid = param->src_pid;
    p.owner_pid = param->owner_pid;
    p.stride = param->stride;
    p.client_widget_id = -1; /* assigned by the owner */

    if (new_ipc::PeerManager::Instance().AttachRemoteGdi(p, fd) != 0) {
        /* Send failed: the fd was duplicated by the kernel but is still
         * held locally, clean it up */
        munmap(map, size);
        close(fd);
        LOGE("CreateRemoteGdiSharedSurface: relay failed, owner_pid=%{public}d",
             param->owner_pid);
        return -1;
    }
    close(fd); /* the mmap keeps it alive, the fd can be closed */
    param->client_widget_id = p.client_widget_id;
    *pixels = map;
    {
        std::lock_guard<std::mutex> lock(g_gdi_maps_mutex);
        g_gdi_maps[param->client_widget_id] = {map, size};
    }
    LOGI("CreateRemoteGdiSharedSurface: id=%{public}d, owner_pid=%{public}d, "
         "%{public}dx%{public}d", param->client_widget_id, param->owner_pid,
         param->width, param->height);
    return 0;
}

int RemoteGdiFlush(int client_widget_id, int x, int y, int width, int height)
{
    return new_ipc::PeerManager::Instance().RemoteGdiFlush(
        client_widget_id, x, y, width, height);
}

void DestroyRemoteGdiSharedSurface(int client_widget_id)
{
    void* map = nullptr;
    size_t size = 0;
    {
        std::lock_guard<std::mutex> lock(g_gdi_maps_mutex);
        auto it = g_gdi_maps.find(client_widget_id);
        if (it == g_gdi_maps.end()) return;
        map = it->second.first;
        size = it->second.second;
        g_gdi_maps.erase(it);
    }
    new_ipc::PeerManager::Instance().DestroyRemoteGdi(client_widget_id);
    if (map) munmap(map, size);
}

void UpdateRemoteGdiBounds(int client_widget_id, int off_x, int off_y)
{
    new_ipc::PeerManager::Instance().UpdateRemoteGdiBounds(client_widget_id,
                                                           off_x, off_y);
}

void SetRemoteGdiSharedSurfaceVisible(int client_widget_id, int visible)
{
    new_ipc::PeerManager::Instance().SetRemoteGdiVisible(client_widget_id,
                                                         visible);
}

/* Bridge the C callback to the new_ipc-internal overlay notification */
static remote_gdi_overlay_fn g_remote_gdi_fn = nullptr;

static void remote_gdi_overlay_bridge(new_ipc::RemoteGdiOp op, int32_t id,
                                      const new_ipc::RemoteGdiParam* param,
                                      const void* pixels)
{
    if (!g_remote_gdi_fn) return;
    int parent_id = 0, off_x = 0, off_y = 0, w = 0, h = 0, stride = 0;
    if (param) {
        parent_id = param->parent_widget_id;
        off_x = param->off_x;
        off_y = param->off_y;
        w = param->width;
        h = param->height;
        stride = param->stride;
    }
    g_remote_gdi_fn(static_cast<int>(op), id, parent_id, off_x, off_y, w, h,
                    stride, pixels);
}

void RegisterRemoteGdiOverlayCallback(remote_gdi_overlay_fn fn)
{
    g_remote_gdi_fn = fn;
    new_ipc::SetRemoteGdiOverlayCallback(remote_gdi_overlay_bridge);
}

int GetFocusWindow()
{
    return XComponentManager::GetInstance()->GetActivateWindow();
}

struct window_bounds GetDisplayAvailArea()
{
    auto rect = ScreenAdapter::GetInstance().GetDisplayAvailArea();
    return {rect.left, rect.top, rect.width, rect.height};
}

int GetSystemDpi()
{
    return ScreenAdapter::GetInstance().GetSystemDpi();
}

float GetDensityPixels()
{
    return ScreenAdapter::GetInstance().GetDensityPixels();
}

int CaptureScreen(struct screenshot_frame *frame)
{
    if (!frame) return -1;
    frame->width = 0;
    frame->height = 0;
    frame->stride = 0;
    frame->size = 0;
    frame->data = nullptr;

    auto desktop = ScreenshotAdapter::GetInstance().StartCapture();
    if (!desktop || !desktop->data() ||
        desktop->size().width() <= 0 || desktop->size().height() <= 0) {
        LOGE("CaptureScreen: no frame captured");
        return -1;
    }

    int32_t width = desktop->size().width();
    int32_t height = desktop->size().height();
    int32_t src_stride = desktop->stride();
    int32_t dst_stride = width * 4;
    int64_t size = static_cast<int64_t>(dst_stride) * height;

    uint8_t *dst = static_cast<uint8_t*>(malloc(static_cast<size_t>(size)));
    if (!dst) {
        LOGE("CaptureScreen: out of memory");
        return -1;
    }

    /* Normalize to BGRA_8888, which is what a 32bpp BI_RGB DIB expects.
     * image.PixelMapFormat: RGBA_8888 = 3, BGRA_8888 = 4. */
    const int32_t format = desktop->format();
    if (format != 3 /* RGBA_8888 */ && format != 4 /* BGRA_8888 */) {
        LOGE("CaptureScreen: unsupported pixel format %d", format);
        free(dst);
        return -1;
    }
    const bool src_bgra = (format == 4 /* BGRA_8888 */);
    const uint8_t *src = desktop->data();
    for (int32_t y = 0; y < height; ++y) {
        const uint8_t *s = src + static_cast<size_t>(y) * src_stride;
        uint8_t *d = dst + static_cast<size_t>(y) * dst_stride;
        if (src_bgra) {
            memcpy(d, s, static_cast<size_t>(width) * 4);
            for (int32_t x = 0; x < width; ++x) d[x * 4 + 3] = 0xff;
        } else {
            for (int32_t x = 0; x < width; ++x, s += 4, d += 4) {
                d[0] = s[2];  /* B */
                d[1] = s[1];  /* G */
                d[2] = s[0];  /* R */
                d[3] = 0xff;  /* A */
            }
        }
    }

    frame->width = width;
    frame->height = height;
    frame->stride = dst_stride;
    frame->size = size;
    frame->data = dst;
    return 0;
}

void ReleaseScreenFrame(struct screenshot_frame *frame)
{
    if (!frame) return;
    free(frame->data);
    frame->data = nullptr;
    frame->size = 0;
}

int GetGlobalDescriptor(int key)
{
    std::vector<std::pair<int, int>> ids_fds_map = AppSpawnCommunication::GetFdIdsRemap();
    for (const auto& [rawfd, remap_fd] : ids_fds_map) {
        LOGI("GetGlobalDescriptor fd %{public}d => %{public}d", rawfd, remap_fd);
        if (remap_fd == key) {
            return rawfd;
        }
    }
    return -1;
}

/* Unlink this process's wine_ipc_<pid>.sock.  Deliberately does NOT call
 * IpcServer::Stop(): the accept thread sits blocked in accept() while
 * holding the socket send mutex, so Stop()'s Close() deadlocks on that
 * mutex, and the thread joins would hang anyway (a closed fd does not
 * wake a blocked accept/recvmsg).  Threads and fds are torn down by
 * process death itself; unlink(2) only clears the stale name and is
 * async-signal-safe. */
static void CleanupIpcServer()
{
    LOGI("CleanupIpcServer pid=%{public}d", static_cast<int>(getpid()));
    unlink(new_ipc::IpcServer::SocketPathForPid(getpid()).c_str());
}

void KillNativeProcess(int exitcode)
{
    int pid = getpid();
    LOGI("KillNativeProcess pid %{public}d, exitcode %{public}#x", pid, exitcode);
    /* Both normal exit (process_exit_wrapper) and fatal/signal crash
     * paths converge here: this environment never calls exit(), static
     * destructors do not run, so wine_ipc_<pid>.sock must be cleaned up
     * explicitly before handing over to the JS-side kill.  Mark the wine
     * thread as orderly-exiting FIRST so the exit watchdog's liveness
     * monitor does not mistake this controlled death for a crash. */
    watchdog::MarkWineOrderlyExit();
    CleanupIpcServer();
    ipc_lite::IpcLiteHelper::KillProcess();
}

int IsMainProcess()
{
    return CommandLineHelper::IsMainProcess() ? 1 : 0;
}

int GetMainProcessId()
{
    return CommandLineHelper::GetMainProcessId();
}

void RegisterInputEventCallBack(input_event_callback callback)
{
    EventManager::GetInstance().RegisterInputEventCallBack(callback);
}

void RegisterWindowEventCallBack(input_event_callback callback)
{
    EventManager::GetInstance().RegisterWindowEventCallBack(callback);
}

void RegisterIMEEventCallBack(ime_event_callback callback)
{
    if (callback == nullptr) {
        LOGE("ime_event_callback is nullptr!");
        return;
    }
    WineIMEDelegate::GetInstance().RegisterIMEEventCallBack(callback);
}

void IMEAttachTextInput(bool show_keyboard, const struct ime_text_config* text_config, int32_t request_keyboard_reason)
{
    if (text_config == nullptr) {
        LOGE("text_config is nullptr!");
        return;
    }
    ohos::adapter::IMFAdapterTextConfig config;
    config.inputAttribute.inputPattern = text_config->input_attribute.input_pattern;
    config.inputAttribute.enterKeyType = text_config->input_attribute.enter_key_type;
    config.cursorInfo.left = text_config->cursor_info.left;
    config.cursorInfo.top = text_config->cursor_info.top;
    config.cursorInfo.width = text_config->cursor_info.width;
    config.cursorInfo.height = text_config->cursor_info.height;
    config.range.start = text_config->range.start;
    config.range.end = text_config->range.end;
    InputMethodAdapter::GetInstance().AttachTextInput(show_keyboard, config, request_keyboard_reason);
}

void IMEDetachTextInput()
{
    InputMethodAdapter::GetInstance().DetachTextInput();
}

void IMEUpdateAttribute(const struct ime_input_attribute input_attribute)
{
    IMFAdapterInputAttribute attribute;
    attribute.inputPattern = input_attribute.input_pattern;
    attribute.enterKeyType = input_attribute.enter_key_type;
    InputMethodAdapter::GetInstance().UpdateAttribute(attribute);
}

void IMENotifyCursorUpdate(const struct ime_cursor_info* cursor_info)
{
    if (cursor_info == nullptr) {
        LOGE("cursor_info is nullptr!");
        return;
    }
    ohos::adapter::IMFAdapterCursorInfo info;
    info.left = cursor_info->left;
    info.top = cursor_info->top;
    info.width = cursor_info->width;
    info.height = cursor_info->height;
    InputMethodAdapter::GetInstance().NotifyCursorUpdate(info);
}

void StartListeningForScreenlock(screenlock_event_callback callback)
{
    if (callback == nullptr) {
        LOGE("screenlock_event_callback is nullptr!");
        return;
    }
    ScreenlockMonitorAdapter::GetInstance().StartListeningForScreenlock(callback);
}

int OpenNativeUrl(const char *url)
{
    LOGI("OpenNativeUrl: url=%{public}s", url ? url : "(null)");
    if (!url) {
        LOGE("OpenNativeUrl: url is nullptr");
        return -1;
    }
    ohos::adapter::ExternalProtocolAdapter::GetInstance().OpenExternal(std::string(url));
    return 0;
}

int OpenNativeFileWithChooser(const char *path)
{
    LOGI("OpenNativeFileWithChooser: path=%{public}s", path ? path : "(null)");
    if (!path) {
        LOGE("OpenNativeFileWithChooser: path is nullptr");
        return -1;
    }
    FileManagerAdapter::GetInstance().OpenVerifiedItemWithChooser(std::string(path));
    return 0;
}

int GetPrinters(struct ohos_printer_info *printers, uint32_t capacity)
{
    return ohos::adapter::print::PrintAdapter::GetInstance().GetPrinters(printers, capacity);
}

int PrintPostScript(const char *printer_id, const char *job_name,
                        const void *data, uint32_t length)
{
    if (!data || !length) {
        LOGE("PrintPostScript: invalid input, data=%{public}p length=%{public}u",
            data, length);
        return -1;
    }
    std::shared_ptr<char[]> buffer(new (std::nothrow) char[length]);
    if (!buffer) {
        LOGE("PrintPostScript: buffer allocation failed, length=%{public}u", length);
        return -1;
    }
    std::memcpy(buffer.get(), data, length);
    return ohos::adapter::print::PrintAdapter::GetInstance().PrintPostScript(
        printer_id ? printer_id : "", job_name ? job_name : "", buffer, length) ? 0 : -1;
}
EXTERN_C_END

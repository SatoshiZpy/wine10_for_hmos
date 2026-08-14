// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/new_ipc/ipc_adapter.h"

#include <unistd.h>

#include <chrono>

#include "ohos/adapter/common/constants.h"
#include "ohos/adapter/common/logging.h"
#include "ohos/adapter/common/mutex.h"
#include "ohos/adapter/external_protocol/external_protocol_adapter.h"
#include "ohos/adapter/ipc/window_param.h"
#include "ohos/adapter/multiprocess/child_process_starter.h"
#include "ohos/adapter/new_ipc/ipc_proxy_stub.h"
#include "ohos/adapter/screen/screen_adapter.h"
#include "ohos/adapter/window/wine_window.h"
#include "ohos/adapter/window/wine_window_manager.h"

namespace ohos::adapter::new_ipc {

static Mutex g_process_mutex;

static const std::string kIpcSocketPathPrefix =
    "/data/storage/el2/base/files/wine_ipc_";

static window::WindowParam ConvertWindowParam(const WindowParam& param)
{
    window::WindowParam win_param;
    win_param.ability_type = param.ability_type;
    win_param.window_type = param.window_type;
    win_param.left = param.left;
    win_param.top = param.top;
    win_param.width = param.width;
    win_param.height = param.height;
    win_param.parent_id = param.parent_id;
    win_param.hwnd = param.hwnd;
    win_param.owner_pid = param.owner_pid;
    win_param.features = param.features;
    win_param.window_state = param.window_state;
    return win_param;
}

static std::string GetProcessName(const std::string& image_file)
{
    std::string process_name = image_file;
    size_t last_slash = process_name.rfind('\\');
    if (last_slash != std::string::npos) {
        process_name = process_name.substr(last_slash + 1);
    }
    size_t last_dot = process_name.rfind('.');
    if (last_dot != std::string::npos) {
        return process_name.substr(0, last_dot);
    }
    return process_name;
}

ChildProcess& ChildProcess::Instance()
{
    static ChildProcess instance;
    return instance;
}

ChildProcess::ChildProcess() = default;

ChildProcess::~ChildProcess()
{
    if (channel_) channel_->Close();
}

bool ChildProcess::ConnectToParent(const std::string& socket_path)
{
    if (connected_) {
        LOGW("ChildProcess already connected");
        return true;
    }

    channel_ = std::make_unique<IpcChannel>();
    if (!channel_->ConnectToServer(socket_path)) {
        LOGE("ChildProcess ConnectToParent failed, path=%{public}s",
             socket_path.c_str());
        channel_.reset();
        return false;
    }

    if (!channel_->ExchangeShmFd()) {
        LOGE("ChildProcess ExchangeShmFd failed");
        channel_->Close();
        channel_.reset();
        return false;
    }

    channel_->SetRequestHandler([this](const IpcMessage& msg) {
        if (msg.type != IpcMessageType::SYNC_REQUEST || !msg.shm) return;
        msg.shm->ResetReadWrite();
        IpcParcel data(*msg.shm);
        int32_t code = 0;
        data.ReadInt32(code);
        IpcParcel reply(*msg.shm);
        stub_.OnRemoteRequest(static_cast<uint32_t>(code), data, reply,
                              msg.fds);
        channel_->SendReply();
    });

    channel_->StartListening();

    proxy_ = std::make_unique<ProcessManagerProxy>(*channel_);
    connected_ = true;
    LOGI("ChildProcess connected to parent via %{public}s",
         socket_path.c_str());

    std::vector<std::pair<int, int>> fds;
    if (proxy_->GetFdIdsRemap(fds) == 0) {
        fd_ids_remap_ = fds;
        LOGI("ChildProcess got %{public}zu fd remap entries",
             fds.size());
    } else {
        LOGW("ChildProcess GetFdIdsRemap failed");
    }
    return true;
}

int ChildProcess::GetDisplay(DisplayParam& display)
{
    if (!connected_ || !proxy_) return -1;
    return proxy_->GetDisplay(display);
}

int ChildProcess::CreateProcess(
    const std::vector<std::string>& commands,
    const std::vector<std::pair<int, int>>& fds)
{
    if (!connected_ || !proxy_) return -1;
    return proxy_->CreateProcess(commands, fds);
}

int ChildProcess::KillProcess(int32_t pid)
{
    if (!connected_ || !proxy_) return -1;
    return proxy_->KillProcess(pid);
}

int ChildProcess::OpenUrl(const std::string& url)
{
    if (!connected_ || !proxy_) return -1;
    return proxy_->OpenUrl(url);
}

int ChildProcess::GetFdIdsRemap(std::vector<std::pair<int, int>>& fds)
{
    fds = fd_ids_remap_;
    return 0;
}

void ChildProcess::SetFdIdsRemap(const std::vector<std::pair<int, int>>& fds)
{
    fd_ids_remap_ = fds;
}

int IpcHelper::CreateProcess(
    const std::vector<std::string>& commands,
    const std::vector<std::pair<int, int>>& fds)
{
    MutexLock lock(g_process_mutex);

    if (commands.empty()) {
        LOGE("IpcHelper::CreateProcess empty commands");
        return -1;
    }

    std::string process_loader = commands[0];
    std::string image_file = process_loader == "wineserver"
        ? process_loader : commands[1];
    std::string processKey = GetProcessName(image_file);

    std::string socket_path = kIpcSocketPathPrefix
        + std::to_string(std::chrono::steady_clock::now()
              .time_since_epoch().count())
        + ".sock";

    if (!ChildProcessHost::Instance().StartServer(socket_path)) {
        LOGE("IpcHelper CreateProcess: StartServer failed");
        return -1;
    }

    if (!multiprocess::ChildProcessStarter::GetInstance()
             .CreateProcess(processKey, socket_path)) {
        LOGE("IpcHelper CreateProcess: CreateProcess failed");
        return -1;
    }

    LOGI("IpcHelper CreateProcess: sock=%{public}s", socket_path.c_str());

    int32_t channel_id = ChildProcessHost::Instance().AcceptNewChild();
    if (channel_id < 0) {
        LOGE("AcceptNewChild failed, channel_id=%{public}d", channel_id);
        return -1;
    }

    if (!ChildProcessHost::Instance().SetProcessInfo(
            channel_id, commands, fds)) {
        LOGE("SetProcessInfo failed for channel %{public}d", channel_id);
    }

    IpcChannel* channel = ChildProcessHost::Instance()
        .GetManager()->GetChannel(channel_id);
    if (!channel) {
        LOGE("GetChannel failed for channel %{public}d", channel_id);
        return -1;
    }

    ProcessManagerProxy proxy(*channel);
    int32_t pid = -1;
    if (proxy.GetPid(pid) != 0 || pid <= 0) {
        LOGE("GetPid via IPC failed");
        return -1;
    }

    LOGI("IpcHelper CreateProcess done: pid=%{public}d, "
         "channel=%{public}d", pid, channel_id);
    return pid;
}

int IpcHelper::KillProcess(int32_t pid)
{
    LOGI("IpcHelper::KillProcess pid=%{public}d (pending impl)",
         pid);
    return -1;
}

int IpcHelper::CreateWindow(const WindowParam& param)
{
    LOGI("CreateWindow type: %{public}d, rect: "
         "(%{public}d,%{public}d,%{public}d,%{public}d)",
         param.window_type, param.left, param.top,
         param.width, param.height);

    int widget_id = -1;
    auto window = window::WineWindow::Create(
        ConvertWindowParam(param), &widget_id);
    if (!window) {
        LOGE("CreateWindow error");
        return -1;
    }
    LOGI("CreateWindow widgetId %{public}d", widget_id);
    return widget_id;
}

int IpcHelper::UpdateWindow(int32_t widget_id,
                                      const WindowParam& param)
{
    auto window = window::WineWindowManager::GetInstance()
        .GetWindow(widget_id);
    if (!window) {
        LOGE("UpdateWindow: window not exists, widgetId %{public}d",
             widget_id);
        return -1;
    }
    LOGI("UpdateWindow widgetId %{public}d", widget_id);
    return 0;
}

int IpcHelper::SetWindowBounds(int32_t widget_id,
                                         const WindowRect& rect)
{
    auto window = window::WineWindowManager::GetInstance()
        .GetWindow(widget_id);
    if (!window) {
        LOGE("SetWindowBounds: window not exists, widgetId %{public}d",
             widget_id);
        return -1;
    }
    LOGI("SetWindowBounds widgetId %{public}d", widget_id);
    ::WindowRect common_rect;
    common_rect.left = rect.left;
    common_rect.top = rect.top;
    common_rect.width = rect.width;
    common_rect.height = rect.height;
    window->SetBounds(common_rect);
    return 0;
}

int IpcHelper::SetWindowVisible(int32_t widget_id, bool visible)
{
    auto window = window::WineWindowManager::GetInstance()
        .GetWindow(widget_id);
    if (!window) {
        LOGE("SetWindowVisible: window not exists, widgetId %{public}d",
             widget_id);
        return -1;
    }
    LOGI("SetWindowVisible widgetId %{public}d, visible %{public}d",
         widget_id, static_cast<int>(visible));
    visible ? window->Show() : window->Hide();
    return 0;
}

int IpcHelper::CloseWindow(int32_t widget_id)
{
    auto window = window::WineWindowManager::GetInstance()
        .GetWindow(widget_id);
    if (!window) {
        LOGE("CloseWindow: window not exists, widgetId %{public}d",
             widget_id);
        return -1;
    }
    LOGI("CloseWindow widgetId %{public}d", widget_id);
    window->Close();
    return 0;
}

int IpcHelper::SetWindowTitle(int32_t widget_id,
                                        const std::string& title)
{
    auto window = window::WineWindowManager::GetInstance()
        .GetWindow(widget_id);
    if (!window) {
        LOGE("SetWindowTitle: window not exists, widgetId %{public}d",
             widget_id);
        return -1;
    }
    LOGI("SetWindowTitle widgetId %{public}d", widget_id);
    window->SetTitle(title);
    return 0;
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

ChildProcessHost& ChildProcessHost::Instance()
{
    static ChildProcessHost instance;
    return instance;
}

ChildProcessHost::ChildProcessHost()
{
    manager_ = std::make_unique<IpcChannelManager>();
}

ChildProcessHost::~ChildProcessHost()
{
    CloseAll();
}

bool ChildProcessHost::StartServer(const std::string& socket_path)
{
    if (server_started_) {
        LOGW("ChildProcessHost server already started at %{public}s",
             server_socket_path_.c_str());
        return true;
    }
    bool ret = manager_->StartServer(socket_path);
    if (ret) {
        server_started_ = true;
        server_socket_path_ = socket_path;
    }
    return ret;
}

int32_t ChildProcessHost::AcceptNewChild()
{
    int32_t channel_id = manager_->AcceptNewChannel();
    if (channel_id < 0) return channel_id;

    IpcChannel* channel = manager_->GetChannel(channel_id);
    if (!channel) return -1;

    if (!channel->ExchangeShmFd()) {
        LOGE("ChildProcessHost AcceptNewChild ExchangeShmFd failed");
        manager_->RemoveChannel(channel_id);
        return -1;
    }

    ProcessManagerStub& stub = stub_;
    IpcChannel* ch_ptr = channel;
    channel->SetRequestHandler([ch_ptr, &stub](const IpcMessage& msg) {
        if (msg.type != IpcMessageType::SYNC_REQUEST || !msg.shm) return;
        msg.shm->ResetReadWrite();
        IpcParcel data(*msg.shm);
        int32_t code = 0;
        data.ReadInt32(code);
        IpcParcel reply(*msg.shm);
        stub.OnRemoteRequest(static_cast<uint32_t>(code), data, reply,
                             msg.fds);
        ch_ptr->SendReply();
    });

    channel->StartListening();

    LOGI("ChildProcessHost AcceptNewChild channel_id=%{public}d, "
         "shm exchanged, listening started", channel_id);
    return channel_id;
}

bool ChildProcessHost::SetProcessInfo(
    int32_t channel_id,
    const std::vector<std::string>& commands,
    const std::vector<std::pair<int, int>>& fds)
{
    auto* channel = manager_->GetChannel(channel_id);
    if (!channel) return false;
    ChildProcessProxy proxy(*channel);
    return proxy.SetProcessInfo(commands, fds) == 0;
}

bool ChildProcessHost::SetWindowInfo(int32_t channel_id,
                                      int32_t widget_id,
                                      const WindowAttr& attr)
{
    auto* channel = manager_->GetChannel(channel_id);
    if (!channel) return false;
    ChildProcessProxy proxy(*channel);
    return proxy.SetWindowInfo(widget_id, attr) == 0;
}

bool ChildProcessHost::SendInputEvent(int32_t channel_id,
                                       const std::vector<int8_t>& event_data)
{
    auto* channel = manager_->GetChannel(channel_id);
    if (!channel) return false;
    ChildProcessProxy proxy(*channel);
    return proxy.SendInputEvent(event_data) == 0;
}

void ChildProcessHost::CloseAll()
{
    manager_->CloseAll();
}

}  // namespace ohos::adapter::new_ipc

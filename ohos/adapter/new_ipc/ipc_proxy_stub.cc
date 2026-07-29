// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/new_ipc/ipc_proxy_stub.h"

#include <unistd.h>

#include "ohos/adapter/common/logging.h"
#include "ohos/adapter/multiprocess/app_spawn_communication.h"
#include "ohos/adapter/new_ipc/ipc_adapter.h"

namespace ohos::adapter::new_ipc {

IpcProxy::IpcProxy(IpcChannel& channel) : channel_(channel) {}

bool IpcProxy::SendRequest(uint32_t code, IpcParcel& data, IpcParcel& reply)
{
    SharedMemory* shm = channel_.GetShm();
    if (!shm) {
        LOGE("IpcProxy::SendRequest no shm");
        return false;
    }

    shm->ResetReadWrite();
    IpcParcel send_parcel(*shm);
    send_parcel.WriteInt32(static_cast<int32_t>(code));
    while (data.HasData()) {
        int32_t val;
        if (data.ReadInt32(val)) {
            send_parcel.WriteInt32(val);
            continue;
        }
        int64_t val64;
        if (data.ReadInt64(val64)) {
            send_parcel.WriteInt64(val64);
            continue;
        }
        float fval;
        if (data.ReadFloat(fval)) {
            send_parcel.WriteFloat(fval);
            continue;
        }
        std::string str;
        if (data.ReadString(str)) {
            send_parcel.WriteString(str);
            continue;
        }
        break;
    }

    if (!channel_.SendRequest()) {
        LOGE("IpcProxy::SendRequest SendRequest failed");
        return false;
    }

    if (!channel_.WaitForReply()) {
        LOGE("IpcProxy::SendRequest WaitForReply failed");
        return false;
    }

    shm->ResetReadWrite();
    IpcParcel recv_parcel(*shm);
    int32_t ret = -1;
    recv_parcel.ReadInt32(ret);
    while (recv_parcel.HasData()) {
        int32_t val;
        if (recv_parcel.ReadInt32(val)) {
            reply.WriteInt32(val);
            continue;
        }
        int64_t val64;
        if (recv_parcel.ReadInt64(val64)) {
            reply.WriteInt64(val64);
            continue;
        }
        float fval;
        if (recv_parcel.ReadFloat(fval)) {
            reply.WriteFloat(fval);
            continue;
        }
        std::string str;
        if (recv_parcel.ReadString(str)) {
            reply.WriteString(str);
            continue;
        }
        break;
    }

    LOGI("IpcProxy::SendRequest code=%{public}u ret=%{public}d", code, ret);
    return ret == 0;
}

IpcStub::IpcStub(const std::string& descriptor) : descriptor_(descriptor) {}

void IpcStub::SetRequestHandler(IpcRequestHandler handler) { handler_ = handler; }

int IpcStub::OnRemoteRequest(uint32_t code, IpcParcel& data, IpcParcel& reply,
                              const std::vector<int>& fds)
{
    if (handler_) return handler_(code, data, reply, fds);
    LOGE("IpcStub::OnRemoteRequest no handler for code %{public}u", code);
    return -1;
}

ProcessManagerProxy::ProcessManagerProxy(IpcChannel& channel) : channel_(channel) {}

int ProcessManagerProxy::CreateProcess(
    const std::vector<std::string>& commands,
    const std::vector<std::pair<int, int>>& fds)
{
    SharedMemory* shm = channel_.GetShm();
    if (!shm) return -1;
    shm->ResetReadWrite();
    IpcParcel parcel(*shm);
    parcel.WriteInt32(static_cast<int32_t>(COMMAND_CREATE_PROCESS));
    parcel.WriteInt32(static_cast<int32_t>(commands.size()));
    for (const auto& cmd : commands) parcel.WriteString(cmd);
    parcel.WriteInt32(static_cast<int32_t>(fds.size()));
    for (const auto& [fd, remap] : fds) {
        parcel.WriteInt32(remap);
    }

    std::vector<int> actual_fds;
    for (const auto& [fd, remap] : fds) {
        actual_fds.push_back(fd);
    }

    if (!channel_.SendRequestWithFds(actual_fds)) return -1;
    if (!channel_.WaitForReply()) return -1;

    shm->ResetReadWrite();
    IpcParcel reply(*shm);
    int32_t ret = -1;
    reply.ReadInt32(ret);
    return ret;
}

int ProcessManagerProxy::KillProcess(int32_t pid)
{
    SharedMemory* shm = channel_.GetShm();
    if (!shm) return -1;
    shm->ResetReadWrite();
    IpcParcel parcel(*shm);
    parcel.WriteInt32(static_cast<int32_t>(COMMAND_KILL_PROCESS));
    parcel.WriteInt32(pid);

    if (!channel_.SendRequest()) return -1;
    if (!channel_.WaitForReply()) return -1;

    shm->ResetReadWrite();
    IpcParcel reply(*shm);
    int32_t ret = -1;
    reply.ReadInt32(ret);
    return ret;
}

int ProcessManagerProxy::CreateWindow(const WindowParam& param)
{
    SharedMemory* shm = channel_.GetShm();
    if (!shm) return -1;
    shm->ResetReadWrite();
    IpcParcel parcel(*shm);
    parcel.WriteInt32(static_cast<int32_t>(COMMAND_CREATE_WINDOW));
    param.ToParcel(parcel);

    if (!channel_.SendRequest()) return -1;
    if (!channel_.WaitForReply()) return -1;

    shm->ResetReadWrite();
    IpcParcel reply(*shm);
    int32_t ret = -1;
    reply.ReadInt32(ret);
    return ret;
}

int ProcessManagerProxy::SetWindowBounds(int32_t widget_id, const WindowRect& rect)
{
    SharedMemory* shm = channel_.GetShm();
    if (!shm) return -1;
    shm->ResetReadWrite();
    IpcParcel parcel(*shm);
    parcel.WriteInt32(static_cast<int32_t>(COMMAND_SET_BOUNDS));
    parcel.WriteInt32(widget_id);
    rect.ToParcel(parcel);

    if (!channel_.SendRequest()) return -1;
    if (!channel_.WaitForReply()) return -1;

    shm->ResetReadWrite();
    IpcParcel reply(*shm);
    int32_t ret = -1;
    reply.ReadInt32(ret);
    return ret;
}

int ProcessManagerProxy::SetWindowVisible(int32_t widget_id, bool visible)
{
    SharedMemory* shm = channel_.GetShm();
    if (!shm) return -1;
    shm->ResetReadWrite();
    IpcParcel parcel(*shm);
    parcel.WriteInt32(static_cast<int32_t>(COMMAND_SET_VISIBLE));
    parcel.WriteInt32(widget_id);
    parcel.WriteBool(visible);

    if (!channel_.SendRequest()) return -1;
    if (!channel_.WaitForReply()) return -1;

    shm->ResetReadWrite();
    IpcParcel reply(*shm);
    int32_t ret = -1;
    reply.ReadInt32(ret);
    return ret;
}

int ProcessManagerProxy::UpdateWindow(int32_t widget_id, const WindowParam& param)
{
    SharedMemory* shm = channel_.GetShm();
    if (!shm) return -1;
    shm->ResetReadWrite();
    IpcParcel parcel(*shm);
    parcel.WriteInt32(static_cast<int32_t>(COMMAND_UPDATE_WINDOW));
    parcel.WriteInt32(widget_id);
    param.ToParcel(parcel);

    if (!channel_.SendRequest()) return -1;
    if (!channel_.WaitForReply()) return -1;

    shm->ResetReadWrite();
    IpcParcel reply(*shm);
    int32_t ret = -1;
    reply.ReadInt32(ret);
    return ret;
}

int ProcessManagerProxy::CloseWindow(int32_t widget_id)
{
    SharedMemory* shm = channel_.GetShm();
    if (!shm) return -1;
    shm->ResetReadWrite();
    IpcParcel parcel(*shm);
    parcel.WriteInt32(static_cast<int32_t>(COMMAND_CLOSE_WINDOW));
    parcel.WriteInt32(widget_id);

    if (!channel_.SendRequest()) return -1;
    if (!channel_.WaitForReply()) return -1;

    shm->ResetReadWrite();
    IpcParcel reply(*shm);
    int32_t ret = -1;
    reply.ReadInt32(ret);
    return ret;
}

int ProcessManagerProxy::GetDisplay(DisplayParam& param)
{
    SharedMemory* shm = channel_.GetShm();
    if (!shm) return -1;
    shm->ResetReadWrite();
    IpcParcel parcel(*shm);
    parcel.WriteInt32(static_cast<int32_t>(COMMAND_GET_DISPLAY));

    if (!channel_.SendRequest()) return -1;
    if (!channel_.WaitForReply()) return -1;

    shm->ResetReadWrite();
    IpcParcel reply(*shm);
    int32_t ret = -1;
    reply.ReadInt32(ret);
    if (ret == 0) {
        param = DisplayParam::FromParcel(reply);
    }
    return ret;
}

int ProcessManagerProxy::OpenUrl(const std::string& url)
{
    SharedMemory* shm = channel_.GetShm();
    if (!shm) return -1;
    shm->ResetReadWrite();
    IpcParcel parcel(*shm);
    parcel.WriteInt32(static_cast<int32_t>(COMMAND_OPEN_URL));
    parcel.WriteString(url);

    if (!channel_.SendRequest()) return -1;
    if (!channel_.WaitForReply()) return -1;

    shm->ResetReadWrite();
    IpcParcel reply(*shm);
    int32_t ret = -1;
    reply.ReadInt32(ret);
    return ret;
}

int ProcessManagerProxy::SetWindowTitle(int32_t widget_id, const std::string& title)
{
    SharedMemory* shm = channel_.GetShm();
    if (!shm) return -1;
    shm->ResetReadWrite();
    IpcParcel parcel(*shm);
    parcel.WriteInt32(static_cast<int32_t>(COMMAND_SET_WINDOW_TITLE));
    parcel.WriteInt32(widget_id);
    parcel.WriteString(title);

    if (!channel_.SendRequest()) return -1;
    if (!channel_.WaitForReply()) return -1;

    shm->ResetReadWrite();
    IpcParcel reply(*shm);
    int32_t ret = -1;
    reply.ReadInt32(ret);
    return ret;
}

int ProcessManagerProxy::GetFdIdsRemap(std::vector<std::pair<int, int>>& fds)
{
    SharedMemory* shm = channel_.GetShm();
    if (!shm) return -1;
    shm->ResetReadWrite();
    IpcParcel parcel(*shm);
    parcel.WriteInt32(static_cast<int32_t>(COMMAND_GET_FD_IDS_REMAP));

    if (!channel_.SendRequest()) return -1;
    if (!channel_.WaitForReply()) return -1;

    shm->ResetReadWrite();
    IpcParcel reply(*shm);
    int32_t ret = -1;
    reply.ReadInt32(ret);
    if (ret != 0) return ret;
    int32_t count = 0;
    reply.ReadInt32(count);
    fds.clear();
    for (int32_t i = 0; i < count; i++) {
        FileDescriptor desc = FileDescriptor::FromParcel(reply);
        fds.push_back({desc.fd, desc.remap});
    }
    return 0;
}

int ProcessManagerProxy::GetPid(int32_t& pid)
{
    SharedMemory* shm = channel_.GetShm();
    if (!shm) return -1;
    shm->ResetReadWrite();
    IpcParcel parcel(*shm);
    parcel.WriteInt32(static_cast<int32_t>(COMMAND_GET_PID));

    if (!channel_.SendRequest()) return -1;
    if (!channel_.WaitForReply()) return -1;

    shm->ResetReadWrite();
    IpcParcel reply(*shm);
    int32_t ret = -1;
    reply.ReadInt32(ret);
    if (ret != 0) return ret;
    reply.ReadInt32(pid);
    return 0;
}
ProcessManagerStub::ProcessManagerStub() {}

int ProcessManagerStub::OnRemoteRequest(uint32_t code, IpcParcel& data,
                                         IpcParcel& reply,
                                         const std::vector<int>& fds)
{
    LOGI("ProcessManagerStub::OnRemoteRequest code=%{public}u", code);
    int32_t ret = 0;
    switch (code) {
        case COMMAND_CREATE_PROCESS: {
            int32_t cmd_count = 0;
            data.ReadInt32(cmd_count);
            std::vector<std::string> commands;
            for (int32_t i = 0; i < cmd_count; i++) {
                std::string cmd;
                data.ReadString(cmd);
                commands.push_back(cmd);
            }
            int32_t fd_count = 0;
            data.ReadInt32(fd_count);
            std::vector<int32_t> remap_ids;
            for (int32_t i = 0; i < fd_count; i++) {
                int32_t remap = 0;
                data.ReadInt32(remap);
                remap_ids.push_back(remap);
            }
            std::vector<std::pair<int, int>> mapped_fds;
            for (size_t i = 0; i < fds.size() && i < remap_ids.size(); i++) {
                mapped_fds.push_back({fds[i], remap_ids[i]});
            }
            LOGI("ProcessManagerStub::CreateProcess %{public}zu commands, "
                 "%{public}zu fds", commands.size(), mapped_fds.size());
            ret = IpcHelper::CreateProcess(commands, mapped_fds);
            reply.WriteInt32(ret);
            break;
        }
        case COMMAND_KILL_PROCESS: {
            int32_t pid = 0;
            data.ReadInt32(pid);
            ret = IpcHelper::KillProcess(pid);
            reply.WriteInt32(ret);
            break;
        }
        case COMMAND_CREATE_WINDOW: {
            WindowParam param = WindowParam::FromParcel(data);
            ret = IpcHelper::CreateWindow(param);
            reply.WriteInt32(ret);
            break;
        }
        case COMMAND_SET_BOUNDS: {
            int32_t widget_id = 0;
            data.ReadInt32(widget_id);
            WindowRect rect = WindowRect::FromParcel(data);
            ret = IpcHelper::SetWindowBounds(widget_id, rect);
            reply.WriteInt32(ret);
            break;
        }
        case COMMAND_SET_VISIBLE: {
            int32_t widget_id = 0;
            data.ReadInt32(widget_id);
            bool visible = false;
            data.ReadBool(visible);
            ret = IpcHelper::SetWindowVisible(widget_id, visible);
            reply.WriteInt32(ret);
            break;
        }
        case COMMAND_UPDATE_WINDOW: {
            int32_t widget_id = 0;
            data.ReadInt32(widget_id);
            WindowParam param = WindowParam::FromParcel(data);
            ret = IpcHelper::UpdateWindow(widget_id, param);
            reply.WriteInt32(ret);
            break;
        }
        case COMMAND_CLOSE_WINDOW: {
            int32_t widget_id = 0;
            data.ReadInt32(widget_id);
            ret = IpcHelper::CloseWindow(widget_id);
            reply.WriteInt32(ret);
            break;
        }
        case COMMAND_GET_DISPLAY: {
            DisplayParam param;
            ret = IpcHelper::GetDisplay(param);
            reply.WriteInt32(ret);
            if (ret == 0) param.ToParcel(reply);
            break;
        }
        case COMMAND_OPEN_URL: {
            std::string url;
            data.ReadString(url);
            ret = IpcHelper::OpenUrl(url);
            reply.WriteInt32(ret);
            break;
        }
        case COMMAND_SET_WINDOW_TITLE: {
            int32_t widget_id = 0;
            data.ReadInt32(widget_id);
            std::string title;
            data.ReadString(title);
            ret = IpcHelper::SetWindowTitle(widget_id, title);
            reply.WriteInt32(ret);
            break;
        }
        case COMMAND_GET_FD_IDS_REMAP: {
            auto fd_remap = multiprocess::AppSpawnCommunication::GetFdIdsRemap();
            ret = 0;
            reply.WriteInt32(ret);
            reply.WriteInt32(static_cast<int32_t>(fd_remap.size()));
            for (const auto& [rawfd, remap_fd] : fd_remap) {
                FileDescriptor desc = {rawfd, remap_fd};
                desc.ToParcel(reply);
            }
            return 0;
        }
        case COMMAND_GET_PID: {
            int32_t pid = getpid();
            ret = 0;
            reply.WriteInt32(ret);
            reply.WriteInt32(pid);
            LOGI("ProcessManagerStub::GetPid=%{public}d", pid);
            break;
        }
        default:
            LOGE("ProcessManagerStub::OnRemoteRequest unknown code %{public}u",
                 code);
            ret = -1;
            reply.WriteInt32(ret);
            break;
    }
    return ret;
}

ChildProcessProxy::ChildProcessProxy(IpcChannel& channel) : channel_(channel) {}

int ChildProcessProxy::SetProcessInfo(
    const std::vector<std::string>& commands,
    const std::vector<std::pair<int, int>>& fds)
{
    SharedMemory* shm = channel_.GetShm();
    if (!shm) return -1;
    shm->ResetReadWrite();
    IpcParcel parcel(*shm);
    parcel.WriteInt32(static_cast<int32_t>(COMMAND_SET_PROCESS_INFO));
    parcel.WriteInt32(static_cast<int32_t>(commands.size()));
    for (const auto& cmd : commands) parcel.WriteString(cmd);
    parcel.WriteInt32(static_cast<int32_t>(fds.size()));
    for (const auto& [fd, remap] : fds) {
        parcel.WriteInt32(remap);
    }

    std::vector<int> actual_fds;
    for (const auto& [fd, remap] : fds) {
        actual_fds.push_back(fd);
    }

    if (!channel_.SendRequestWithFds(actual_fds)) return -1;
    if (!channel_.WaitForReply()) return -1;

    shm->ResetReadWrite();
    IpcParcel reply(*shm);
    int32_t ret = -1;
    reply.ReadInt32(ret);
    return ret;
}

int ChildProcessProxy::SetWindowInfo(int32_t widget_id, const WindowAttr& attr)
{
    SharedMemory* shm = channel_.GetShm();
    if (!shm) return -1;
    shm->ResetReadWrite();
    IpcParcel parcel(*shm);
    parcel.WriteInt32(static_cast<int32_t>(COMMAND_SET_WINDOW_INFO));
    parcel.WriteInt32(widget_id);
    attr.ToParcel(parcel);

    if (!channel_.SendRequest()) return -1;
    if (!channel_.WaitForReply()) return -1;

    shm->ResetReadWrite();
    IpcParcel reply(*shm);
    int32_t ret = -1;
    reply.ReadInt32(ret);
    return ret;
}

int ChildProcessProxy::SendInputEvent(const std::vector<int8_t>& event_data)
{
    SharedMemory* shm = channel_.GetShm();
    if (!shm) return -1;
    shm->ResetReadWrite();
    IpcParcel parcel(*shm);
    parcel.WriteInt32(static_cast<int32_t>(COMMAND_SEND_INPUT_EVENT));
    parcel.WriteInt32(static_cast<int32_t>(event_data.size()));
    parcel.WriteRaw(event_data.data(), static_cast<uint32_t>(event_data.size()));

    if (!channel_.SendRequest()) return -1;
    if (!channel_.WaitForReply()) return -1;

    shm->ResetReadWrite();
    IpcParcel reply(*shm);
    int32_t ret = -1;
    reply.ReadInt32(ret);
    return ret;
}

ChildProcessStub::ChildProcessStub() {}

int ChildProcessStub::OnRemoteRequest(uint32_t code, IpcParcel& data,
                                       IpcParcel& reply,
                                       const std::vector<int>& fds)
{
    LOGI("ChildProcessStub::OnRemoteRequest code=%{public}u", code);
    int32_t ret = 0;
    switch (code) {
        case COMMAND_SET_PROCESS_INFO: {
            int32_t cmd_count = 0;
            data.ReadInt32(cmd_count);
            std::vector<std::string> commands;
            for (int32_t i = 0; i < cmd_count; i++) {
                std::string cmd;
                data.ReadString(cmd);
                commands.push_back(cmd);
            }
            int32_t fd_count = 0;
            data.ReadInt32(fd_count);
            std::vector<int32_t> remap_ids;
            for (int32_t i = 0; i < fd_count; i++) {
                int32_t remap = 0;
                data.ReadInt32(remap);
                remap_ids.push_back(remap);
            }
            std::vector<std::pair<int, int>> mapped_fds;
            for (size_t i = 0; i < fds.size() && i < remap_ids.size(); i++) {
                mapped_fds.push_back({fds[i], remap_ids[i]});
            }
            ChildProcess::Instance().SetFdIdsRemap(mapped_fds);
            LOGI("ChildProcessStub SetProcessInfo: %{public}zu commands, "
                 "%{public}zu fds", commands.size(), mapped_fds.size());
            reply.WriteInt32(0);
            break;
        }
        case COMMAND_ECHO: {
            std::string str;
            data.ReadString(str);
            reply.WriteInt32(0);
            break;
        }
        case COMMAND_SET_WINDOW_INFO: {
            int32_t widget_id = 0;
            data.ReadInt32(widget_id);
            WindowAttr attr = WindowAttr::FromParcel(data);
            LOGI("ChildProcessStub SetWindowInfo widget_id=%{public}d",
                 widget_id);
            reply.WriteInt32(0);
            break;
        }
        case COMMAND_SEND_INPUT_EVENT: {
            int32_t data_size = 0;
            data.ReadInt32(data_size);
            std::vector<int8_t> event_data(data_size);
            uint32_t len = static_cast<uint32_t>(data_size);
            data.ReadRaw(event_data.data(), len);
            LOGI("ChildProcessStub SendInputEvent size=%{public}d",
                 data_size);
            reply.WriteInt32(0);
            break;
        }
        default:
            LOGE("ChildProcessStub::OnRemoteRequest unknown code %{public}u",
                 code);
            ret = -1;
            reply.WriteInt32(ret);
            break;
    }
    return ret;
}

}  // namespace ohos::adapter::new_ipc

// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/new_ipc/ipc_proxy_stub.h"

#include <unistd.h>

#include "ohos/adapter/common/logging.h"
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

    /* 应答在 sync_request_end_ 之后，读游标已由 WaitForReply 拨到位 */
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

int ProcessManagerProxy::CreateClientSurface(ClientSurfaceParam& param,
                                             uint64_t& surface_id)
{
    SharedMemory* shm = channel_.GetShm();
    if (!shm) return -1;
    shm->ResetReadWrite();
    IpcParcel parcel(*shm);
    parcel.WriteInt32(static_cast<int32_t>(COMMAND_CREATE_CLIENT_SURFACE));
    param.ToParcel(parcel);

    if (!channel_.SendRequest()) return -1;
    if (!channel_.WaitForReply()) return -1;

    IpcParcel reply(*shm);
    int32_t ret = -1;
    reply.ReadInt32(ret);
    if (ret != 0) return ret;
    reply.ReadInt32(param.client_widget_id);
    reply.ReadInt64(reinterpret_cast<int64_t&>(surface_id));
    return 0;
}

int ProcessManagerProxy::SetClientSurfaceBounds(int32_t client_widget_id,
                                                int32_t off_x, int32_t off_y,
                                                int32_t width, int32_t height)
{
    SharedMemory* shm = channel_.GetShm();
    if (!shm) return -1;
    shm->ResetReadWrite();
    IpcParcel parcel(*shm);
    parcel.WriteInt32(static_cast<int32_t>(COMMAND_SET_CLIENT_SURFACE_BOUNDS));
    parcel.WriteInt32(client_widget_id);
    parcel.WriteInt32(off_x);
    parcel.WriteInt32(off_y);
    parcel.WriteInt32(width);
    parcel.WriteInt32(height);

    if (!channel_.SendRequest()) return -1;
    if (!channel_.WaitForReply()) return -1;

    IpcParcel reply(*shm);
    int32_t ret = -1;
    reply.ReadInt32(ret);
    return ret;
}

int ProcessManagerProxy::DestroyClientSurface(int32_t client_widget_id)
{
    SharedMemory* shm = channel_.GetShm();
    if (!shm) return -1;
    shm->ResetReadWrite();
    IpcParcel parcel(*shm);
    parcel.WriteInt32(static_cast<int32_t>(COMMAND_DESTROY_CLIENT_SURFACE));
    parcel.WriteInt32(client_widget_id);

    if (!channel_.SendRequest()) return -1;
    if (!channel_.WaitForReply()) return -1;

    IpcParcel reply(*shm);
    int32_t ret = -1;
    reply.ReadInt32(ret);
    return ret;
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

    IpcParcel reply(*shm);
    int32_t ret = -1;
    reply.ReadInt32(ret);
    if (ret != 0) return ret;
    reply.ReadInt32(pid);
    return 0;
}

int ProcessManagerProxy::AttachRemoteGdi(RemoteGdiParam& param, int fd)
{
    SharedMemory* shm = channel_.GetShm();
    if (!shm) return -1;
    shm->ResetReadWrite();
    IpcParcel parcel(*shm);
    parcel.WriteInt32(static_cast<int32_t>(COMMAND_CREATE_REMOTE_GDI));
    param.ToParcel(parcel);

    if (!channel_.SendRequestWithFds({fd})) return -1;
    if (!channel_.WaitForReply()) return -1;

    IpcParcel reply(*shm);
    int32_t ret = -1;
    reply.ReadInt32(ret);
    if (ret != 0) return ret;
    reply.ReadInt32(param.client_widget_id);
    return 0;
}

int ProcessManagerProxy::RemoteGdiFlush(int32_t client_widget_id, int32_t x,
                                        int32_t y, int32_t w, int32_t h)
{
    SharedMemory* shm = channel_.GetShm();
    if (!shm) return -1;
    shm->ResetReadWrite();
    IpcParcel parcel(*shm);
    parcel.WriteInt32(static_cast<int32_t>(COMMAND_REMOTE_GDI_FLUSH));
    parcel.WriteInt32(client_widget_id);
    parcel.WriteInt32(x);
    parcel.WriteInt32(y);
    parcel.WriteInt32(w);
    parcel.WriteInt32(h);

    if (!channel_.SendRequest()) return -1;
    if (!channel_.WaitForReply()) return -1;

    IpcParcel reply(*shm);
    int32_t ret = -1;
    reply.ReadInt32(ret);
    return ret;
}

int ProcessManagerProxy::DestroyRemoteGdi(int32_t client_widget_id)
{
    SharedMemory* shm = channel_.GetShm();
    if (!shm) return -1;
    shm->ResetReadWrite();
    IpcParcel parcel(*shm);
    parcel.WriteInt32(static_cast<int32_t>(COMMAND_DESTROY_REMOTE_GDI));
    parcel.WriteInt32(client_widget_id);

    if (!channel_.SendRequest()) return -1;
    if (!channel_.WaitForReply()) return -1;

    IpcParcel reply(*shm);
    int32_t ret = -1;
    reply.ReadInt32(ret);
    return ret;
}

int ProcessManagerProxy::UpdateRemoteGdiBounds(int32_t client_widget_id,
                                               int32_t off_x, int32_t off_y)
{
    SharedMemory* shm = channel_.GetShm();
    if (!shm) return -1;
    shm->ResetReadWrite();
    IpcParcel parcel(*shm);
    parcel.WriteInt32(static_cast<int32_t>(COMMAND_UPDATE_REMOTE_GDI_BOUNDS));
    parcel.WriteInt32(client_widget_id);
    parcel.WriteInt32(off_x);
    parcel.WriteInt32(off_y);

    if (!channel_.SendRequest()) return -1;
    if (!channel_.WaitForReply()) return -1;

    IpcParcel reply(*shm);
    int32_t ret = -1;
    reply.ReadInt32(ret);
    return ret;
}

ProcessManagerStub::ProcessManagerStub() {}

int ProcessManagerStub::OnRemoteRequest(uint32_t code, IpcParcel& data,
                                         IpcParcel& reply,
                                         const std::vector<int>& fds)
{
    LOGI("ProcessManagerStub::OnRemoteRequest code=%{public}u", code);
    int32_t ret = 0;
    switch (code) {
        case COMMAND_CREATE_CLIENT_SURFACE: {
            ClientSurfaceParam param = ClientSurfaceParam::FromParcel(data);
            uint64_t surface_id = 0;
            ret = IpcHelper::CreateClientSurface(param, surface_id);
            reply.WriteInt32(ret);
            if (ret == 0) {
                reply.WriteInt32(param.client_widget_id);
                reply.WriteInt64(static_cast<int64_t>(surface_id));
            }
            break;
        }
        case COMMAND_SET_CLIENT_SURFACE_BOUNDS: {
            int32_t client_widget_id = 0;
            int32_t off_x = 0;
            int32_t off_y = 0;
            int32_t width = 0;
            int32_t height = 0;
            data.ReadInt32(client_widget_id);
            data.ReadInt32(off_x);
            data.ReadInt32(off_y);
            data.ReadInt32(width);
            data.ReadInt32(height);
            ret = IpcHelper::SetClientSurfaceBounds(client_widget_id, off_x,
                                                    off_y, width, height);
            reply.WriteInt32(ret);
            break;
        }
        case COMMAND_DESTROY_CLIENT_SURFACE: {
            int32_t client_widget_id = 0;
            data.ReadInt32(client_widget_id);
            ret = IpcHelper::DestroyClientSurface(client_widget_id);
            reply.WriteInt32(ret);
            break;
        }
        case COMMAND_CREATE_REMOTE_GDI: {
            RemoteGdiParam param = RemoteGdiParam::FromParcel(data);
            int fd = fds.empty() ? -1 : fds[0];
            ret = IpcHelper::AttachRemoteGdi(param, fd);
            reply.WriteInt32(ret);
            if (ret == 0) {
                reply.WriteInt32(param.client_widget_id);
            }
            break;
        }
        case COMMAND_REMOTE_GDI_FLUSH: {
            int32_t client_widget_id = 0, x = 0, y = 0, w = 0, h = 0;
            data.ReadInt32(client_widget_id);
            data.ReadInt32(x);
            data.ReadInt32(y);
            data.ReadInt32(w);
            data.ReadInt32(h);
            ret = IpcHelper::RemoteGdiFlush(client_widget_id, x, y, w, h);
            reply.WriteInt32(ret);
            break;
        }
        case COMMAND_DESTROY_REMOTE_GDI: {
            int32_t client_widget_id = 0;
            data.ReadInt32(client_widget_id);
            ret = IpcHelper::DestroyRemoteGdi(client_widget_id);
            reply.WriteInt32(ret);
            break;
        }
        case COMMAND_UPDATE_REMOTE_GDI_BOUNDS: {
            int32_t client_widget_id = 0, off_x = 0, off_y = 0;
            data.ReadInt32(client_widget_id);
            data.ReadInt32(off_x);
            data.ReadInt32(off_y);
            ret = IpcHelper::UpdateRemoteGdiBounds(client_widget_id, off_x, off_y);
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
        case COMMAND_GET_PID: {
            int32_t pid = getpid();
            ret = 0;
            reply.WriteInt32(ret);
            reply.WriteInt32(pid);
            LOGI("ProcessManagerStub::GetPid=%{public}d", pid);
            break;
        }
        case COMMAND_ECHO: {
            std::string str;
            data.ReadString(str);
            reply.WriteInt32(0);
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

}  // namespace ohos::adapter::new_ipc

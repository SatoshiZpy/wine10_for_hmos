// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/new_ipc/ipc_channel_manager.h"

#include <cerrno>
#include <unistd.h>

#include "ohos/adapter/common/logging.h"

namespace ohos::adapter::new_ipc {

IpcChannelManager::IpcChannelManager() = default;

IpcChannelManager::~IpcChannelManager()
{
    StopListening();
    CloseAll();
}

bool IpcChannelManager::StartServer(const std::string& socket_path)
{
    MutexLock lock(mutex_);
    if (!server_socket_.Listen(socket_path)) {
        LOGE("IpcChannelManager::StartServer Listen failed");
        return false;
    }
    server_socket_path_ = socket_path;
    LOGI("IpcChannelManager::StartServer path=%{public}s", socket_path.c_str());
    return true;
}

int32_t IpcChannelManager::AcceptNewChannel()
{
    MutexLock lock(mutex_);
    int client_fd = server_socket_.Accept();
    if (client_fd < 0) {
        LOGE("IpcChannelManager::AcceptNewChannel Accept failed");
        return -1;
    }

    int32_t channel_id = AllocateChannelId();
    auto channel = std::make_shared<IpcChannel>();
    if (!channel->ConnectWithFd(client_fd)) {
        LOGE("IpcChannelManager::AcceptNewChannel ConnectWithFd failed");
        close(client_fd);
        return -1;
    }

    channels_[channel_id] = channel;
    LOGI("IpcChannelManager::AcceptNewChannel channel_id=%{public}d, fd=%{public}d",
         channel_id, client_fd);
    return channel_id;
}

bool IpcChannelManager::ConnectToServer(const std::string& socket_path)
{
    MutexLock lock(mutex_);
    auto channel = std::make_shared<IpcChannel>();
    if (!channel->ConnectToServer(socket_path)) {
        LOGE("IpcChannelManager::ConnectToServer Connect failed");
        return false;
    }

    int32_t channel_id = IPC_CHANNEL_ID_PARENT;
    channels_[channel_id] = channel;
    LOGI("IpcChannelManager::ConnectToServer channel_id=%{public}d (parent)", channel_id);
    return true;
}

int32_t IpcChannelManager::GetChannelCount()
{
    MutexLock lock(mutex_);
    return static_cast<int32_t>(channels_.size());
}

std::vector<int32_t> IpcChannelManager::GetChannelIds()
{
    MutexLock lock(mutex_);
    std::vector<int32_t> ids;
    for (const auto& [id, channel] : channels_) {
        ids.push_back(id);
    }
    return ids;
}

IpcChannel* IpcChannelManager::GetChannel(int32_t channel_id)
{
    MutexLock lock(mutex_);
    auto it = channels_.find(channel_id);
    if (it == channels_.end()) return nullptr;
    return it->second.get();
}

bool IpcChannelManager::SendTo(int32_t channel_id, const SharedMemory& shm)
{
    MutexLock lock(mutex_);
    auto it = channels_.find(channel_id);
    if (it == channels_.end()) {
        LOGE("IpcChannelManager::SendTo channel_id=%{public}d not found", channel_id);
        return false;
    }
    auto result = it->second->SendShmFd(shm);
    return result != nullptr;
}

bool IpcChannelManager::Broadcast(const SharedMemory& shm)
{
    MutexLock lock(mutex_);
    bool all_ok = true;
    for (const auto& [id, channel] : channels_) {
        auto result = channel->SendShmFd(shm);
        if (!result) all_ok = false;
    }
    return all_ok;
}

bool IpcChannelManager::SendControlTo(int32_t channel_id, IpcMessageType type,
                                       const void* data, uint32_t length)
{
    MutexLock lock(mutex_);
    auto it = channels_.find(channel_id);
    if (it == channels_.end()) return false;
    return it->second->SendControlMessage(type, data, length);
}

void IpcChannelManager::RemoveChannel(int32_t channel_id)
{
    MutexLock lock(mutex_);
    auto it = channels_.find(channel_id);
    if (it != channels_.end()) {
        it->second->Close();
        channels_.erase(it);
        LOGI("IpcChannelManager::RemoveChannel channel_id=%{public}d", channel_id);
    }
}

void IpcChannelManager::CloseAll()
{
    MutexLock lock(mutex_);
    for (auto& [id, channel] : channels_) {
        channel->StopListening();
        channel->Close();
    }
    channels_.clear();
    server_socket_.Close();
    LOGI("IpcChannelManager::CloseAll");
}

void IpcChannelManager::SetCallback(IpcManagerCallback callback)
{
    MutexLock lock(mutex_);
    callback_ = callback;
}

bool IpcChannelManager::StartListening()
{
    if (listening_) {
        LOGW("IpcChannelManager::StartListening already listening");
        return true;
    }

    MutexLock lock(mutex_);

    for (auto& entry : channels_) {
        int32_t cid = entry.first;
        entry.second->SetCallback([this, cid](const IpcMessage& msg) {
            DispatchMessage(cid, msg);
        });
        entry.second->StartListening();
    }

    listening_ = true;

    if (!server_socket_path_.empty()) {
        accepting_ = true;
        accept_thread_ = std::thread(&IpcChannelManager::AcceptLoop, this);
    }

    LOGI("IpcChannelManager::StartListening started");
    return true;
}

void IpcChannelManager::StopListening()
{
    accepting_ = false;
    listening_ = false;

    MutexLock lock(mutex_);
    for (auto& [id, channel] : channels_) {
        channel->StopListening();
    }

    if (accept_thread_.joinable()) {
        accept_thread_.join();
    }
    LOGI("IpcChannelManager::StopListening stopped");
}

int32_t IpcChannelManager::GetParentChannelId() const
{
    return IPC_CHANNEL_ID_PARENT;
}

int32_t IpcChannelManager::AllocateChannelId()
{
    return next_channel_id_++;
}

void IpcChannelManager::AcceptLoop()
{
    LOGI("IpcChannelManager::AcceptLoop enter");
    while (accepting_) {
        int client_fd = server_socket_.Accept();
        if (client_fd < 0) {
            if (!accepting_) break;
            LOGE("IpcChannelManager::AcceptLoop Accept failed, errno=%{public}d", errno);
            continue;
        }

        int32_t channel_id;
        std::shared_ptr<IpcChannel> channel;
        {
            MutexLock lock(mutex_);
            channel_id = AllocateChannelId();
            channel = std::make_shared<IpcChannel>();
            if (!channel->ConnectWithFd(client_fd)) {
                LOGE("IpcChannelManager::AcceptLoop ConnectWithFd failed");
                close(client_fd);
                continue;
            }

            channel->SetCallback([this, channel_id](const IpcMessage& msg) {
                DispatchMessage(channel_id, msg);
            });
            channel->StartListening();
            channels_[channel_id] = channel;
        }

        LOGI("IpcChannelManager::AcceptLoop new channel_id=%{public}d, fd=%{public}d",
             channel_id, client_fd);

        IpcManagerMessage mgr_msg;
        mgr_msg.channel_id = channel_id;
        mgr_msg.msg.type = IpcMessageType::CONTROL;
        DispatchMessage(channel_id, mgr_msg.msg);
    }
    LOGI("IpcChannelManager::AcceptLoop exit");
}

void IpcChannelManager::ListenLoopForChannel(int32_t channel_id)
{
    LOGI("IpcChannelManager::ListenLoopForChannel channel_id=%{public}d", channel_id);
}

void IpcChannelManager::DispatchMessage(int32_t channel_id, const IpcMessage& msg)
{
    if (callback_) {
        IpcManagerMessage mgr_msg;
        mgr_msg.channel_id = channel_id;
        mgr_msg.msg = msg;
        callback_(mgr_msg);
    } else {
        LOGW("IpcChannelManager::DispatchMessage no callback, channel_id=%{public}d, type=%{public}d dropped",
             channel_id, static_cast<int>(msg.type));
    }
}

}  // namespace ohos::adapter::new_ipc
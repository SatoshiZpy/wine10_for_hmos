// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/new_ipc/ipc_channel.h"

#include <cerrno>
#include <cstring>
#include <unistd.h>

#include "ohos/adapter/common/logging.h"

namespace ohos::adapter::new_ipc {

IpcChannel::IpcChannel() = default;

IpcChannel::~IpcChannel()
{
    StopListening();
    Close();
}

bool IpcChannel::ConnectToServer(const std::string& socket_path)
{
    MutexLock lock(mutex_);
    if (state_ != IpcChannelState::CLOSED) {
        LOGE("IpcChannel::ConnectToServer channel not closed");
        return false;
    }

    if (!socket_.Connect(socket_path)) {
        LOGE("IpcChannel::ConnectToServer Connect failed");
        state_ = IpcChannelState::ERROR;
        return false;
    }

    role_ = IpcChannelRole::CLIENT;
    state_ = IpcChannelState::CONNECTED;
    LOGI("IpcChannel::ConnectToServer connected");
    return true;
}

bool IpcChannel::ConnectWithFd(int fd)
{
    MutexLock lock(mutex_);
    if (state_ != IpcChannelState::CLOSED) {
        LOGE("IpcChannel::ConnectWithFd channel not closed");
        return false;
    }
    if (fd < 0) {
        LOGE("IpcChannel::ConnectWithFd invalid fd %{public}d", fd);
        return false;
    }

    if (!socket_.AttachFd(fd)) {
        LOGE("IpcChannel::ConnectWithFd AttachFd failed");
        state_ = IpcChannelState::ERROR;
        return false;
    }

    role_ = IpcChannelRole::SERVER;
    state_ = IpcChannelState::CONNECTED;
    LOGI("IpcChannel::ConnectWithFd connected via fd=%{public}d", fd);
    return true;
}

void IpcChannel::Close()
{
    MutexLock lock(mutex_);
    shm_.reset();
    socket_.Close();
    state_ = IpcChannelState::CLOSED;
    shm_exchanged_ = false;
    {
        std::lock_guard<std::mutex> lg(reply_std_mutex_);
        reply_ready_ = false;
    }
    reply_cv_.notify_one();
    LOGI("IpcChannel::Close");
}

bool IpcChannel::ExchangeShmFd()
{
    MutexLock lock(mutex_);
    if (!socket_.IsConnected()) {
        LOGE("IpcChannel::ExchangeShmFd not connected");
        return false;
    }
    if (shm_exchanged_) {
        LOGW("IpcChannel::ExchangeShmFd already exchanged");
        return true;
    }

    if (role_ == IpcChannelRole::SERVER) {
        shm_ = std::make_shared<SharedMemory>();
        if (!shm_->Create(SHARED_MEMORY_DEFAULT_SIZE)) {
            LOGE("IpcChannel::ExchangeShmFd create shm failed");
            return false;
        }
        if (!SendShmFdInternal(*shm_, IpcMessageType::SHM_FD)) {
            LOGE("IpcChannel::ExchangeShmFd send shm fd failed");
            return false;
        }
    } else {
        shm_ = ReceiveShmFdInternal(IpcMessageType::SHM_FD);
        if (!shm_) {
            LOGE("IpcChannel::ExchangeShmFd receive shm fd failed");
            return false;
        }
    }

    shm_exchanged_ = true;
    LOGI("IpcChannel::ExchangeShmFd done: role=%{public}d, "
         "shm_fd=%{public}d", static_cast<int>(role_), shm_->GetFd());
    return true;
}

bool IpcChannel::SendRequest()
{
    MutexLock lock(mutex_);
    if (!shm_exchanged_) {
        LOGE("IpcChannel::SendRequest shm not exchanged");
        return false;
    }
    {
        std::lock_guard<std::mutex> lg(reply_std_mutex_);
        reply_ready_ = false;
    }
    if (shm_) sync_request_end_ = shm_->GetWriteOffset();
    if (!socket_.SendControlMessage(IpcMessageType::SYNC_REQUEST, nullptr, 0)) {
        LOGE("IpcChannel::SendRequest send notification failed");
        return false;
    }
    LOGI("IpcChannel::SendRequest sent");
    return true;
}

bool IpcChannel::SendRequestWithFds(const std::vector<int>& fds)
{
    MutexLock lock(mutex_);
    if (!shm_exchanged_) {
        LOGE("IpcChannel::SendRequestWithFds shm not exchanged");
        return false;
    }
    if (fds.empty()) {
        LOGE("IpcChannel::SendRequestWithFds empty fds");
        return false;
    }
    {
        std::lock_guard<std::mutex> lg(reply_std_mutex_);
        reply_ready_ = false;
    }
    if (shm_) sync_request_end_ = shm_->GetWriteOffset();
    if (!socket_.SendControlMessage(
            IpcMessageType::SYNC_REQUEST_FDS, nullptr, 0)) {
        LOGE("IpcChannel::SendRequestWithFds send control failed");
        return false;
    }
    if (!socket_.SendFds(fds)) {
        LOGE("IpcChannel::SendRequestWithFds send fds failed");
        return false;
    }
    LOGI("IpcChannel::SendRequestWithFds sent %{public}zu fds",
         fds.size());
    return true;
}

bool IpcChannel::WaitForReply()
{
    std::unique_lock<std::mutex> lock(reply_std_mutex_);
    if (!reply_cv_.wait_for(lock, std::chrono::seconds(5),
                            [this] { return reply_ready_; })) {
        LOGE("IpcChannel::WaitForReply timeout");
        return false;
    }
    reply_ready_ = false;
    if (shm_) shm_->SetReadOffset(sync_request_end_);
    LOGI("IpcChannel::WaitForReply received");
    return true;
}

bool IpcChannel::SendAck()
{
    MutexLock lock(mutex_);
    return socket_.SendControlMessage(IpcMessageType::ACK, nullptr, 0);
}

bool IpcChannel::SendReply()
{
    MutexLock lock(mutex_);
    return socket_.SendControlMessage(IpcMessageType::SYNC_REPLY, nullptr, 0);
}

std::shared_ptr<SharedMemory> IpcChannel::SendShmFd(const SharedMemory& shm)
{
    MutexLock lock(mutex_);
    if (!SendShmFdInternal(shm, IpcMessageType::SHM_FD)) return nullptr;
    return ReceiveShmFdInternal(IpcMessageType::SHM_FD);
}

std::shared_ptr<SharedMemory> IpcChannel::ReceiveShmFd()
{
    MutexLock lock(mutex_);
    auto shm = ReceiveShmFdInternal(IpcMessageType::SHM_FD);
    if (shm) shm_ = shm;
    return shm;
}

bool IpcChannel::SendControlMessage(IpcMessageType type,
                                     const void* data,
                                     uint32_t length)
{
    MutexLock lock(mutex_);
    return socket_.SendControlMessage(type, data, length);
}

void IpcChannel::SetCallback(IpcChannelCallback callback)
{
    MutexLock lock(mutex_);
    callback_ = callback;
}

void IpcChannel::SetRequestHandler(IpcChannelCallback handler)
{
    MutexLock lock(mutex_);
    request_handler_ = handler;
}

bool IpcChannel::StartListening()
{
    if (listening_) {
        LOGW("IpcChannel::StartListening already listening");
        return true;
    }
    if (state_ != IpcChannelState::CONNECTED) {
        LOGE("IpcChannel::StartListening not connected");
        return false;
    }

    listening_ = true;
    listen_thread_ = std::thread(&IpcChannel::ListenLoop, this);
    LOGI("IpcChannel::StartListening thread started");
    return true;
}

void IpcChannel::StopListening()
{
    if (!listening_) return;
    listening_ = false;
    {
        std::lock_guard<std::mutex> lg(reply_std_mutex_);
        reply_ready_ = true;
    }
    reply_cv_.notify_one();
    if (listen_thread_.joinable()) {
        listen_thread_.join();
    }
    LOGI("IpcChannel::StopListening thread stopped");
}

void IpcChannel::ListenLoop()
{
    LOGI("IpcChannel::ListenLoop enter");
    while (listening_ && socket_.IsConnected()) {
        IpcMessageType type;
        uint32_t length = 0;
        if (!socket_.RecvControlMessage(type, nullptr, &length)) {
            if (!listening_) break;
            LOGE("IpcChannel::ListenLoop recv failed, errno=%{public}d", errno);
            break;
        }

        IpcMessage msg;
        msg.type = type;

        if (type == IpcMessageType::SHM_FD) {
            std::vector<int> fds;
            if (!socket_.RecvFds(fds, 1)) {
                LOGE("IpcChannel::ListenLoop recv fd failed");
                break;
            }
            auto shm = std::make_shared<SharedMemory>();
            if (!shm->Attach(fds[0])) {
                LOGE("IpcChannel::ListenLoop Attach failed");
                for (int fd : fds) close(fd);
                break;
            }
            close(fds[0]);
            {
                MutexLock lock(mutex_);
                shm_ = shm;
            }
            msg.shm = shm;
            DispatchMessage(msg);
        } else if (type == IpcMessageType::SYNC_REQUEST) {
            LOGI("IpcChannel::ListenLoop received SYNC_REQUEST");
            msg.shm = shm_;
            HandleSyncRequest(msg);
        } else if (type == IpcMessageType::SYNC_REQUEST_FDS) {
            LOGI("IpcChannel::ListenLoop received SYNC_REQUEST_FDS");
            std::vector<int> received_fds;
            char dummy;
            if (socket_.RecvMsg(&dummy, 1, received_fds) < 0) {
                LOGE("IpcChannel::ListenLoop recv fds failed");
                break;
            }
            msg.type = IpcMessageType::SYNC_REQUEST;
            msg.shm = shm_;
            msg.fds = received_fds;
            HandleSyncRequest(msg);
        } else if (type == IpcMessageType::SYNC_REPLY) {
            LOGI("IpcChannel::ListenLoop received SYNC_REPLY");
            {
                std::lock_guard<std::mutex> lg(reply_std_mutex_);
                reply_ready_ = true;
            }
            reply_cv_.notify_one();
            msg.shm = shm_;
            DispatchMessage(msg);
        } else if (type == IpcMessageType::SYNC_REQUEST) {
            LOGI("IpcChannel::ListenLoop received SYNC_REQUEST");
            msg.shm = shm_;
            HandleSyncRequest(msg);
        } else if (type == IpcMessageType::SYNC_REQUEST_FDS) {
            LOGI("IpcChannel::ListenLoop received SYNC_REQUEST_FDS");
            std::vector<int> received_fds;
            char dummy;
            if (socket_.RecvMsg(&dummy, 1, received_fds) < 0) {
                LOGE("IpcChannel::ListenLoop recv fds failed");
                break;
            }
            msg.type = IpcMessageType::SYNC_REQUEST;
            msg.shm = shm_;
            msg.fds = received_fds;
            HandleSyncRequest(msg);
        } else if (type == IpcMessageType::SYNC_REPLY) {
            LOGI("IpcChannel::ListenLoop received SYNC_REPLY");
            {
                std::lock_guard<std::mutex> lg(reply_std_mutex_);
                reply_ready_ = true;
            }
            reply_cv_.notify_one();
            msg.shm = shm_;
            DispatchMessage(msg);
        } else if (type == IpcMessageType::ACK) {
            LOGI("IpcChannel::ListenLoop received ACK");
            DispatchMessage(msg);
        } else if (type == IpcMessageType::DISCONNECT) {
            LOGI("IpcChannel::ListenLoop remote disconnected");
            {
                MutexLock lock(mutex_);
                state_ = IpcChannelState::CLOSED;
            }
            break;
        } else {
            LOGW("IpcChannel::ListenLoop unknown type %{public}d",
                 static_cast<int>(type));
            DispatchMessage(msg);
        }
    }
    LOGI("IpcChannel::ListenLoop exit");
}

void IpcChannel::HandleSyncRequest(const IpcMessage& incoming_msg)
{
    if (request_handler_) {
        request_handler_(incoming_msg);
    } else if (callback_) {
        callback_(incoming_msg);
    } else {
        LOGW("IpcChannel::HandleSyncRequest no handler, SYNC_REQUEST dropped");
    }
}

void IpcChannel::DispatchMessage(const IpcMessage& msg)
{
    if (callback_) {
        callback_(msg);
    } else {
        LOGW("IpcChannel::DispatchMessage no callback, type=%{public}d dropped",
             static_cast<int>(msg.type));
    }
}

bool IpcChannel::SendShmFdInternal(const SharedMemory& shm, IpcMessageType type)
{
    if (!socket_.IsConnected()) {
        LOGE("IpcChannel::SendShmFdInternal not connected");
        return false;
    }
    if (!socket_.SendControlMessage(type, nullptr, 0)) {
        LOGE("IpcChannel::SendShmFdInternal send control failed");
        return false;
    }
    std::vector<int> fds = {shm.GetFd()};
    if (!socket_.SendFds(fds)) {
        LOGE("IpcChannel::SendShmFdInternal send fd failed");
        return false;
    }
    LOGI("IpcChannel::SendShmFdInternal sent fd=%{public}d", shm.GetFd());
    return true;
}

std::shared_ptr<SharedMemory> IpcChannel::ReceiveShmFdInternal(
    IpcMessageType expected_type)
{
    if (!socket_.IsConnected()) {
        LOGE("IpcChannel::ReceiveShmFdInternal not connected");
        return nullptr;
    }
    IpcMessageType type;
    uint32_t length = 0;
    if (!socket_.RecvControlMessage(type, nullptr, &length)) {
        LOGE("IpcChannel::ReceiveShmFdInternal recv control failed");
        return nullptr;
    }
    if (type != expected_type) {
        LOGE("IpcChannel::ReceiveShmFdInternal unexpected type %{public}d",
             static_cast<int>(type));
        return nullptr;
    }
    std::vector<int> fds;
    if (!socket_.RecvFds(fds, 1)) {
        LOGE("IpcChannel::ReceiveShmFdInternal recv fd failed");
        return nullptr;
    }
    auto shm = std::make_shared<SharedMemory>();
    if (!shm->Attach(fds[0])) {
        LOGE("IpcChannel::ReceiveShmFdInternal Attach failed");
        for (int fd : fds) close(fd);
        return nullptr;
    }
    close(fds[0]);
    LOGI("IpcChannel::ReceiveShmFdInternal received fd=%{public}d",
         shm->GetFd());
    return shm;
}

IpcChannelState IpcChannel::GetState() const { return state_; }
IpcChannelRole IpcChannel::GetRole() const { return role_; }
int IpcChannel::GetSocketFd() const { return socket_.GetFd(); }
SharedMemory* IpcChannel::GetShm() { return shm_.get(); }

}  // namespace ohos::adapter::new_ipc

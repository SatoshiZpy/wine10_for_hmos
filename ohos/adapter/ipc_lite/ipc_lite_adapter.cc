// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/ipc_lite/ipc_lite_adapter.h"

#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "ohos/adapter/common/logging.h"
#include "ohos/adapter/common/mutex.h"
#include "ohos/adapter/common/unix_socket_utils.h"
#include "ohos/adapter/multiprocess/app_spawn_communication.h"
#include "ohos/adapter/multiprocess/child_process_starter.h"

using namespace ohos::adapter::common;
using namespace ohos::adapter::multiprocess;

namespace ohos::adapter::ipc_lite {

static const std::string kIpcSocketPathPrefix = "/data/storage/el2/base/files/wine_ipc_lite_";

static std::string GenerateRandomName()
{
    static const char kChars[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    std::string name;
    for (int i = 0; i < 8; i++) { name += kChars[rand() % (sizeof(kChars) - 1)]; }
    return name;
}

static std::string GetProcessName(const std::vector<std::string>& commands)
{
    if (commands.empty()) { return GenerateRandomName(); }
    if (commands[0] == "wineserver") { return "wineserver"; }
    if (commands[0] == "wine" && commands.size() >= 2) {
        std::string image = commands[1];
        size_t pos = image.rfind('\\');
        if (pos == std::string::npos) { pos = image.rfind('/'); }
        if (pos != std::string::npos) { image = image.substr(pos + 1); }
        if (image.size() > 4 && image.substr(image.size() - 4) == ".exe") {
            image = image.substr(0, image.size() - 4);
        }
        if (!image.empty()) {
            return image + "-" + GenerateRandomName();
        }
    }
    return GenerateRandomName();
}

static std::vector<uint8_t> SerializeProcessInfo(
    const std::vector<std::string>& commands, const std::vector<std::pair<int, int>>& fds)
{
    std::vector<uint8_t> buf;
    int32_t msg_type = MSG_SET_PROCESS_INFO;
    auto append_i32 = [&](int32_t v) {
        buf.insert(buf.end(), reinterpret_cast<uint8_t*>(&v), reinterpret_cast<uint8_t*>(&v) + 4);
    };
    append_i32(msg_type);
    append_i32(static_cast<int32_t>(commands.size()));
    for (const auto& cmd : commands) {
        append_i32(static_cast<int32_t>(cmd.size()));
        buf.insert(buf.end(), cmd.begin(), cmd.end());
    }
    append_i32(static_cast<int32_t>(fds.size()));
    for (const auto& [fd, remap] : fds) { append_i32(remap); }
    return buf;
}

bool DeserializeProcessInfo(const uint8_t* data, size_t len,
    std::vector<std::string>& commands, std::vector<int32_t>& remap_ids)
{
    if (len < 4) return false;
    size_t pos = 0;
    auto read_i32 = [&]() -> int32_t {
        int32_t v = 0;
        if (pos + 4 > len) return -1;
        memcpy(&v, data + pos, 4);
        pos += 4;
        return v;
    };
    int32_t msg_type = read_i32();
    if (msg_type != MSG_SET_PROCESS_INFO) return false;
    int32_t cmd_count = read_i32();
    for (int32_t i = 0; i < cmd_count; i++) {
        int32_t str_len = read_i32();
        if (str_len < 0 || pos + str_len > len) return false;
        commands.push_back(std::string(reinterpret_cast<const char*>(data + pos), static_cast<size_t>(str_len)));
        pos += str_len;
    }
    int32_t fd_count = read_i32();
    for (int32_t i = 0; i < fd_count; i++) { remap_ids.push_back(read_i32()); }
    return true;
}

int IpcLiteHelper::CreateProcess(
    const std::vector<std::string>& commands, const std::vector<std::pair<int, int>>& fds)
{
    if (commands.empty()) {
        LOGE("IpcLiteHelper::CreateProcess empty commands");
        return -1;
    }
    std::string processKey = GetProcessName(commands);
    std::string socket_path = kIpcSocketPathPrefix
        + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".sock";
    if (!ChildProcessLiteHost::Instance().StartServer(socket_path)) {
        LOGE("IpcLiteHelper CreateProcess: StartServer failed");
        return -1;
    }
    ChildProcessStarter::GetInstance().CreateProcess(processKey, socket_path);
    int32_t pid = ChildProcessLiteHost::Instance().AcceptAndSetup(commands, fds);
    ChildProcessLiteHost::Instance().Close();
    if (pid <= 0) {
        LOGE("IpcLiteHelper CreateProcess: got invalid pid=%{public}d", pid);
        return -1;
    }
    LOGI("IpcLiteHelper CreateProcess done: pid=%{public}d, key=%{public}s, sock=%{public}s",
         pid, processKey.c_str(), socket_path.c_str());
    return pid;
}

void IpcLiteHelper::KillProcess()
{
    LOGI("IpcLiteHelper::KillProcess");
    ChildProcessStarter::GetInstance().KillProcess();
}

ChildProcessLiteHost& ChildProcessLiteHost::Instance()
{
    static ChildProcessLiteHost instance;
    return instance;
}

bool ChildProcessLiteHost::StartServer(const std::string& socket_path)
{
    if (server_started_) {
        LOGW("ChildProcessLiteHost server already started");
        return true;
    }
    server_fd_ = UnixSocketUtils::CreateServerSocket(socket_path);
    if (server_fd_ < 0) {
        LOGE("ChildProcessLiteHost StartServer failed");
        return false;
    }
    server_socket_path_ = socket_path;
    server_started_ = true;
    LOGI("ChildProcessLiteHost StartServer at %{public}s", socket_path.c_str());
    return true;
}

int32_t ChildProcessLiteHost::AcceptAndSetup(
    const std::vector<std::string>& commands, const std::vector<std::pair<int, int>>& fds)
{
    int client_fd = accept(server_fd_, nullptr, nullptr);
    if (client_fd < 0) {
        LOGE("ChildProcessLiteHost accept failed");
        return -1;
    }
    LOGI("ChildProcessLiteHost accepted client fd=%{public}d", client_fd);
    std::vector<uint8_t> send_buf = SerializeProcessInfo(commands, fds);
    std::vector<int> send_fds;
    for (const auto& [fd, remap] : fds) { send_fds.push_back(fd); }
    if (!UnixSocketUtils::SendMsgWithFds(client_fd, send_buf.data(), send_buf.size(), send_fds)) {
        LOGE("ChildProcessLiteHost SendMsgWithFds failed");
        close(client_fd);
        return -1;
    }
    int32_t pid = -1;
    ssize_t ret = recv(client_fd, &pid, sizeof(pid), 0);
    if (ret != sizeof(pid)) {
        LOGE("ChildProcessLiteHost recv pid failed, ret=%{public}zd", ret);
        close(client_fd);
        return -1;
    }
    LOGI("ChildProcessLiteHost got pid=%{public}d", pid);
    close(client_fd);
    return pid;
}

void ChildProcessLiteHost::Close()
{
    if (server_fd_ >= 0) { close(server_fd_); server_fd_ = -1; }
    if (!server_socket_path_.empty()) {
        UnixSocketUtils::DeleteSocketFile(server_socket_path_);
        server_socket_path_.clear();
    }
    server_started_ = false;
}

}  // namespace ohos::adapter::ipc_lite

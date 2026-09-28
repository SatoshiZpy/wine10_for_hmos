// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/web_entry/web_entry.h"

#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <thread>

#include "ohos/adapter/aki_hook/aki_hook.h"
#include "ohos/adapter/common/constants.h"
#include "ohos/adapter/common/logging.h"
#include "ohos/adapter/common/unix_socket_utils.h"
#include "ohos/adapter/ipc_lite/ipc_lite_adapter.h"
#include "ohos/adapter/multiprocess/app_spawn_communication.h"
#include "ohos/adapter/multiprocess/command_line/command_line.h"
#include "ohos/adapter/multiprocess/command_line/command_line_helper.h"
#include "ohos/adapter/new_ipc/child_process_entry.h"
#include "ohos/adapter/web_entry/web_loader.h"

using namespace std::chrono_literals;
namespace ohos::adapter::web_entry {

const std::string kFdRemapCommand = "--fds_to_remap=";
const std::string kEnvCommand = "--env=";
const std::string kCrashpadHandler = "crashpad-handler";

static bool g_initialized = false;
static std::atomic<bool> g_destroyed(false);

namespace {
void SetEnv(const std::vector<std::string>& env) {
  if (env.empty()) {
    return;
  }
  int count = env.size();
  for (int i = 0; i < count - 1; i += 2) {
    auto& key = env[i];
    auto& value = env[i + 1];
    if (!value.empty()) {
      setenv(key.c_str(), value.c_str(), 1 );
      if (key == "WINEDEBUGLOG") {
        int fd = open(value.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0666);
        if (fd != -1)
        {
          dup2(fd, STDOUT_FILENO);
          dup2(fd, STDERR_FILENO);
          close(fd);
        }
      }
    }
    else {
      unsetenv(key.c_str());
    }
  }
}

void Destroy() {
  LOGI("Process exiting, calling AppLifecycleAdapter.onDestroy");
  auto jsfunc = ohos::adapter::GetJSFunction(
      "AppLifecycleAdapter.onDestroy");
  if (!jsfunc) {
      LOGE("AppLifecycleAdapter.onDestroy not defined");
      return;
  }
  jsfunc->Invoke<void>();
}

bool Destroyed() {
  return g_destroyed.load();
}
}  // namespace

void RunBrowser(const std::vector<std::string>& args) {
  LOGI("RunBrowser in g_initialized: %{public}d", g_initialized);
  ohos::adapter::multiprocess::CommandLine::InitForCurrentProcess(args);

  if (!g_initialized) {
    std::thread WineMainthread([args]() {
      auto loader = BasicLoader::Create(USE_LOADER_TARGET);
      if (loader->StartMainProcess(ProcessType::kMainProcess, args) != 0) {
        return;
      }
      Destroy();
      g_destroyed.store(true);
    });
    WineMainthread.detach();
    g_initialized = true;
  }

  LOGI("RunBrowser out g_initialized: %{public}d", g_initialized);
}

int RunOtherProcessType(int process_type) {
  std::vector<std::string> commands;
  multiprocess::AppSpawnCommunication::RequestCommandline(commands);
  std::vector<const char*> cstrs;
  auto loader = BasicLoader::Create(USE_LOADER_TARGET);
  for (size_t i = 0; i < commands.size(); ++i) {
    if (commands[i].find("--type") != std::string::npos) {
      auto pos = commands[i].find_last_of("=");
      auto proc_type = commands[i].substr(pos + 1);
      if (proc_type == kCrashpadHandler) {
        process_type = ProcessType::kCrashpadHandler;
        continue;
      }
    }
    if (process_type == ProcessType::kCrashpadHandler &&
        commands[i].starts_with(kFdRemapCommand)) {
      continue;
    }

    cstrs.push_back(commands[i].data());
  }
  ohos::adapter::multiprocess::CommandLine::InitForCurrentProcess(commands);

  int result = -1;
  switch (process_type) {
    case ProcessType::kRenderProcess:
    case ProcessType::kGpuProcess:
    case ProcessType::kCrashpadHandler:
      result = loader->StartChildProcess(process_type, cstrs.size(), cstrs.data());
      break;
    default:
      LOGI("RunOtherProcessType nothing to do");
      break;
  }
  return result;
}

int RunIsolateProcessType(int process_type,
                          const std::vector<std::string>& commands) {
  LOGI("RunIsolateProcessType begin process_type: %{public}d", process_type);
  auto loader = BasicLoader::Create(USE_LOADER_TARGET, process_type);
  std::vector<const char*> cstrs;
  for (size_t i = 0; i < commands.size(); ++i) {
    if (commands[i].find("--type") != std::string::npos) {
      auto pos = commands[i].find_last_of("=");
      auto proc_type = commands[i].substr(pos + 1);
      if (proc_type == kCrashpadHandler) {
        process_type = ProcessType::kCrashpadHandler;
        continue;
      }
    }
    if (process_type == ProcessType::kCrashpadHandler &&
        commands[i].starts_with(kFdRemapCommand)) {
      continue;
    }
    if (commands[i].starts_with(kEnvCommand)) {
      auto pos = commands[i].find_last_of("=");
      auto env_str = commands[i].substr(pos + 1);
      std::vector<std::string> env = multiprocess::CommandLineHelper::Split(env_str, ";");
      SetEnv(env);
      continue;
    }

    cstrs.push_back(commands[i].data());
  }
  auto result = loader->StartChildProcess(process_type, cstrs.size(), cstrs.data());
  LOGI("RunIsolateProcessType ret = %{public}d", result);
  return result;
}

void RunWine(int process_type,
             const std::vector<std::string>& args,
             const std::vector<std::string>& env) {
  LOGI("RunWine in g_initialized: %{public}d", g_initialized);
  if (!g_initialized) {
    multiprocess::CommandLineHelper::SetCommandLine(process_type, args, env);
    std::thread mainthread([args, env, process_type]() {
      /* set the environment variables first: the IPC server derives its
       * socket directory from WINE_IPC_DIR */
      SetEnv(env);

      /* 每个进程（主进程与被 spawn 的子进程）都持有 IPC server：
       * 以 pid 命名常驻监听，等待其他进程在嵌入本进程窗口时按需连接 */
      if (StartIpcServerEntry() != 0) {
        LOGW("RunWine: ipc server start failed (continue)");
      }

      auto loader = BasicLoader::Create(USE_LOADER_TARGET, process_type);
      if (loader->StartMainProcess(process_type, args) != 0) {
        Destroy();
        return;
      }

      Destroy();
      g_destroyed.store(true);
    });
    mainthread.detach();
    g_initialized = true;
  }

  LOGI("RunWine out g_initialized: %{public}d", g_initialized);
}

int RunWineProcess(const std::vector<std::string>& env_args,
                   const std::string& ipc_socket_path)
{
    LOGI("RunWineProcess pid=%{public}d, socket=%{public}s",
         getpid(), ipc_socket_path.c_str());

    if (ipc_socket_path.empty()) {
        LOGE("RunWineProcess: ipc_socket_path is empty");
        return -1;
    }

    /* spawn 链路：ipc_lite 一次性握手取 argv 与 fd remap（与宿主一致） */
    int fd = common::UnixSocketUtils::CreateClientSocket(ipc_socket_path);
    if (fd < 0) {
        LOGE("RunWineProcess: connect failed");
        return -1;
    }

    const size_t kMaxBufSize = 4096;
    std::vector<uint8_t> recv_buf(kMaxBufSize);
    std::vector<int> recv_fds;

    ssize_t len = common::UnixSocketUtils::RecvMsgWithFdsAndPid(
        fd, recv_buf.data(), recv_buf.size(), recv_fds);
    if (len <= 0) {
        LOGE("RunWineProcess: recv failed, len=%{public}zd", len);
        close(fd);
        return -1;
    }

    std::vector<std::string> commands;
    std::vector<int32_t> remap_ids;

    if (!ipc_lite::DeserializeProcessInfo(
            recv_buf.data(), static_cast<size_t>(len),
            commands, remap_ids)) {
        LOGE("RunWineProcess: deserialize failed");
        close(fd);
        return -1;
    }

    std::vector<std::pair<int, int>> mapped_fds;
    for (size_t i = 0; i < recv_fds.size() && i < remap_ids.size(); i++) {
        mapped_fds.push_back({recv_fds[i], remap_ids[i]});
    }

    multiprocess::AppSpawnCommunication::SetFdIdsRemap(mapped_fds);

    int32_t pid = getpid();
    ssize_t ret = send(fd, &pid, sizeof(pid), 0);
    if (ret != sizeof(pid)) {
        LOGE("RunWineProcess: send pid failed");
        close(fd);
        return -1;
    }

    close(fd);
    LOGI("RunWineProcess done: pid=%{public}d, socket closed", pid);

    int process_type = (!commands.empty() && commands[0] == "wineserver")
        ? ProcessType::kServerProcess : ProcessType::kChildProcess;
    RunWine(process_type, commands, env_args);
    return 0;
}

JSBIND_GLOBAL() {
  JSBIND_FUNCTION(Destroyed);
}

}  // namespace ohos::adapter::web_entry

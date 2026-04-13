// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/web_entry/web_entry.h"

#include <unistd.h>

#include <atomic>
#include <chrono>
#include <thread>

#include "ohos/adapter/aki_hook/aki_hook.h"
#include "ohos/adapter/common/constants.h"
#include "ohos/adapter/common/logging.h"
#include "ohos/adapter/multiprocess/app_spawn_communication.h"
#include "ohos/adapter/multiprocess/command_line/command_line.h"
#include "ohos/adapter/multiprocess/command_line/command_line_helper.h"
#include "ohos/adapter/web_entry/web_loader.h"

using namespace std::chrono_literals;
namespace ohos::adapter::web_entry {

const std::string kFdRemapCommand = "--fds_to_remap=";
const std::string kEnvCommand = "--env=";
const std::string kCrashpadHandler = "crashpad-handler";

static bool g_initialized = false;
static std::atomic<bool> g_destroyed(false);

namespace  {
void SetEnv(const std::vector<std::string>& env) {
  if (env.empty()) {
    return;
  }
  int count = env.size();
  for (int i = 0; i < count - 1; i += 2) {
    auto& key = env[i];
    auto& value = env[i + 1];
    if (!value.empty()) {
      LOGD("set the environment %{public}s:%{public}s", key.c_str(), value.c_str());
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
}

void DestroyBrowser() {
  LOGE("The browser process has exited");
  auto jsfunc = ohos::adapter::GetJSFunction(
      "AppLifecycleAdapter.onWebDestroy");
  if (!jsfunc) {
      LOGE("AppLifecycleAdapter.onWebDestroy not defined");
      return;
  }
  jsfunc->Invoke<void>();
}

bool Destroyed() {
  return g_destroyed.load();
}

void RunBrowser(const std::vector<std::string>& args) {
  LOGI("RunBrowser in g_initialized: %{public}d", g_initialized);
  ohos::adapter::multiprocess::CommandLine::InitForCurrentProcess(args);

  if (!g_initialized) {
    std::thread WineMainthread([args]() {
      auto loader = BasicLoader::Create(USE_LOADER_TARGET);
      if (loader->StartMainProcess(args) != 0) {
        return;
      }
      DestroyBrowser();
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

void RunWine(const std::vector<std::string>& args,
             const std::vector<std::string>& env) {
  LOGI("RunWine in g_initialized: %{public}d", g_initialized);
  if (!g_initialized) {
    multiprocess::CommandLineHelper::WrapCommands(args, env);
    std::thread mainthread([args, env]() {
      /* set the environment variables */
      SetEnv(env);

      auto loader = BasicLoader::Create(USE_LOADER_TARGET);
      if (loader->StartMainProcess(args) != 0) {
        return;
      }

      g_destroyed.store(true);
    });
    mainthread.detach();
    g_initialized = true;
  }

  LOGI("RunWine out g_initialized: %{public}d", g_initialized);
}

JSBIND_GLOBAL() {
  JSBIND_FUNCTION(Destroyed);
}

}  // namespace ohos::adapter::web_entry

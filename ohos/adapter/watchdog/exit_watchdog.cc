// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/watchdog/exit_watchdog.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <strings.h>
#include <thread>

#include "ohos/adapter/common/logging.h"
#include "ohos/adapter/multiprocess/child_process_starter.h"
#include "ohos/adapter/window/wine_window_manager.h"

namespace ohos::adapter::watchdog {

namespace {

constexpr uint32_t kExitGraceMs = 5000;
/* A thread death without the orderly mark also occurs in processes being
 * force-terminated during session teardown; those die almost immediately,
 * a crashed-thread zombie does not. */
constexpr uint32_t kAbnormalGraceMs = 2000;

std::atomic<uint32_t> g_exit_watchdog_generation{0};

/* wine main-thread liveness monitor */
std::atomic<bool> g_wine_thread_completed{true};
std::atomic<bool> g_wine_thread_watched{false};
std::thread g_wine_main_thread;

bool ExitWatchdogEnabled()
{
  const char* env = getenv("WINE_EXIT_WATCHDOG");
  if (!env || !*env) {
    return false;
  }
  return strcasecmp(env, "true") == 0 || strcmp(env, "1") == 0;
}

}  // namespace

void ExitWatchdog()
{
  if (!ExitWatchdogEnabled()) {
    LOGI("exit watchdog disabled (WINE_EXIT_WATCHDOG not true), keeping "
         "the app running");
    return;
  }
  uint32_t generation = ++g_exit_watchdog_generation;
  LOGW("exit watchdog armed, grace %{public}u ms", kExitGraceMs);
  std::thread([generation]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(kExitGraceMs));
    if (generation != g_exit_watchdog_generation.load()) {
      return; /* re-shown, re-created or re-armed meanwhile */
    }
    if (window::WineWindowManager::GetInstance().HasVisibleToplevelWindow()) {
      LOGI("exit watchdog canceled: a visible toplevel window exists");
      return;
    }
    LOGW("app keeps running with no visible window, terminating app");
    multiprocess::ChildProcessStarter::GetInstance().KillProcess();
  }).detach();
}

void CancelExitWatchdog()
{
  ++g_exit_watchdog_generation;
}

void MarkWineOrderlyExit()
{
  /* The death is controlled and the kill chain is already tearing the
   * process down. */
  g_wine_thread_completed.store(true);
}

void WatchWineMainThread(std::function<void()> wine_main)
{
  if (!wine_main) {
    return;
  }
  if (g_wine_thread_watched.exchange(true)) {
    LOGW("wine main thread already watched, running unwatched");
    std::thread(std::move(wine_main)).detach();
    return;
  }

  /* Reset before the thread starts, so a fast-completing body cannot be
   * overwritten back to "not completed". */
  g_wine_thread_completed.store(false);
  g_wine_main_thread = std::thread([wine_main = std::move(wine_main)]() {
    wine_main();
    g_wine_thread_completed.store(true);
  });
  std::thread([]() {
    g_wine_main_thread.join();
    if (g_wine_thread_completed.load()) {
      LOGI("wine main thread exited orderly (body returned or "
           "KillNativeProcess seen)");
      return;
    }
    /* Only a genuine zombie (dead wine thread, arkui still alive)
     * survives the grace window. */
    std::this_thread::sleep_for(std::chrono::milliseconds(kAbnormalGraceMs));
    LOGE("wine main thread died abnormally (no orderly exit, arkui "
         "would stay alive), terminating app");
    multiprocess::ChildProcessStarter::GetInstance().KillProcess();
  }).detach();
}

}  // namespace ohos::adapter::watchdog

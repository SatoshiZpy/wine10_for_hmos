// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_WATCHDOG_EXIT_WATCHDOG_H_
#define OHOS_ADAPTER_WATCHDOG_EXIT_WATCHDOG_H_

#include <functional>

namespace ohos::adapter::watchdog {

/* Delayed-exit watchdog for an app that stays alive with no visible
 * window.  Opt-in through the WINE_EXIT_WATCHDOG environment variable:
 * enabled when set to "true" or "1", disabled otherwise.  ExitWatchdog()
 * arms a 5 s timer when the last visible toplevel window is hidden or
 * destroyed, and terminates the app if no toplevel window re-appears in
 * time.  CancelExitWatchdog() cancels a pending exit when a window is
 * shown again or a new toplevel window is created. */
void ExitWatchdog();
void CancelExitWatchdog();

/* Run the wine main-thread body on a monitored thread; applies to every
 * wine process type and cannot be disabled.  The body returning, or an
 * orderly exit marked by MarkWineOrderlyExit(), counts as controlled
 * completion.  A thread death without either leaves the arkui side
 * running with a dead wine thread: after a short grace period the app
 * is terminated. */
void WatchWineMainThread(std::function<void()> wine_main);

/* Mark the wine thread of this process as orderly-exiting.  Called from
 * KillNativeProcess() on the exiting thread, before it dies. */
void MarkWineOrderlyExit();

}  // namespace ohos::adapter::watchdog

#endif  // OHOS_ADAPTER_WATCHDOG_EXIT_WATCHDOG_H_

// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_WINDOW_WINDOW_STATE_MONITOR_H_
#define OHOS_ADAPTER_WINDOW_WINDOW_STATE_MONITOR_H_

#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>

namespace ohos::adapter::window {

class WindowStateMonitor {
  public:
    enum State {
      WINDOW_NONE,
      WINDOW_ENABLE,
      WINDOW_DISABLE
    };

    bool WaitForState(State target, std::chrono::milliseconds timeout) {
      std::unique_lock<std::mutex> lock(mtx_);
      bool success = cv_.wait_for(lock, timeout, [this, target] {
          return state_ == target;
      });
      return success;
    }

    void SetState(State newState) {
      {
          std::lock_guard<std::mutex> lock(mtx_);
          state_ = newState;
      }
      cv_.notify_all();
    }

    State GetState() {
      std::lock_guard<std::mutex> lock(mtx_);
      return state_;
    }

  private:
    State state_ = WINDOW_NONE;
    std::mutex mtx_;
    std::condition_variable cv_;
};

}  // namespace ohos::adapter::window
#endif  // OHOS_ADAPTER_WINDOW_WINDOW_STATE_MONITOR_H_
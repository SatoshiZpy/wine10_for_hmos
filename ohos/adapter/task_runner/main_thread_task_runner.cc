/*
 * Copyright (c) 2023-2025 Huawei Device Co., Ltd.
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of
 *    conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list
 *    of conditions and the following disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used
 *    to endorse or promote products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "ohos/adapter/task_runner/main_thread_task_runner.h"
 
#include "ohos/adapter/common/logging.h"
 
namespace ohos::adapter::taskRunner {
 
MainThreadTaskRunner& MainThreadTaskRunner::GetInstance() {
  static MainThreadTaskRunner instance;
  return instance;
}
 
// This method can only be called in the main thread
void MainThreadTaskRunner::Initialize(napi_env env) {
  if (loop_ != nullptr) {
    return;
  }
  napi_get_uv_event_loop(env, &loop_);
}
 
__attribute__((no_sanitize("cfi", "cfi-icall")))
void MainThreadTaskRunner::PostTask(std::function<void()> task_func) {
  if (loop_ == nullptr) {
    LOGE("MainThreadTaskRunner::PostTask not initialized");
    return;
  }
 
  if (task_func == nullptr) {
    LOGE("MainThreadTaskRunner::PostTask Invalid parameter");
    return;
  }
 
  uv_work_t* work = new uv_work_t;
  if (work == nullptr) {
    LOGE("MainThreadTaskRunner::PostTask create work object failed");
    return;
  }
  MainThreadTaskRunnerFuncWrapper* wrapper =
      new MainThreadTaskRunnerFuncWrapper();
  if (wrapper == nullptr) {
    LOGE("MainThreadTaskRunner::PostTask create task runner context failed");
    return;
  }
  wrapper->task_func = task_func;
  work->data = wrapper;
 
  uv_after_work_cb after_work = [](uv_work_t* work, int status) {
    if (work == nullptr) {
      LOGE("MainThreadTaskRunner::PostTask input param work is nullptr");
      return;
    }
 
    MainThreadTaskRunnerFuncWrapper* wrapper =
        static_cast<MainThreadTaskRunnerFuncWrapper*>(work->data);
    if (wrapper == nullptr) {
      LOGE("MainThreadTaskRunner::PostTask static cast runner context failed");
      delete work;
      work = nullptr;
      return;
    }
 
    wrapper->task_func();
 
    delete wrapper;
    wrapper = nullptr;
    delete work;
    work = nullptr;
  };
 
  // Submit task to main thread
  uv_queue_work(
      loop_, std::move(work), [](uv_work_t* work) {}, std::move(after_work));
}
 
}  // namespace ohos::adapter::taskRunner
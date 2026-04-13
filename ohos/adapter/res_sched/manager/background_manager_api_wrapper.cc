// Copyright (c) 2025 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/res_sched/manager/background_manager_api_wrapper.h"

#include "ohos/adapter/common/logging.h"
#include "ohos/adapter/common/shared_library.h"

#include <type_traits>

namespace ohos::adapter::res_sched::manager {
// ensure the exported function in dll first int parameter is valid pid_t type
static_assert(std::is_same_v<pid_t, int>, "the type pid_t must be same as int");

/**
 * log error when load symbol failed
 * @param error string literal of function name
 */
void LogLoadError(const char* error) {
  LOGE(
      "BackgroundManagerApiWrapperImpl::BackgroundManagerApiWrapperImpl get "
      "%{public}s failed",
      error);
}

class BackgroundManagerApiWrapperImpl : public BackgroundManagerApiWrapper {
 public:
  static constexpr const char* kSetProcessPriority =
      "OH_BackgroundProcessManager_SetProcessPriority";
  static constexpr const char* kResetProcessPriority =
      "OH_BackgroundProcessManager_ResetProcessPriority";
  BackgroundManagerApiWrapperImpl();
  ErrorCode SetProcessPriority(pid_t pid, ProcessPriority priority) override;
  ErrorCode ResetProcessPriority(pid_t pid) override;

 protected:
  using SetProcessPriorityFn = ErrorCode (*)(pid_t pid,
                                             ProcessPriority priority);
  using ResetProcessPriorityFn = ErrorCode (*)(pid_t pid);
  common::SharedLibrary background_dll_;
  SetProcessPriorityFn set_process_priority_fn_ = nullptr;
  ResetProcessPriorityFn reset_process_priority_fn_ = nullptr;
};

__attribute__((no_sanitize("cfi", "cfi-icall")))
BackgroundManagerApiWrapperImpl::BackgroundManagerApiWrapperImpl()
    : background_dll_("/system/lib64/ndk/", "background_process_manager.z") {
  set_process_priority_fn_ = reinterpret_cast<SetProcessPriorityFn>(
      background_dll_.GetSymbol(kSetProcessPriority));
  if (!set_process_priority_fn_) {
    LogLoadError(kSetProcessPriority);
  }
  reset_process_priority_fn_ = reinterpret_cast<ResetProcessPriorityFn>(
      background_dll_.GetSymbol(kResetProcessPriority));
  if (!reset_process_priority_fn_) {
    LogLoadError(kResetProcessPriority);
  }
}

__attribute__((no_sanitize("cfi", "cfi-icall")))
BackgroundManagerApiWrapper::ErrorCode
BackgroundManagerApiWrapperImpl::SetProcessPriority(pid_t pid,
                                                    ProcessPriority priority) {
  if (!set_process_priority_fn_) {
    return BackgroundManagerApiWrapper::ErrorCode::ERR_NOT_LOADED;
  }
  auto code = set_process_priority_fn_(pid, priority);
  LOGI(
      "BackgroundManagerApiWrapper::SetProcessPriority pid: %{public}d, "
      "priority: %{public}d, result: %{public}d",
      pid, priority, code);
  return code;
}

__attribute__((no_sanitize("cfi", "cfi-icall")))
BackgroundManagerApiWrapper::ErrorCode
BackgroundManagerApiWrapperImpl::ResetProcessPriority(pid_t pid) {
  if (!reset_process_priority_fn_) {
    return BackgroundManagerApiWrapper::ErrorCode::ERR_NOT_LOADED;
  }
  auto code = reset_process_priority_fn_(pid);
  LOGI(
      "BackgroundManagerApiWrapper::ResetProcessPriority pid: %{public}d, "
      "result: %{public}d",
      pid, code);
  return code;
}

BackgroundManagerApiWrapper& BackgroundManagerApiWrapper::GetInstance() {
  static BackgroundManagerApiWrapperImpl instance;
  return instance;
}

}  // namespace ohos::adapter::res_sched::manager

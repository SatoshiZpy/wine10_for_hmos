// Copyright (c) 2025 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_RES_SCHED_MANAGER_BACKGROUND_MANAGER_API_WRAPPER_H_
#define OHOS_ADAPTER_RES_SCHED_MANAGER_BACKGROUND_MANAGER_API_WRAPPER_H_

#include <sys/types.h>
#include "ohos/adapter/export.h"

namespace ohos::adapter::res_sched::manager {
class ADAPTER_EXPORT_API BackgroundManagerApiWrapper {
 public:
  /** Describes the level of BackgroundProcessManager priority. */
  enum ProcessPriority: int32_t {
    /** Means the process has stopped working and in the background */
    PROCESS_BACKGROUND = 1,
    /** Means the process is working in the background */
    PROCESS_INACTIVE = 2,
  };
  /** Enum for BackgroundProcessManager error code. */
  enum ErrorCode: int32_t {
    /** @error result is OK. */
    ERR_BACKGROUND_PROCESS_MANAGER_SUCCESS = 0,
    /**
     * @error invalid parameter. Possible causes:
     * 1. priority is out of range.
     */
    ERR_BACKGROUND_PROCESS_MANAGER_INVALID_PARAM = 401,
    /**
     * @error remote error. Possible causes:
     * 1. remote is not work.
     */
    ERR_BACKGROUND_PROCESS_MANAGER_REMOTE_ERROR = 31800001,
    /**
     * custom error code:
     * not loaded, means the current version not support that api
     */
    ERR_NOT_LOADED = 404,
  };
  /** get the singleton instance */
  static BackgroundManagerApiWrapper& GetInstance();

  /**
   * lower the process qos of the process
   * @param pid the pid of the target process
   * @param priority the priority type to be lowered
   * @returns the error code, 0 for invoke succeed
   */
  virtual ErrorCode SetProcessPriority(pid_t pid, ProcessPriority priority) = 0;
  /**
   * restore the process qos of the process to normal active state
   * @param pid the pid of the target process
   */
  virtual ErrorCode ResetProcessPriority(pid_t pid) = 0;
};
}  // namespace ohos::adapter::res_sched::manager

#endif

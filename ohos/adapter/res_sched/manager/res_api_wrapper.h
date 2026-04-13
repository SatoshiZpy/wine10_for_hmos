// Copyright (c) 2025 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_RES_SCHED_MANAGER_RES_API_WRAPPER_H_
#define OHOS_ADAPTER_RES_SCHED_MANAGER_RES_API_WRAPPER_H_

#include <cstdint>
#include <string>
#include <unordered_map>
#include "ohos/adapter/common/shared_library.h"
#include "ohos/adapter/export.h"

namespace ohos::adapter::res_sched::manager {
/**
 * ResApiWrapper wrap the process of get inner res_sched api
 * and process of invoke api.
 */
class ADAPTER_EXPORT_API ResApiWrapper {
 public:
  static ResApiWrapper& GetInstance();
  /** not copyable and transferable */
  ResApiWrapper(const ResApiWrapper&) = delete;
  /** call before `invokeApi` to avoid construct payload parameter */
  bool isValid() const { return fn; }
  /**
   * invoke the inner res_sched api to change schedule
   * @param resType the inner api category number
   * @param param extra parameter according to resType
   * @param payload extra payload, dist that key value are both string
   */
  bool invokeApi(uint32_t resType,
                 int64_t param,
                 std::unordered_map<std::string, std::string>& payload);

 private:
  /**
   * init the wrapper api in constructor, connect dll and load function by get
   * symbol
   */
  ResApiWrapper();
  ~ResApiWrapper() = default;
  using ReportFunc =
      void (*)(uint32_t,
               int64_t,
               const std::unordered_map<std::string, std::string>&);
  /** the client dll */
  common::SharedLibrary ressched_client_dll_;
  /** pointer to inner api */
  ReportFunc fn = nullptr;
  std::atomic<bool> log_first_empty = false;
};
}  // namespace ohos::adapter::res_sched::manager

#endif  // OHOS_ADAPTER_RES_SCHED_MANAGER_RES_API_WRAPPER_H_

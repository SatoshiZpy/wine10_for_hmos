// Copyright (c) 2025 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/res_sched/manager/res_api_wrapper.h"
#include "ohos/adapter/common/logging.h"
#include "ohos/adapter/common/shared_library.h"

#include <bundle/native_interface_bundle.h>
#include <dlfcn.h>
#include <type_traits>

namespace ohos::adapter::res_sched::manager {

namespace {
/**
 * use an false initialize atomic bool to guard first time
 */
bool ForFirstTime(std::atomic<bool>& false_first) {
  bool false_value = false;
  return false_first.compare_exchange_weak(false_value, true);
}

// dll full path is: `/system/lib64/platformsdk/libressched_client.z.so`
const char kDllDirPath[] = "/system/lib64/platformsdk/";
const char kDllName[] = "ressched_client.z";
}  // namespace

ResApiWrapper& ResApiWrapper::GetInstance() {
  static ResApiWrapper singleton;
  return singleton;
}

__attribute__((no_sanitize("cfi", "cfi-icall"))) ResApiWrapper::ResApiWrapper()
    : ressched_client_dll_(kDllDirPath, kDllName) {
  fn = reinterpret_cast<ReportFunc>(
      ressched_client_dll_.GetSymbol("ReportData"));
  if (!fn) {
    LOGE(
        "ResApiWrapper::ResApiWrapper get ReportData failed for dlsym "
        "ReportData function");
    return;
  }
}

__attribute__((no_sanitize("cfi", "cfi-icall"))) bool ResApiWrapper::invokeApi(
    uint32_t resType,
    int64_t param,
    std::unordered_map<std::string, std::string>& payload) {
  if (!fn) {
    if (ForFirstTime(log_first_empty)) {
      LOGE("ResApiWrapper::ResApiWrapper fn empty");
    }
    return false;
  }
  fn(resType, param, payload);
  return true;
}
}  // namespace ohos::adapter::res_sched::manager
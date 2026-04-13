// Copyright (c) 2025 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <bundle/native_interface_bundle.h>
#include <unistd.h>
#include <atomic>
#include <string>
#include <unordered_map>

#include "ohos/adapter/common/trace.h"
#include "ohos/adapter/res_sched/manager/res_api_wrapper.h"
#include "ohos/adapter/res_sched/res_sched.h"

namespace ohos::adapter::res_sched {
namespace {
const std::string kTriggerWebSlideNormal =
    "ResSchedManager::TriggerWebSlideNormal[ohos]";
const std::string kForceWebSlideNormal =
    "ResSchedManager::ForceWebSlideNormal[ohos]";

/**
 * use atomic reference to store process global bundle_name
 */
std::string GetBundleName() {
  OH_NativeBundle_ElementName element_name =
      OH_NativeBundle_GetMainElementName();
  return std::string(element_name.bundleName);
}
}  // namespace

ResSchedManager& ResSchedManager::GetInstance() {
  static ResSchedManager instance;
  return instance;
}

ResSchedManager::ResSchedManager() : bundle_name_(GetBundleName()) {}

bool ResSchedManager::TriggerWebSlideNormal(TimeMicro64 current_time) {
  TRACE_EVENT_0(kTriggerWebSlideNormal);
  bool result = web_slide_move_throttle_.AttemptEnter(current_time);
  if (result) {
    ForceWebSlideNormal(WebSlideParam::kBegin);
  }
  return result;
}

void ResSchedManager::ForceWebSlideNormal(WebSlideParam param) {
  TRACE_EVENT_1(kForceWebSlideNormal, "param", static_cast<int32_t>(param));
  pid_t pid = getpid();
  std::unordered_map<std::string, std::string> event_param = {
      {"pid", std::to_string(pid)}, {"bundleName", bundle_name_}};
  manager::ResApiWrapper::GetInstance().invokeApi(
      kResTypeWebSlideNormal, static_cast<int64_t>(param), event_param);
}
}  // namespace ohos::adapter::res_sched

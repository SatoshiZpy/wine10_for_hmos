// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/web_entry/chrome/chrome_loader.h"

#include "ohos/adapter/common/logging.h"
#include "ohos/adapter/common/shared_library.h"
#include "ohos/adapter/device_info/device_info.h"

namespace ohos::adapter::web_entry {
namespace {
__attribute__((no_sanitize("cfi", "cfi-icall")))
bool CheckAdvSecMode() {
  uint32_t mode = -1;
  common::SharedLibrary shared_library_("/system/lib64/platformsdk/",
                                        "dsmm_innersdk.z");
  auto fn = shared_library_.GetFunction<int(const char*, uint32_t, const char*,
                                            uint32_t, uint32_t*)>(
      "AdvancedSecurityModeGetStateByFeature");
  if (fn != nullptr) {
    // returns 0 if success or errcode
    int32_t ret = fn("default", 7, "default", 7, &mode); // len:7
    if (ret == 0 && mode == 1) {
      return true;
    }
  }
  return false;
}
}  // namespace

std::string ChromeMainLoader::GetEntryPoint(int process_type) {
  std::string entry_point;
  switch (process_type) {
    case ProcessType::kMainProcess:
    case ProcessType::kRenderProcess:
    case ProcessType::kGpuProcess:
      entry_point = "ChromeMain";
      break;
    case ProcessType::kCrashpadHandler:
      entry_point = "CrashpadHandlerMain";
      break;
    default:
      LOGE("GetEntryPoint nothing to do, unknown process type: %{public}d",
           process_type);
      break;
  }
  return entry_point;
}

const std::vector<std::string> ChromeMainLoader::GetEntryArgs() {
  std::vector<std::string> args = {
      "chrome",
      "--use-gl=egl",
      "--enable-features=UseOzonePlatform",
      "--ozone-platform=ohos",
      "--enable-logging",
      "--ozone-dump-file=/data/storage/el2/base/cache/",
      "--no-zygote",
      "--user-data-dir=/data/storage/el2/base/files/",
      "--disable-gpu-watchdog",
      "--log-net-log",
  };
  ohos::adapter::device_info::DeviceType device_type =
    ohos::adapter::device_info::DeviceInfo::GetInstance().GetDeviceType();
  // pad not support gpu process
  if (device_type == ohos::adapter::device_info::DeviceType::_TABLET) {
    args.push_back("--in-process-gpu");
    args.push_back("--js-flags=--jitless");
  }
  if (CheckAdvSecMode()) {
    args.emplace_back("--js-flags=--jitless");
  }
  return args;
}

}  // namespace ohos::adapter::web_entry

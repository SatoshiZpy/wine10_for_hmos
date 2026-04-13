// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/web_entry/shell/content_shell_loader.h"

#include "ohos/adapter/common/logging.h"

namespace ohos::adapter::web_entry {

std::string ContentShellLoader::GetEntryPoint(int process_type) {
  std::string entry_point;
  switch (process_type) {
    case ProcessType::kMainProcess:
    case ProcessType::kRenderProcess:
    case ProcessType::kGpuProcess:
      entry_point = "ContentMain";
      break;
    default:
      LOGE("GetEntryPoint nothing to do, unknown process type: %{public}d",
           process_type);
      break;
  }
  return entry_point;
}

const std::vector<std::string> ContentShellLoader::GetEntryArgs() {
  const std::vector<std::string> args = {
    "content_shell",
    "--use-gl=egl",
    "--enable-features=UseOzonePlatform",
    "--ozone-platform=ohos",
    "--enable-logging",
    "--v=0",
    "--ozone-dump-file=/data/storage/el2/base/cache/",
    "--log-file=/data/storage/el2/base/cache/test.log",
    "--disable-gpu-sandbox",
    "--no-zygote",
    "--content-shell-host-window-size=1139x654",
    "--remote-debugging-port=9222",  // for remote dubug and autotest
    "--remote-allow-origins=http://localhost:9222",  // for remote dubug and autotest
  };
  return args;
}

}  // namespace ohos::adapter::web_entry

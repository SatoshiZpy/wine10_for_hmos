// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/web_entry/wine/wine_loader.h"

#include "ohos/adapter/common/constants.h"
#include "ohos/adapter/common/logging.h"
#include "ohos/adapter/common/shared_library.h"
#include "ohos/adapter/device_info/device_info.h"

namespace ohos::adapter::web_entry {

std::string WineMainLoader::GetEntryPoint(int process_type) {
  std::string entry_point;
  switch (process_type) {
    case ProcessType::kMainProcess:
    case ProcessType::kChildProcess:
    case ProcessType::kServerProcess:
      entry_point = "wine_main";
      break;
    default:
      LOGE("GetEntryPoint nothing to do, unknown process type: %{public}d",
           process_type);
      break;
  }
  return entry_point;
}

const std::vector<std::string> WineMainLoader::GetEntryArgs() {
  return {};
}

}  // namespace ohos::adapter::web_entry

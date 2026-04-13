// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_MULTIPROCESS_ISOLATE_PROCESS_ISOLATE_PROCESS_HELPER_H_
#define OHOS_ADAPTER_MULTIPROCESS_ISOLATE_PROCESS_ISOLATE_PROCESS_HELPER_H_

#include <string>
#include <vector>
namespace ohos::adapter::multiprocess {

class IsolateProcessHelper {
 public:
  static int32_t StartChildProcess(const std::vector<std::string>& command,
                                   const std::vector<std::pair<int, int>>& fds,
                                   const std::string& entry_point = "",
                                   bool is_isolated_process = false);
};
}  // namespace ohos::adapter::multiprocess

#endif  // OHOS_ADAPTER_MULTIPROCESS_ISOLATE_PROCESS_ISOLATE_PROCESS_HELPER_H_

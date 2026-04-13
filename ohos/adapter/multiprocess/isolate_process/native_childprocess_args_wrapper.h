// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_MULTIPROCESS_ISOLATE_PROCESS_NATIVECHILDPROCESSARGSWRAPPER_H_
#define OHOS_ADAPTER_MULTIPROCESS_ISOLATE_PROCESS_NATIVECHILDPROCESSARGSWRAPPER_H_

#include <string>
#include <vector>
#include "ohos/adapter/multiprocess/isolate_process/native_child_process_library.h"

namespace ohos::adapter::multiprocess {
class NativeChildProcessArgsWrapper {
 public:
  NativeChildProcessArgsWrapper() = default;

  static NativeChildProcess_Args Build(
      const std::string& params,
      const std::vector<std::pair<int32_t, int32_t>>& fds);

  static void Parse(NativeChildProcess_Args orgs,
                    std::vector<std::string>& commands,
                    std::vector<std::pair<int32_t, int32_t>>& fds);

  static void Free(NativeChildProcess_Args& orgs);
};
}  // namespace ohos::adapter::multiprocess

#endif  // OHOS_ADAPTER_MULTIPROCESS_ISOLATE_PROCESS_NATIVECHILDPROCESSARGSWRAPPER_H_

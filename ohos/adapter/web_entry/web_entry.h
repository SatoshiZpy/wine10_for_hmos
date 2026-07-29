// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_WEB_ENTRY_H_
#define OHOS_ADAPTER_WEB_ENTRY_H_

#include <string>
#include <vector>

namespace ohos::adapter::web_entry {

void RunBrowser(const std::vector<std::string>& args);
int RunOtherProcessType(int process_type);
int RunIsolateProcessType(int process_type, const std::vector<std::string>& args);
void RunWine(int process_type,
             const std::vector<std::string>& args,
             const std::vector<std::string>& env);
int RunWineProcess(const std::vector<std::string>& env_args,
                   const std::string& ipc_socket_path);

}  // namespace ohos::adapter::web_entry
#endif  // OHOS_ADAPTER_WEB_ENTRY_H_

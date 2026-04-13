// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/web_entry/web_loader.h"

namespace ohos::adapter::web_entry {

class ContentShellLoader : public BasicLoader {
 public:
  ContentShellLoader(const std::string& dir, const std::string& name)
      : BasicLoader(dir, name) {}

  std::string GetEntryPoint(int process_type) override;
  const std::vector<std::string> GetEntryArgs() override;
};

}  // namespace ohos::adapter::web_entry

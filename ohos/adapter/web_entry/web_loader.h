// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_WEB_ENTRY_WEB_LOADER_H_
#define OHOS_ADAPTER_WEB_ENTRY_WEB_LOADER_H_

#include <memory>
#include <string>
#include <vector>

#include "ohos/adapter/common/constants.h"
#include "ohos/adapter/common/shared_library.h"

namespace ohos::adapter::web_entry {

using OnLoaderCallback = std::function<void(int error_code)>;

enum WebLoaderType {
  kContent_shell = 0,
  kChrome,
  kWine,
};

class BasicLoader {
 public:
  BasicLoader(const BasicLoader&) = delete;
  BasicLoader& operator=(const BasicLoader&) = delete;
  BasicLoader(const std::string& dir, const std::string& name);

  virtual ~BasicLoader() {}

  // A factory method that can create any of the derived types of BasicLoader
  // (ContentShellLoader and ChromeMainLoader ).
  static BasicLoader* Create(int loaderType);

  static BasicLoader* Create(int loaderType, int process_type);

  bool Initialize();

  int StartMainProcess(int process_type, const std::vector<std::string>& args);
  int StartChildProcess(int process_type, int argc, const char** argv);

  virtual std::string GetEntryPoint(int process_type) = 0;
  virtual const std::vector<std::string> GetEntryArgs() = 0;

 protected:
  common::SharedLibrary shared_library_;
};
}  // namespace ohos::adapter::web_entry
#endif  // OHOS_ADAPTER_WEB_ENTRY_WEB_LOADER_H_

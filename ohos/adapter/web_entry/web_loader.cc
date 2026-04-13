// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/web_entry/web_loader.h"
#include "ohos/adapter/common/logging.h"

namespace ohos::adapter::web_entry {

BasicLoader::BasicLoader(const std::string& dir, const std::string& name)
    : shared_library_(dir, name) {}

bool BasicLoader::Initialize() { return true; }

__attribute__((no_sanitize("cfi", "cfi-icall")))
int BasicLoader::StartMainProcess(const std::vector<std::string>& args) {
  if (!shared_library_.IsLoaded()) {
    LOGE("StartMainProcess shared_library_ load failed.");
    return -1;
  }

  auto entry = GetEntryPoint(ProcessType::kMainProcess);
  if (entry.empty()) {
    LOGE("Get Entry Point failed.");
    return -1;
  }
  auto fn = shared_library_.GetFunction<int(int, const char**)>(entry.c_str());
  if (fn != nullptr) {
    auto argv_origin = GetEntryArgs();
    std::vector<const char*> argv_cstr;
    for (const auto& arg : argv_origin) {
      argv_cstr.push_back(const_cast<char*>(arg.c_str()));
    }
    for (const auto& arg : args) {
      argv_cstr.push_back(const_cast<char*>(arg.c_str()));
    }
    return fn(argv_cstr.size(), argv_cstr.data());
  }
  LOGE("Start Main Process failed.");
  return -1;
}

__attribute__((no_sanitize("cfi", "cfi-icall")))
int BasicLoader::StartChildProcess(int process_type,
                                   int argc, const char** argv) {
  if (!shared_library_.IsLoaded()) {
    LOGE("StartChildProcess shared_library_ load failed.");
    return -1;
  }

  auto entry = GetEntryPoint(process_type);
  if (entry.empty()) {
    LOGE("Get Entry Point failed.");
    return -1;
  }

  auto fn = shared_library_.GetFunction<int(int, const char**)>(entry.c_str());
  if (fn != nullptr) {
    return fn(argc, argv);
  }
  LOGE("Run Child Process failed.");
  return -1;
}

}  // namespace ohos::adapter::web_entry

// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/context_path/context_path_adapter.h"

#include <functional>
#include "ohos/adapter/aki_hook/aki_hook.h"
#include "ohos/adapter/common/logging.h"

namespace ohos::adapter::ContextPathAdapter {
std::string GetDir(std::string& getDirFuncName) {
  if (auto getDirFunc = ohos::adapter::GetJSFunction(getDirFuncName)) {
    auto path = getDirFunc->Invoke<std::string>();
    return path;
  }
  LOGE("ContextPathAdapter get %{public}s js function failed.",
       getDirFuncName.c_str());
  return "";
}

std::string GetCacheDir() {
  std::string funcName("ContextPathAdapter.GetCacheDir");
  return GetDir(funcName);
}

std::string GetFilesDir() {
  std::string funcName("ContextPathAdapter.GetFilesDir");
  return GetDir(funcName);
}

std::string GetTempDir() {
  std::string funcName("ContextPathAdapter.GetTempDir");
  return GetDir(funcName);
}

std::string GetUserDocumentDir() {
  std::string funcName("ContextPathAdapter.GetUserDocumentDir");
  return GetDir(funcName);
}

std::string GetUserDesktopDir() {
  std::string funcName("ContextPathAdapter.GetUserDesktopDir");
  return GetDir(funcName);
}

std::string GetUserDownloadDir() {
  std::string funcName("ContextPathAdapter.GetUserDownloadDir");
  return GetDir(funcName);
}

std::string GetAppDownloadDir() {
  if (auto getDirFunc = ohos::adapter::GetJSFunction(
      "ContextPathAdapter.GetAppDownloadDir")) {
    auto promise = std::make_shared<std::promise<std::string>>();
    auto future = promise->get_future();
    std::function<void(std::string)> download_dir = [promise](std::string ret) {
      promise->set_value(ret);
    };

    getDirFunc->Invoke<void>(download_dir);
    auto status = future.wait_for(std::chrono::seconds(1));
    if (status == std::future_status::timeout) {
      LOGE("GetAppDownloadDir timed out.");
      return "";
    } else {
      return future.get();
    }
  }
  LOGE(
      "ContextPathAdapter get ContextPathAdapter.GetAppDownloadDir js "
      "function failed.");
  return "";
}
}  // namespace ohos::adapter::ContextPathAdapter

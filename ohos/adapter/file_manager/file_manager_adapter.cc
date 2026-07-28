// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/file_manager/file_manager_adapter.h"

#include <filemanagement/file_uri/error_code.h>
#include <filemanagement/file_uri/oh_file_uri.h>

#include "ohos/adapter/aki_hook/aki_hook.h"

namespace ohos::adapter {

FileManagerAdapter& FileManagerAdapter::GetInstance() {
  static FileManagerAdapter instance;
  return instance;
}

void FileManagerAdapter::OpenItemInFolder(const std::string& full_path) {
  auto func =
    ohos::adapter::GetJSFunction("FileManagerAdapter.OpenItemInFolder");
  if (func) {
    func->Invoke<void>(full_path);
  }
}

void FileManagerAdapter::OpenVerifiedItem(const std::string& full_path) {
  auto func =
    ohos::adapter::GetJSFunction("FileManagerAdapter.OpenVerifiedItem");
  if (func) {
    func->Invoke<void>(full_path);
  }
}

void FileManagerAdapter::OpenVerifiedItemWithChooser(const std::string& full_path) {
  auto func =
    ohos::adapter::GetJSFunction("FileManagerAdapter.OpenVerifiedItemWithChooser");
  if (func) {
    func->Invoke<void>(full_path);
  }
}

void FileManagerAdapter::GetPathForUri(const char* uri, std::string& path) {
  if (uri == nullptr) {
    LOGE("[FileManagerAdapter] GetPathForUri fail, uri is null");
    return;
  }
  unsigned int uri_length = strlen(uri);
  char* temp = nullptr;
  FileManagement_ErrCode ret =
      OH_FileUri_GetPathFromUri(uri, uri_length, &temp);
  if (ret != ERR_OK) {
    LOGE("[FileManagerAdapter] GetPathForUri fail, ret=%{public}d, uri=%{public}s", ret, uri);
  } else {
    path = temp;
    std::free(temp);
    temp = nullptr;
  }
}
}  // namespace ohos

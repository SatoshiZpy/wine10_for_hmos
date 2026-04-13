// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/mime_type/mime_type_adapter.h"

#include "ohos/adapter/aki_hook/aki_hook.h"

namespace ohos::adapter {

// static
std::vector<std::string> MimeTypeAdapter::GetMimeTypeByExtension(
    const std::string& extension) {
  auto func =
      ohos::adapter::GetJSFunction("MimeTypeAdapter.GetMimeTypeByExtension");
  if (func) {
    return func->Invoke<std::vector<std::string>>(extension);
  } else {
    LOGE("[MimeTypeAdapter] GetMimeTypeByExtension ets function not found");
  }
  return std::vector<std::string>();
}

// static
std::vector<std::string> MimeTypeAdapter::GetExtensionByMimeType(
    const std::string& mime_type) {
  auto func =
      ohos::adapter::GetJSFunction("MimeTypeAdapter.GetExtensionByMimeType");
  if (func) {
    return func->Invoke<std::vector<std::string>>(mime_type);
  } else {
    LOGE("[MimeTypeAdapter] GetExtensionByMimeType ets function not found");
  }
  return std::vector<std::string>();
}

}  // namespace ohos::adapter

// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_MIME_TYPE_MIME_TYPE_ADAPTER_H_
#define OHOS_ADAPTER_MIME_TYPE_MIME_TYPE_ADAPTER_H_

#include <string>
#include <vector>

#include "ohos/adapter/export.h"

namespace ohos::adapter {

class ADAPTER_EXPORT_API MimeTypeAdapter {
 public:
  static std::vector<std::string> GetMimeTypeByExtension(
      const std::string& extension);
  static std::vector<std::string> GetExtensionByMimeType(
      const std::string& mime_type);

 private:
  MimeTypeAdapter() = default;
  ~MimeTypeAdapter() = default;
};
}  // namespace ohos::adapter
using MimeTypeOhos = ohos::adapter::MimeTypeAdapter;

#endif  // OHOS_ADAPTER_MIME_TYPE_MIME_TYPE_ADAPTER_H_

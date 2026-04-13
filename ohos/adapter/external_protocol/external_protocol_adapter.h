// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_EXTERNAL_PROTOCOL_EXTERNAL_PROTOCOL_ADAPTER_H_
#define OHOS_ADAPTER_EXTERNAL_PROTOCOL_EXTERNAL_PROTOCOL_ADAPTER_H_

#include <string>

#include "ohos/adapter/export.h"

namespace ohos::adapter {

class ADAPTER_EXPORT_API ExternalProtocolAdapter {
 public:
  static ExternalProtocolAdapter& GetInstance();

  ExternalProtocolAdapter(const ExternalProtocolAdapter&) = delete;
  ExternalProtocolAdapter& operator=(const ExternalProtocolAdapter&) = delete;

  ~ExternalProtocolAdapter() = default;

  void OpenExternal(const std::string& url_str);

 private:
  ExternalProtocolAdapter() = default;
};

}  // namespace ohos

#endif  // OHOS_ADAPTER_EXTERNAL_PROTOCOL_EXTERNAL_PROTOCOL_ADAPTER_H_

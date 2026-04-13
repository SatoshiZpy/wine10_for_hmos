// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_GEOLOCATION_LOCATION_ADAPTER_H
#define OHOS_ADAPTER_GEOLOCATION_LOCATION_ADAPTER_H

#include <sys/types.h>
#include <cstdint>
#include <memory>

#include "location_info.h"
#include "ohos/adapter/export.h"

namespace ohos::adapter {
class ADAPTER_EXPORT_API LocationCallbackAdapter {
 public:
  LocationCallbackAdapter() = default;
  virtual ~LocationCallbackAdapter() = default;
  virtual void OnLocationReport(
      const std::unique_ptr<ohos::adapter::LocationInfo>& location) = 0;
  virtual void OnErrorReport(const int errorCode) = 0;
};
}  // namespace ohos::adapter
#endif

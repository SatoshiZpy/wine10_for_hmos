// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_GEOLOCATION_LOCATION_INFO_H
#define OHOS_ADAPTER_GEOLOCATION_LOCATION_INFO_H

#include <sys/types.h>
#include <cstdint>
#include <memory>
#include <string>

#include "ohos/adapter/aki_hook/aki_hook.h"
#include "ohos/adapter/export.h"

namespace ohos::adapter {
class ADAPTER_EXPORT_API LocationInfo {
 public:
  explicit LocationInfo(aki::Value location);
  ~LocationInfo() = default;

  double latitude;
  double longitude;
  double altitude;
  double accuracy;
  double speed;
};
}  // namespace ohos::adapter

#endif

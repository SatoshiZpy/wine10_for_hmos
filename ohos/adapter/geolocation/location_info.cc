// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/geolocation/location_info.h"

#include "ohos/adapter/aki_hook/aki_hook.h"

namespace ohos::adapter {
LocationInfo::LocationInfo(aki::Value location) {
  longitude = location["longitude"].As<double>();
  latitude = location["latitude"].As<double>();
  altitude = location["altitude"].As<double>();
  accuracy = location["accuracy"].As<double>();
  speed = location["speed"].As<double>();
}
}  // namespace ohos::adapter

// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_GEOLOCATION_LOCATION_PROXY_ADAPTER_IMPL_H
#define OHOS_ADAPTER_GEOLOCATION_LOCATION_PROXY_ADAPTER_IMPL_H

#include <map>

#include "location_adapter.h"
#include "ohos/adapter/export.h"

namespace ohos::adapter {
class ADAPTER_EXPORT_API LocationProxyAdapter {
 public:
  LocationProxyAdapter() {}
  virtual ~LocationProxyAdapter() = default;
  bool StartListening(std::shared_ptr<LocationCallbackAdapter> callback,
                      bool high_accuracy);
  bool StopListening(std::shared_ptr<LocationCallbackAdapter> callback);
};
}  // namespace ohos::adapter

#endif  // OHOS_ADAPTER_GEOLOCATION_LOCATION_PROXY_ADAPTER_IMPL_H

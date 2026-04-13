// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "connection_adapter.h"
#include "ohos/adapter/aki_hook/aki_hook.h"

namespace ohos::adapter::net {
NetConnectionType ConvertBearTypeToNetConnType(const NetBearType& bearType) {
  if (bearType == NetBearType::BEARER_CELLULAR)
    return NetConnectionType::CONNECTION_4G;
  if (bearType == NetBearType::BEARER_WIFI)
    return NetConnectionType::CONNECTION_WIFI;
  if (bearType == NetBearType::BEARER_ETHERNET)
    return NetConnectionType::CONNECTION_ETHERNET;
  return NetConnectionType::CONNECTION_UNKNOWN;
}

} // namespace ohos::adapter::net

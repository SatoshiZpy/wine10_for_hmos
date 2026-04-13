// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_NET_CONNECTION_ADAPTER_H
#define OHOS_ADAPTER_NET_CONNECTION_ADAPTER_H

#include <memory>
#include <string>
#include <napi/native_api.h>
#include "ohos/adapter/export.h"

namespace ohos::adapter::net {
enum class ADAPTER_EXPORT_API NetBearType {
  BEARER_CELLULAR = 0,
  BEARER_WIFI = 1,
  BEARER_ETHERNET = 3
};

enum class ADAPTER_EXPORT_API NetConnectionType {
  CONNECTION_UNKNOWN = 0,
  CONNECTION_ETHERNET = 1,
  CONNECTION_WIFI = 2,
  CONNECTION_2G = 3,
  CONNECTION_3G = 4,
  CONNECTION_4G = 5,
  CONNECTION_NONE = 6,
  CONNECTION_BLUETOOTH = 7,
  CONNECTION_5G = 8,
  CONNECTION_LAST = CONNECTION_5G
};

enum class ADAPTER_EXPORT_API NetConnectionSubtype {
  SUBTYPE_UNKNOWN = 0,
  SUBTYPE_NONE,
  SUBTYPE_OTHER,
  SUBTYPE_GSM,
  SUBTYPE_IDEN,
  SUBTYPE_CDMA,
  SUBTYPE_1XRTT,
  SUBTYPE_GPRS,
  SUBTYPE_EDGE,
  SUBTYPE_UMTS,
  SUBTYPE_EVDO_REV_0,
  SUBTYPE_EVDO_REV_A,
  SUBTYPE_HSPA,
  SUBTYPE_EVDO_REV_B,
  SUBTYPE_HSDPA,
  SUBTYPE_HSUPA,
  SUBTYPE_EHRPD,
  SUBTYPE_HSPAP,
  SUBTYPE_LTE,
  SUBTYPE_LTE_ADVANCED,
  SUBTYPE_BLUETOOTH_1_2,
  SUBTYPE_BLUETOOTH_2_1,
  SUBTYPE_BLUETOOTH_3_0,
  SUBTYPE_BLUETOOTH_4_0,
  SUBTYPE_ETHERNET,
  SUBTYPE_FAST_ETHERNET,
  SUBTYPE_GIGABIT_ETHERNET,
  SUBTYPE_10_GIGABIT_ETHERNET,
  SUBTYPE_WIFI_B,
  SUBTYPE_WIFI_G,
  SUBTYPE_WIFI_N,
  SUBTYPE_WIFI_AC,
  SUBTYPE_WIFI_AD,
  SUBTYPE_LAST = SUBTYPE_WIFI_AD
};

class ADAPTER_EXPORT_API NetConnectionCallback {
};

template <typename nativeType, typename OhosType>
nativeType ConvertOhosConnTypeToNativeConnType(const OhosType& type) {
  return static_cast<nativeType>(type);
}
} // namespace ohos::adapter::net

#endif  // OHOS_ADAPTER_NET_CONNECTION_ADAPTER_H

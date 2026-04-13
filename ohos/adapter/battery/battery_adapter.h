// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_BATTERY_BATTERY_ADAPTER_H_
#define OHOS_ADAPTER_BATTERY_BATTERY_ADAPTER_H_

#include <atomic>
#include <string>

#include "ohos/adapter/aki_hook/aki_hook.h"
#include "ohos/adapter/export.h"

namespace ohos::adapter::battery {

static constexpr int32_t INVALID_BATT_INT_VALUE = -1;
static constexpr bool INVALID_BATT_BOOL_VALUE = true;

enum class BatteryChargeState : uint32_t {
  CHARGE_STATE_NONE,     // Battery is discharge.
  CHARGE_STATE_ENABLE,   // Battery is charging.
  CHARGE_STATE_DISABLE,  // Battery is not charging.
  CHARGE_STATE_FULL,     // Battery charge full.
  CHARGE_STATE_BUTT      // The bottom of the enum.
};

struct BatteryInfo {
  int32_t battery_soc = INVALID_BATT_INT_VALUE;
  BatteryChargeState charging_status = BatteryChargeState::CHARGE_STATE_BUTT;
  bool is_battery_present = INVALID_BATT_BOOL_VALUE;
  int32_t estimated_remaining_charge_time = INVALID_BATT_INT_VALUE;
  int32_t now_current = INVALID_BATT_INT_VALUE;
  int32_t remaining_energy = INVALID_BATT_INT_VALUE;
};

class ADAPTER_EXPORT_API BatteryUpdateCallback
    : public std::enable_shared_from_this<BatteryUpdateCallback> {
 public:
  BatteryUpdateCallback() = default;
  virtual ~BatteryUpdateCallback() = default;
  virtual void OnBatteryStatusChanged(const BatteryInfo& battery_info) = 0;
};

class ADAPTER_EXPORT_API BatteryAdapter {
 public:
  static BatteryAdapter& GetInstance();
  void ConvertBatteryInfo(aki::Value battery_info,
                          BatteryInfo& ohos_battery_info);
  bool GetBatteryInfo(BatteryInfo& battery_info_ohos);
  bool RegisterBatteryUpdateCallback(
      std::shared_ptr<BatteryUpdateCallback> battery_update_callback);
  void UnregisterBatteryUpdateCallback();

 private:
  BatteryAdapter() = default;
  ~BatteryAdapter() = default;
  BatteryAdapter(const BatteryAdapter&) = delete;
  BatteryAdapter& operator=(const BatteryAdapter&) = delete;

  std::atomic_bool is_active_ = false;
};

}  // namespace ohos::adapter::battery

using BatteryInfo = ohos::adapter::battery::BatteryInfo;

#endif  // OHOS_ADAPTER_BATTERY_BATTERY_ADAPTER_H_

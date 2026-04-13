// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/battery/battery_adapter.h"

#include <hilog/log.h>
#include "ohos/adapter/aki_hook/aki_hook.h"
#include "ohos/adapter/common/logging.h"

namespace ohos::adapter::battery {

BatteryAdapter& BatteryAdapter::GetInstance() {
  static BatteryAdapter instance;
  return instance;
}

template <class T>
T GetValue(aki::Value aki_value, T default_value) {
  if (!(aki_value.IsNull() || aki_value.IsUndefined())) {
    return aki_value.As<T>();
  }
  return default_value;
}

void BatteryAdapter::ConvertBatteryInfo(aki::Value battery_info,
                                        BatteryInfo& ohos_battery_info) {
  ohos_battery_info.battery_soc =
      GetValue<int32_t>(battery_info["batterySOC"], INVALID_BATT_INT_VALUE);
  ohos_battery_info.charging_status = GetValue<BatteryChargeState>(
      battery_info["chargingStatus"], BatteryChargeState::CHARGE_STATE_BUTT);
  ohos_battery_info.is_battery_present =
      GetValue<bool>(battery_info["isBatteryPresent"], INVALID_BATT_BOOL_VALUE);
  ohos_battery_info.estimated_remaining_charge_time = GetValue<int32_t>(
      battery_info["estimatedRemainingChargeTime"], INVALID_BATT_INT_VALUE);
  ohos_battery_info.now_current =
      GetValue<int32_t>(battery_info["nowCurrent"], INVALID_BATT_INT_VALUE);
  ohos_battery_info.remaining_energy = GetValue<int32_t>(
      battery_info["remainingEnergy"], INVALID_BATT_INT_VALUE);
}

bool BatteryAdapter::GetBatteryInfo(BatteryInfo& battery_info_ohos) {
  bool result = false;
  std::function<void(aki::Value)> callback = [&](aki::Value battery_info) {
    ConvertBatteryInfo(battery_info, battery_info_ohos);
  };

  auto func = ohos::adapter::GetJSFunction("BatteryAdapter.GetBatteryInfo");
  if (func) {
    result = func->Invoke<bool>(callback);
  } else {
    LOGE("Failed to get BatteryAdapter.GetBatteryInfo.");
  }
  return result;
}

bool BatteryAdapter::RegisterBatteryUpdateCallback(
    std::shared_ptr<BatteryUpdateCallback> battery_update_callback) {
  auto battery_update_func = ohos::adapter::GetJSFunction(
      "BatteryAdapter.RegisterBatteryUpdateCallback");
  if (battery_update_func) {
    is_active_.store(true);
    std::function<void(aki::Value)> callback = [=](aki::Value battery_info) {
      BatteryInfo battery_info_ohos;
      ConvertBatteryInfo(battery_info, battery_info_ohos);
      if (is_active_.load() && battery_update_callback) {
        battery_update_callback->OnBatteryStatusChanged(battery_info_ohos);
      }
    };
    return battery_update_func->Invoke<bool>(callback);
  }
  return false;
}

void BatteryAdapter::UnregisterBatteryUpdateCallback() {
  is_active_.store(false);
  auto unregister_battery_update_callback = ohos::adapter::GetJSFunction(
      "BatteryAdapter.UnregisterBatteryStatusCallback");
  if (unregister_battery_update_callback) {
    unregister_battery_update_callback->Invoke<void>();
  }
}

}  // namespace ohos::adapter::battery

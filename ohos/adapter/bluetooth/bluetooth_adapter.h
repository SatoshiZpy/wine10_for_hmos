// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_BLUETOOTH_BLUETOOTH_ADAPTER_H_
#define OHOS_ADAPTER_BLUETOOTH_BLUETOOTH_ADAPTER_H_

#include <functional>
#include <string>
#include <vector>
#include "ohos/adapter/aki_hook/aki_hook.h"
#include "ohos/adapter/export.h"

namespace ohos::adapter::device {
enum BluetoothState {
  STATE_ERROR = -1,
  STATE_OFF = 0,
  STATE_TURNING_ON = 1,
  STATE_ON = 2,
  STATE_TURNING_OFF = 3,
  STATE_BLE_TURNING_ON = 4,
  STATE_BLE_ON = 5,
  STATE_BLE_TURNING_OFF = 6
};

enum ScanMode {
  SCAN_MODE_NONE = 0,
  SCAN_MODE_CONNECTABLE = 1,
  SCAN_MODE_GENERAL_DISCOVERABLE = 2,
  SCAN_MODE_LIMITED_DISCOVERABLE = 3,
  SCAN_MODE_CONNECTABLE_GENERAL_DISCOVERABLE = 4,
  SCAN_MODE_CONNECTABLE_LIMITED_DISCOVERABLE = 5
};

enum BondState {
  BOND_STATE_INVALID = 0,
  BOND_STATE_BONDING = 1,
  BOND_STATE_BONDED = 2
};

class ADAPTER_EXPORT_API BluetoothAdapter {
 public:
  static bool RequestOsPermission();
  static bool IsOsGranted();
  static std::string GetAdapterName();
  static bool SetAdapterName(const std::string& name);
  static int32_t GetBluetoothState();
  static bool EnableBluetooth();
  static bool DisableBluetooth();
  static void StartBluetoothStateMonitor(std::function<void(int32_t)> state_cb);
  static void StopBluetoothStateMonitor();
  static int32_t GetBluetoothScanMode();
  static void SetBluetoothScanMode(int32_t mode, int32_t duration);
  static void StartDiscovery();
  static void StopDiscovery();
  static void StartDiscoveryMonitor(
      std::function<void(std::vector<std::string>)> get_devices_cb);
  static void StopDiscoveryMonitor();
  static std::string GetRemoteDeviceName(const std::string& device_id);
  static int32_t GetRemoteDeviceClass(const std::string& device_id);
  static void PairDevice(const std::string& device_id,
                         std::function<void(bool)> pair_callback);
  static std::vector<std::string> GetPairedDevices();
  static bool SetDevicePinCode(const std::string& device_id,
                               const std::string& pin_code,
                               std::function<void(bool)> set_pin_code_cb);
  static bool IsDiscovering();
  static BondState GetPairedState(const std::string& device_id);

 private:
  BluetoothAdapter() = default;
  virtual ~BluetoothAdapter() = default;
};

}  // namespace ohos::adapter::device
using BluetoothOhos = ohos::adapter::device::BluetoothAdapter;
#endif  // OHOS_ADAPTER_BLUETOOTH_BLUETOOTH_ADAPTER_H_

// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_BLUETOOTH_BLUETOOTH_LOW_ENERGY_ADAPTER_H_
#define OHOS_ADAPTER_BLUETOOTH_BLUETOOTH_LOW_ENERGY_ADAPTER_H_

#include <functional>
#include <map>
#include <string>
#include <vector>
#include "ohos/adapter/export.h"

namespace ohos::adapter::device {

struct ScanResult {
  std::string device_id;
  int32_t rssi;
  uint8_t* data;
  uint32_t data_length;
  std::string device_name;
  bool connectable;
};

struct AdvertisingParams {
  bool connectable;
  std::vector<std::string> service_uuids;
  std::map<uint16_t, std::vector<uint8_t>> manufacturer_data;
  std::map<std::string, std::vector<uint8_t>> service_data;
  std::map<uint8_t, std::vector<uint8_t>> scan_response_data;
};

enum ProfileConnectionState {
  STATE_DISCONNECTED = 0,
  STATE_CONNECTING = 1,
  STATE_CONNECTED = 2,
  STATE_DISCONNECTING = 3,
};

enum AdvertisingState {
  TARTED = 1,
  ENABLED = 2,
  DISABLED = 3,
  STOPPED = 4,
};

class ADAPTER_EXPORT_API BluetoothLowEnergyAdapter {
 public:
  static void StartDiscovery();
  static void StopDiscovery();
  static void StartDiscoveryMonitor(
      std::function<void(ScanResult)> get_devices_cb);
  static void StopDiscoveryMonitor();
  static void CreateGattClientDevice(
      const std::string& device_id,
      std::function<void(uint8_t)> get_connection_state);
  static void DisConnectGatt(const std::string& device_id);
  static void StartAdvertising(const AdvertisingParams& params,
                               std::function<void(int32_t)> get_advertising_id);
  static void StopAdvertising(int32_t advertising_id);
  static void OnAdvertisingStateChange(
      std::function<void(int32_t, uint8_t)> on_advertising_callback);
  static void OffAdvertisingStateChange();

 private:
  BluetoothLowEnergyAdapter() = default;
  virtual ~BluetoothLowEnergyAdapter() = default;
};

}  // namespace ohos::adapter::device
#endif  // OHOS_ADAPTER_BLUETOOTH_BLUETOOTH_LOW_ENERGY_ADAPTER_H_

// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/bluetooth/bluetooth_low_energy_adapter.h"
#include "ohos/adapter/bluetooth/bluetooth_adapter.h"

#include "ohos/adapter/aki_hook/aki_hook.h"
#include "aki/value/array_buffer.h"
#include "ohos/adapter/common/logging.h"

using BluetoothAdapter = ohos::adapter::device::BluetoothAdapter;

namespace ohos::adapter::device {

namespace {
void ConvertScanResult(aki::ArrayBuffer buffer,
                       aki::Value aki_result,
                       ScanResult& ohos_scan_result) {
  if (!(aki_result.IsNull() || aki_result.IsUndefined())) {
    ohos_scan_result.device_id = aki_result["deviceId"].As<std::string>();
    ohos_scan_result.rssi = aki_result["rssi"].As<int32_t>();
    ohos_scan_result.device_name = aki_result["deviceName"].As<std::string>();
    ohos_scan_result.connectable = aki_result["connectable"].As<bool>();
    if (buffer.GetData()) {
      ohos_scan_result.data = reinterpret_cast<uint8_t*>(buffer.GetData());
      ohos_scan_result.data_length = buffer.GetLength();
    }
  }
}
}  // namespace

// static
void BluetoothLowEnergyAdapter::StartDiscovery() {
  if (BluetoothAdapter::IsOsGranted()) {
    if (auto start_discovery = ohos::adapter::GetJSFunction(
        "BluetoothLowEnergyAdapter.StartBluetoothDiscovery")) {
      start_discovery->Invoke<void>();
    }
  }
}

// static
void BluetoothLowEnergyAdapter::StopDiscovery() {
  if (BluetoothAdapter::IsOsGranted()) {
    if (auto stop_discovery = ohos::adapter::GetJSFunction(
        "BluetoothLowEnergyAdapter.StopBluetoothDiscovery")) {
      stop_discovery->Invoke<void>();
    }
  }
}

void BluetoothLowEnergyAdapter::StartDiscoveryMonitor(
    std::function<void(ScanResult)> get_devices_cb) {
  if (BluetoothAdapter::IsOsGranted()) {
    if (auto start_discovery_monitor = ohos::adapter::GetJSFunction(
        "BluetoothLowEnergyAdapter.StartDiscoveryMonitor")) {
      std::function<void(aki::ArrayBuffer, aki::Value)> on_scan_receive =
          [=](aki::ArrayBuffer buffer, aki::Value endpoints) {
            ScanResult ohos_scan_results;
            ConvertScanResult(buffer, endpoints, ohos_scan_results);
            get_devices_cb(ohos_scan_results);
          };
      start_discovery_monitor->Invoke<void>(on_scan_receive);
    }
  }
}

// static
void BluetoothLowEnergyAdapter::StopDiscoveryMonitor() {
  if (BluetoothAdapter::IsOsGranted()) {
    if (auto stop_discovery_monitor = ohos::adapter::GetJSFunction(
        "BluetoothLowEnergyAdapter.StopDiscoveryMonitor")) {
      stop_discovery_monitor->Invoke<void>();
    }
  }
}

// static
void BluetoothLowEnergyAdapter::CreateGattClientDevice(
    const std::string& device_id,
    std::function<void(uint8_t)> get_connection_state) {
  if (BluetoothAdapter::IsOsGranted()) {
    if (auto create_gatt_client_device = ohos::adapter::GetJSFunction(
        "BluetoothLowEnergyAdapter.CreateGattClientDevice")) {
      create_gatt_client_device->Invoke<void>(device_id, get_connection_state);
    }
  }
}

// static
void BluetoothLowEnergyAdapter::DisConnectGatt(const std::string& device_id) {
  if (BluetoothAdapter::IsOsGranted()) {
    if (auto dis_connect_gatt = ohos::adapter::GetJSFunction(
        "BluetoothLowEnergyAdapter.DisConnectGatt")) {
      dis_connect_gatt->Invoke<void>(device_id);
    }
  }
}

// static
void BluetoothLowEnergyAdapter::StartAdvertising(
    const AdvertisingParams& params,
    std::function<void(int32_t)> get_advertising_id) {
  if (BluetoothAdapter::IsOsGranted()) {
    if (auto start_advertising = ohos::adapter::GetJSFunction(
        "BluetoothLowEnergyAdapter.StartAdvertising")) {
      start_advertising->Invoke<void>(params, get_advertising_id);
    }
  };
}

// static
void BluetoothLowEnergyAdapter::StopAdvertising(int32_t advertising_id) {
  if (BluetoothAdapter::IsOsGranted()) {
    if (auto stop_advertising = ohos::adapter::GetJSFunction(
        "BluetoothLowEnergyAdapter.StopAdvertising")) {
      stop_advertising->Invoke<void>(advertising_id);
    }
  }
}

// static
void BluetoothLowEnergyAdapter::OnAdvertisingStateChange(
    std::function<void(int32_t, uint8_t)> on_advertising_callback) {
  if (!BluetoothAdapter::IsOsGranted()) {
    LOGW(
        "BluetoothLowEnergyAdapter OnAdvertisingStateChange warn: os not "
        "granted.");
    return;
  }
  if (auto on_advertising_state_change = ohos::adapter::GetJSFunction(
      "BluetoothLowEnergyAdapter.OnAdvertisingStateChange")) {
    std::function<void(aki::Value)> on_advertising_state_callback =
        [=](aki::Value stateInfo) {
          if (stateInfo.IsNull() || stateInfo.IsUndefined()) {
            return;
          }
          if (stateInfo["advertisingId"].IsNumber() &&
              stateInfo["state"].IsNumber()) {
            on_advertising_callback(stateInfo["advertisingId"].As<int32_t>(),
                                    stateInfo["state"].As<uint8_t>());
          }
        };
    on_advertising_state_change->Invoke<void>(on_advertising_state_callback);
  }
}

// static
void BluetoothLowEnergyAdapter::OffAdvertisingStateChange() {
  if (BluetoothAdapter::IsOsGranted()) {
    if (auto off_advertising_state_change = ohos::adapter::GetJSFunction(
        "BluetoothLowEnergyAdapter.OffAdvertisingStateChange")) {
      off_advertising_state_change->Invoke<void>();
    }
  }
}

}  // namespace ohos::adapter::device

// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/bluetooth/bluetooth_adapter.h"

#include <string>

#include "ohos/adapter/aki_hook/aki_hook.h"
#include "ohos/adapter/common/logging.h"
#include "ohos/adapter/permission_manager/permission_manager_adapter.h"

namespace ohos::adapter::device {

// static
bool BluetoothAdapter::IsOsGranted() {
  return permission::PermissionManagerAdapter::RequestPermission(
    permission::OHOSPermissionType::BLUETOOTH);
}

// static
std::string BluetoothAdapter::GetAdapterName() {
  std::string adapter_name = "--";
  if (IsOsGranted()) {
    BluetoothState bluetooth_state =
        static_cast<BluetoothState>(GetBluetoothState());
    if (bluetooth_state != STATE_ON && bluetooth_state != STATE_BLE_ON) {
      LOGW(
          "BluetoothAdapter GetAdapterName warn: please turn on bluetooth "
          "first.");
      EnableBluetooth();
    }
    if (auto get_adapter_name =
            ohos::adapter::GetJSFunction("BluetoothAdapter.GetAdapterName")) {
      return (adapter_name = get_adapter_name->Invoke<std::string>());
    }
  }

  return adapter_name;
}

// static
bool BluetoothAdapter::SetAdapterName(const std::string& name) {
  if (IsOsGranted()) {
    if (auto set_adapter_name =
            ohos::adapter::GetJSFunction("BluetoothAdapter.SetAdapterName")) {
      return set_adapter_name->Invoke<bool>(name);
    }
  }

  return false;
}

// static
int32_t BluetoothAdapter::GetBluetoothState() {
  if (IsOsGranted()) {
    if (auto get_bluetooth_state = ohos::adapter::GetJSFunction(
            "BluetoothAdapter.GetBluetoothState")) {
      return get_bluetooth_state->Invoke<int32_t>();
    }
  }

  return -1;
}

// static
bool BluetoothAdapter::EnableBluetooth() {
  if (IsOsGranted()) {
    if (auto enable_bluetooth =
            ohos::adapter::GetJSFunction("BluetoothAdapter.EnableBluetooth")) {
      enable_bluetooth->Invoke<void>();
      return true;
    }
  }

  return false;
}

// static
bool BluetoothAdapter::DisableBluetooth() {
  if (IsOsGranted()) {
    if (auto disable_bluetooth =
            ohos::adapter::GetJSFunction("BluetoothAdapter.DisableBluetooth")) {
      disable_bluetooth->Invoke<void>();
      return true;
    }
  }
  return false;
}

// static
void BluetoothAdapter::StartBluetoothStateMonitor(
    std::function<void(int32_t)> state_cb) {
  if (IsOsGranted()) {
    if (auto start_state_monitor = ohos::adapter::GetJSFunction(
            "BluetoothAdapter.StartBluetoothStateMonitor")) {
      start_state_monitor->Invoke<void>(state_cb);
    }
  }
}

// static
void BluetoothAdapter::StopBluetoothStateMonitor() {
  if (IsOsGranted()) {
    if (auto stop_state_monitor = ohos::adapter::GetJSFunction(
            "BluetoothAdapter.StopBluetoothStateMonitor")) {
      stop_state_monitor->Invoke<void>();
    }
  }
}

// static
int32_t BluetoothAdapter::GetBluetoothScanMode() {
  if (IsOsGranted()) {
    if (auto get_scan_mode = ohos::adapter::GetJSFunction(
            "BluetoothAdapter.GetBluetoothScanMode")) {
      return get_scan_mode->Invoke<int32_t>();
    }
  }
  return -1;
}

// static
void BluetoothAdapter::SetBluetoothScanMode(int32_t mode, int32_t duration) {
  if (IsOsGranted()) {
    if (auto set_scan_mode = ohos::adapter::GetJSFunction(
            "BluetoothAdapter.SetBluetoothScanMode")) {
      set_scan_mode->Invoke<void>(mode, duration);
    }
  }
}

// static
void BluetoothAdapter::StartDiscovery() {
  if (IsOsGranted()) {
    if (auto start_discovery = ohos::adapter::GetJSFunction(
            "BluetoothAdapter.StartBluetoothDiscovery")) {
      start_discovery->Invoke<void>();
    }
  }
}

// static
void BluetoothAdapter::StopDiscovery() {
  if (IsOsGranted()) {
    if (auto stop_discovery = ohos::adapter::GetJSFunction(
            "BluetoothAdapter.StopBluetoothDiscovery")) {
      stop_discovery->Invoke<void>();
    }
  }
}

// static
void BluetoothAdapter::StartDiscoveryMonitor(
    std::function<void(std::vector<std::string>)> get_devices_cb) {
  if (IsOsGranted()) {
    if (auto start_discovery_monitor = ohos::adapter::GetJSFunction(
            "BluetoothAdapter.StartDiscoveryMonitor")) {
      start_discovery_monitor->Invoke<void>(get_devices_cb);
    }
  }
}

// static
void BluetoothAdapter::StopDiscoveryMonitor() {
  if (IsOsGranted()) {
    if (auto stop_discovery_monitor = ohos::adapter::GetJSFunction(
            "BluetoothAdapter.StopDiscoveryMonitor")) {
      stop_discovery_monitor->Invoke<void>();
    }
  }
}

// static
std::string BluetoothAdapter::GetRemoteDeviceName(
    const std::string& device_id) {
  if (auto get_remote_device_name = ohos::adapter::GetJSFunction(
          "BluetoothAdapter.GetRemoteDeviceName")) {
    return get_remote_device_name->Invoke<std::string>(device_id);
  }
  return std::string();
}

// static
int32_t BluetoothAdapter::GetRemoteDeviceClass(const std::string& device_id) {
  if (auto get_remote_device_class = ohos::adapter::GetJSFunction(
          "BluetoothAdapter.GetRemoteDeviceClass")) {
    return get_remote_device_class->Invoke<int32_t>(device_id);
  }
  return 0x1F00u;  // Unclassified
}

// static
void BluetoothAdapter::PairDevice(const std::string& device_id,
                                  std::function<void(bool)> pair_callback) {
  if (auto pair_device =
          ohos::adapter::GetJSFunction("BluetoothAdapter.PairDevice")) {
    pair_device->Invoke<void>(device_id, pair_callback);
  }
}

// static
std::vector<std::string> BluetoothAdapter::GetPairedDevices() {
  if (auto get_paired_devices =
          ohos::adapter::GetJSFunction("BluetoothAdapter.GetPairedDevices")) {
    return get_paired_devices->Invoke<std::vector<std::string>>();
  }
  return std::vector<std::string>();
}

// static
bool BluetoothAdapter::SetDevicePinCode(
    const std::string& device_id,
    const std::string& pin_code,
    std::function<void(bool)> set_pin_code_cb) {
  if (auto set_device_pin_code =
          ohos::adapter::GetJSFunction("BluetoothAdapter.SetDevicePinCode")) {
    return set_device_pin_code->Invoke<bool>(device_id, pin_code,
                                             set_pin_code_cb);
  }
  return false;
}

// static
bool BluetoothAdapter::IsDiscovering() {
  if (auto is_bluetooth_discovering = ohos::adapter::GetJSFunction(
          "BluetoothAdapter.IsBluetoothDiscovering")) {
    return is_bluetooth_discovering->Invoke<bool>();
  }
  return false;
}

// static
BondState BluetoothAdapter::GetPairedState(const std::string& device_id) {
  if (auto get_paired_state =
          ohos::adapter::GetJSFunction("BluetoothAdapter.GetPairedState")) {
    return static_cast<BondState>(get_paired_state->Invoke<int32_t>(device_id));
  }
  return BOND_STATE_INVALID;
}

}  // namespace ohos::adapter::device

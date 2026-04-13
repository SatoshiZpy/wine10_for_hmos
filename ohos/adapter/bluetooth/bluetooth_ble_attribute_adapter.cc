// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/bluetooth/bluetooth_ble_attribute_adapter.h"

#include "ohos/adapter/aki_hook/aki_hook.h"
#include "aki/value/array_buffer.h"
#include "ohos/adapter/bluetooth/bluetooth_adapter.h"

uint32_t GetArrayLength(const aki::Value& array);

namespace ohos::adapter::device {

namespace {
void ConvertServiceInfo(aki::Value aki_results,
                        std::vector<ServiceInfo>& service_infos) {
  for (uint32_t index = 0; index < GetArrayLength(aki_results); index++) {
    aki::Value aki_result = aki_results[index];
    ServiceInfo service_info;
    service_info.identifier = aki_result["identifier"].As<std::string>();
    service_info.device_id = aki_result["device_id"].As<std::string>();
    service_info.service_uuid = aki_result["service_uuid"].As<std::string>();
    service_info.is_primary = aki_result["is_primary"].As<bool>();
    service_infos.push_back(service_info);
  }
}
void ConvertCharacteristicInfo(
    aki::Value aki_results,
    std::vector<CharacteristicInfo>& characteristic_infos) {
  for (uint32_t index = 0; index < GetArrayLength(aki_results); index++) {
    aki::Value aki_result = aki_results[index];
    CharacteristicInfo characteristic_info;
    characteristic_info.identifier = aki_result["identifier"].As<std::string>();
    characteristic_info.device_id = aki_result["device_id"].As<std::string>();
    characteristic_info.service_uuid =
        aki_result["service_uuid"].As<std::string>();
    characteristic_info.characteristic_uuid =
        aki_result["characteristic_uuid"].As<std::string>();
    characteristic_info.properties = aki_result["properties"].As<int32_t>();
    characteristic_infos.push_back(characteristic_info);
  }
}
void ConvertDescriptorInfo(aki::Value aki_results,
                           std::vector<DescriptorInfo>& descriptor_infos) {
  for (uint32_t index = 0; index < GetArrayLength(aki_results); index++) {
    aki::Value aki_result = aki_results[index];
    DescriptorInfo descriptor_info;
    descriptor_info.identifier = aki_result["identifier"].As<std::string>();
    descriptor_info.device_id = aki_result["device_id"].As<std::string>();
    descriptor_info.service_uuid = aki_result["service_uuid"].As<std::string>();
    descriptor_info.characteristic_uuid =
        aki_result["characteristic_uuid"].As<std::string>();
    descriptor_info.descriptor_uuid =
        aki_result["descriptor_uuid"].As<std::string>();
    descriptor_infos.push_back(descriptor_info);
  }
}

}  // namespace

void BluetoothBleAttributeAdapter::StartGattDiscovery(
    const std::string& device_id,
    BluetoothBleAttributeAdapter::CreateServicesCallback callback) {
  if (auto start_gatt_discovery = ohos::adapter::GetJSFunction(
      "BluetoothLowEnergyAdapter.StartGattDiscovery")) {
    std::function<void(aki::Value)> serviceCallBack =
        [=](aki::Value endpoints) {
          std::vector<ServiceInfo> service_infos;
          ConvertServiceInfo(endpoints, service_infos);
          callback(std::move(service_infos));
        };
    start_gatt_discovery->Invoke<void>(device_id, serviceCallBack);
  }
}

void BluetoothBleAttributeAdapter::CreateCharacteristic(
    const std::string& service_id,
    const std::string& device_id,
    BluetoothBleAttributeAdapter::CreateCharacteristicCallback callback) {
  if (auto create_characteristic = ohos::adapter::GetJSFunction(
      "BluetoothLowEnergyAdapter.CreateCharacteristic")) {
    std::function<void(aki::Value)> characteristicCallBack =
        [=](aki::Value endpoints) {
          if (!endpoints.IsNull() && !endpoints.IsUndefined()) {
            std::vector<CharacteristicInfo> characteristic_infos;
            ConvertCharacteristicInfo(endpoints, characteristic_infos);
            callback(characteristic_infos);
          }
        };
    create_characteristic->Invoke<void>(service_id, device_id,
                                        characteristicCallBack);
  }
}

void BluetoothBleAttributeAdapter::CreateDescriptor(
    const std::string& characteristic_id,
    const std::string& device_id,
    BluetoothBleAttributeAdapter::CreateDescriptorCallback callback) {
  if (auto create_descriptor = ohos::adapter::GetJSFunction(
      "BluetoothLowEnergyAdapter.CreateDescriptor")) {
    std::function<void(aki::Value)> descriptorInfoCallBack =
        [=](aki::Value endpoints) {
          if (!endpoints.IsNull() && !endpoints.IsUndefined()) {
            std::vector<DescriptorInfo> descriptor_infos;
            ConvertDescriptorInfo(endpoints, descriptor_infos);
            callback(descriptor_infos);
          }
        };
    create_descriptor->Invoke<void>(characteristic_id, device_id,
                                    descriptorInfoCallBack);
  }
}

void BluetoothBleAttributeAdapter::ReadDescriptor(
    const std::string& descriptor_id,
    const std::string& device_id,
    BluetoothBleAttributeAdapter::ReadValueCallback callback) {
  if (auto read_descriptor = ohos::adapter::GetJSFunction(
      "BluetoothLowEnergyAdapter.ReadDescriptor")) {
    std::function<void(int, aki::ArrayBuffer)> readCallback =
        [=](int error_code, aki::ArrayBuffer buffer) {
          const uint8_t* data = static_cast<const uint8_t*>(buffer.GetData());
          const int size = buffer.GetLength();
          callback(error_code, std::vector<uint8_t>(data, data + size));
        };
    read_descriptor->Invoke<void>(descriptor_id, device_id, readCallback);
  }
}

void BluetoothBleAttributeAdapter::ReadCharacteristic(
    const std::string& characteristic_id,
    const std::string& device_id,
    BluetoothBleAttributeAdapter::ReadValueCallback callback) {
  if (auto read_characteristic = ohos::adapter::GetJSFunction(
      "BluetoothLowEnergyAdapter.ReadCharacteristic")) {
    std::function<void(int, aki::ArrayBuffer)> readCallback =
        [=](int error_code, aki::ArrayBuffer buffer) {
          const uint8_t* data = static_cast<const uint8_t*>(buffer.GetData());
          const int size = buffer.GetLength();
          callback(error_code, std::vector<uint8_t>(data, data + size));
        };
    read_characteristic->Invoke<void>(characteristic_id, device_id,
                                      readCallback);
  }
}

void BluetoothBleAttributeAdapter::WriteDescriptor(
    const std::string& descriptor_id,
    const std::string& device_id,
    const std::vector<uint8_t>& value,
    BluetoothBleAttributeAdapter::WriteCallback callback) {
  if (auto write_descriptor = ohos::adapter::GetJSFunction(
      "BluetoothLowEnergyAdapter.WriteDescriptor")) {
    std::vector<uint8_t>& copy_value = const_cast<std::vector<uint8_t>&>(value);
    aki::ArrayBuffer buffer(copy_value.data(), copy_value.size());
    write_descriptor->Invoke<void>(descriptor_id, device_id, buffer, callback);
  }
}

void BluetoothBleAttributeAdapter::DeprecatedWriteCharacteristic(
    const std::string& characteristic_id,
    const std::string& device_id,
    const std::vector<uint8_t>& value,
    BluetoothBleAttributeAdapter::WriteCallback callback) {
  if (auto write_characteristic = ohos::adapter::GetJSFunction(
      "BluetoothLowEnergyAdapter.DeprecatedWriteCharacteristic")) {
    std::vector<uint8_t>& copy_value = const_cast<std::vector<uint8_t>&>(value);
    aki::ArrayBuffer buffer(copy_value.data(), copy_value.size());
    write_characteristic->Invoke<void>(characteristic_id, device_id, buffer,
                                       callback);
  }
}

void BluetoothBleAttributeAdapter::WriteCharacteristic(
    const std::string& characteristic_id,
    const std::string& device_id,
    const int write_type,
    const std::vector<uint8_t>& value,
    BluetoothBleAttributeAdapter::WriteCallback callback) {
  if (auto write_characteristic = ohos::adapter::GetJSFunction(
      "BluetoothLowEnergyAdapter.WriteCharacteristic")) {
    std::vector<uint8_t>& copy_value = const_cast<std::vector<uint8_t>&>(value);
    aki::ArrayBuffer buffer(copy_value.data(), copy_value.size());
    write_characteristic->Invoke<void>(characteristic_id, device_id, write_type,
                                       buffer, callback);
  }
}

void BluetoothBleAttributeAdapter::SubscribeToNotifications(
    const std::string& characteristic_id,
    const std::string& device_id,
    OnValueChangeCallback callback) {
  if (auto subscribe = ohos::adapter::GetJSFunction(
      "BluetoothLowEnergyAdapter.SubscribeToNotifications")) {
    std::function<void(aki::ArrayBuffer, int)> subscribeCallback =
        [=](aki::ArrayBuffer buffer, int error_code) {
          const uint8_t* data = static_cast<const uint8_t*>(buffer.GetData());
          const int size = buffer.GetLength();
          callback(error_code, std::vector<uint8_t>(data, data + size));
        };

    subscribe->Invoke<void>(characteristic_id, device_id, subscribeCallback);
  }
}

void BluetoothBleAttributeAdapter::UnsubscribeFromNotifications(
    const std::string& characteristic_id,
    const std::string& device_id,
    BluetoothBleAttributeAdapter::WriteCallback callback) {
  if (auto un_ubscribe = ohos::adapter::GetJSFunction(
      "BluetoothLowEnergyAdapter.UnsubscribeFromNotifications")) {
    un_ubscribe->Invoke<void>(characteristic_id, device_id, callback);
  }
}

}  // namespace ohos::adapter::device

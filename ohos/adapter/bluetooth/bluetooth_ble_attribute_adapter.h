// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_BLUETOOTH_BLUETOOTH_BLE_ATTRIBUTE_ADAPTER_H_
#define OHOS_ADAPTER_BLUETOOTH_BLUETOOTH_BLE_ATTRIBUTE_ADAPTER_H_

#include <functional>
#include <string>
#include <vector>

#include "ohos/adapter/aki_hook/aki_hook.h"
#include "ohos/adapter/export.h"

namespace ohos::adapter::device {

struct ServiceInfo {
  std::string identifier;
  std::string device_id;
  std::string service_uuid;
  bool is_primary;
};
struct DescriptorInfo {
  std::string identifier;
  std::string device_id;
  std::string service_uuid;
  std::string characteristic_uuid;
  std::string descriptor_uuid;
};

struct CharacteristicInfo {
  std::string identifier;
  std::string device_id;
  std::string service_uuid;
  std::string characteristic_uuid;
  int32_t properties;
};

class ADAPTER_EXPORT_API BluetoothBleAttributeAdapter {
 public:
  using CreateServicesCallback =
      std::function<void(const std::vector<ServiceInfo>&)>;
  using CreateCharacteristicCallback =
      std::function<void(const std::vector<CharacteristicInfo>&)>;
  using CreateDescriptorCallback =
      std::function<void(const std::vector<DescriptorInfo>&)>;
  using ReadValueCallback =
      std::function<void(int error_code, const std::vector<uint8_t>& value)>;
  using WriteCallback = std::function<void(int error_code)>;
  using OnValueChangeCallback =
      std::function<void(int error_code, const std::vector<uint8_t>& value)>;
  using OnUnsubscribeCallback = std::function<void(int error_code)>;

  static void StartGattDiscovery(const std::string& device_id,
                                 CreateServicesCallback callback);

  static void CreateCharacteristic(const std::string& service_id,
                                   const std::string& device_id,
                                   CreateCharacteristicCallback callback);

  static void CreateDescriptor(const std::string& characteristic_id,
                               const std::string& device_id,
                               CreateDescriptorCallback callback);

  static void ReadDescriptor(const std::string& descriptor_id,
                             const std::string& device_id,
                             ReadValueCallback callback);

  static void ReadCharacteristic(const std::string& characteristic_id,
                                 const std::string& device_id,
                                 ReadValueCallback callback);

  static void WriteDescriptor(const std::string& descriptor_id,
                              const std::string& device_id,
                              const std::vector<uint8_t>& value,
                              WriteCallback callback);

  static void DeprecatedWriteCharacteristic(const std::string& characteristic_id,
                                            const std::string& device_id,
                                            const std::vector<uint8_t>& value,
                                            WriteCallback callback);

  static void WriteCharacteristic(const std::string& characteristic_id,
                                  const std::string& device_id,
                                  const int write_type,
                                  const std::vector<uint8_t>& value,
                                  WriteCallback callback);

  static void UnsubscribeFromNotifications(const std::string& characteristic_id,
                                           const std::string& device_id,
                                           OnUnsubscribeCallback callback);

  static void SubscribeToNotifications(const std::string& characteristic_id,
                                       const std::string& device_id,
                                       OnValueChangeCallback callback);

 private:
  BluetoothBleAttributeAdapter() = default;
  virtual ~BluetoothBleAttributeAdapter() = default;
};

}  // namespace ohos::adapter::device
#endif  // OHOS_ADAPTER_BLUETOOTH_BLUETOOTH_BLE_ATTRIBUTE_ADAPTER_H_

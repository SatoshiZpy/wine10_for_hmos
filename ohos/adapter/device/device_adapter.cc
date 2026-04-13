// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/device/device_adapter.h"
#include "ohos/adapter/aki_hook/aki_hook.h"
#include "ohos/adapter/common/logging.h"
#include "ohos/adapter/common/util.h"

namespace ohos::adapter::device {
void ConvertEndpoint(aki::Value endpoints,
                     std::vector<USBEndpoint>& ohos_endpoints) {
  if (endpoints.IsNull() || endpoints.IsUndefined()) {
    LOGE("ConvertEndpoint, endpoints is nullptr");
    return;
  }
  for (uint32_t index = 0; index < GetArrayLength(endpoints); index++) {
    aki::Value endpoint = endpoints[index];
    USBEndpoint ohos_endpoint;
    ohos_endpoint.address = GetValue<int32_t>(endpoint["address"], -1);
    ohos_endpoint.attributes = GetValue<int32_t>(endpoint["attributes"], -1);
    ohos_endpoint.interval = GetValue<int32_t>(endpoint["interval"], -1);
    ohos_endpoint.maxPacketSize =
        GetValue<int32_t>(endpoint["maxPacketSize"], -1);
    ohos_endpoint.direction = GetValue<USBRequestDirection>(
        endpoint["direction"], USB_REQUEST_DIR_TO_DEVICE);
    ohos_endpoint.number = GetValue<int32_t>(endpoint["number"], -1);
    ohos_endpoint.type = GetValue<int32_t>(endpoint["type"], -1);
    ohos_endpoint.interfaceId = GetValue<int32_t>(endpoint["interfaceId"], -1);
    ohos_endpoints.push_back(ohos_endpoint);
  }
}

void ConvertInterface(aki::Value interfaces,
                      std::vector<USBInterface>& ohos_interfaces) {
  if (interfaces.IsNull() || interfaces.IsUndefined()) {
    LOGE("ConvertInterface, interfaces is nullptr");
    return;
  }
  for (uint32_t index = 0; index < GetArrayLength(interfaces); index++) {
    aki::Value interface = interfaces[index];
    USBInterface ohos_interface;
    ohos_interface.id = GetValue<int32_t>(interface["id"], -1);
    ohos_interface.protocol = GetValue<int32_t>(interface["protocol"], -1);
    ohos_interface.clazz = GetValue<int32_t>(interface["clazz"], -1);
    ohos_interface.subClass = GetValue<int32_t>(interface["subClass"], -1);
    ohos_interface.alternateSetting =
        GetValue<int32_t>(interface["alternateSetting"], -1);
    ohos_interface.name = GetValue<std::string>(interface["name"], "");

    std::vector<USBEndpoint> endpoints;
    if (!interface["endpoints"].IsArray()) {
      LOGE("ConvertInterface, USBEndpoint data formate error");
      return;
    }
    ConvertEndpoint(interface["endpoints"], ohos_interface.endpoints);
    ohos_interfaces.push_back(ohos_interface);
  }
}

void ConvertConfiguration(aki::Value configs,
                          std::vector<USBConfiguration>& ohos_configs) {
  if (configs.IsNull() || configs.IsUndefined()) {
    LOGE("ConvertConfiguration, configs is nullptr");
    return;
  }
  for (uint32_t index = 0; index < GetArrayLength(configs); index++) {
    aki::Value config = configs[index];
    USBConfiguration ohos_config;
    ohos_config.id = GetValue<int32_t>(config["id"], -1);
    ohos_config.attributes = GetValue<int32_t>(config["attributes"], -1);
    ohos_config.maxPower = GetValue<int32_t>(config["maxPower"], -1);
    ohos_config.name = GetValue<std::string>(config["name"], "");
    ohos_config.isRemoteWakeup =
        GetValue<bool>(config["isRemoteWakeup"], false);
    ohos_config.isSelfPowered = GetValue<bool>(config["isSelfPowered"], false);

    if (!config["interfaces"].IsArray()) {
      LOGE("ConvertConfiguration, USBInterface data formate error");
      return;
    }
    ConvertInterface(config["interfaces"], ohos_config.interfaces);
    ohos_configs.push_back(ohos_config);
  }
}

void ConvertDevice(aki::Value device, USBDevice& ohos_device) {
  ohos_device.busNum = GetValue<int32_t>(device["busNum"], -1);
  ohos_device.devAddress = GetValue<int32_t>(device["devAddress"], -1);
  ohos_device.serial = GetValue<std::string>(device["serial"], "");
  ohos_device.name = GetValue<std::string>(device["name"], "");
  ohos_device.manufacturerName =
      GetValue<std::string>(device["manufacturerName"], "");
  ohos_device.productName = GetValue<std::string>(device["productName"], "");
  ohos_device.version = GetValue<std::string>(device["version"], "");
  ohos_device.vendorId = GetValue<int32_t>(device["vendorId"], -1);
  ohos_device.productId = GetValue<int32_t>(device["productId"], -1);
  ohos_device.clazz = GetValue<int32_t>(device["clazz"], -1);
  ohos_device.subClass = GetValue<int32_t>(device["subClass"], -1);
  ohos_device.protocol = GetValue<int32_t>(device["protocol"], -1);

  if (!device["configs"].IsArray()) {
    LOGE("ConvertDevice, usbconfiguration data formate error");
    return;
  }
  ConvertConfiguration(device["configs"], ohos_device.configs);
}

bool DeviceAdapter::GetDevices(std::vector<USBDevice>& device_list) const {
  auto get_devices_call =
      ohos::adapter::GetJSFunction("DeviceAdapter.GetDevices");
  if (!get_devices_call) {
    LOGE("DeviceAdapter GetDevices js binding error: function undefined.");
    return false;
  }

  std::function<void(aki::Value, int32_t)> get_devices_cb =
      [&](aki::Value devices, int32_t len) {
        if (!devices.IsArray()) {
          LOGE("GetDevices input param err, devices is not array.");
          return;
        }
        for (int32_t i = 0; i < len; i++) {
          USBDevice device;
          ConvertDevice(devices[i], device);
          device_list.push_back(device);
        }
      };
  return get_devices_call->Invoke<bool>(get_devices_cb);
}

void HotplugCallback::ParseEvent(int32_t event_type,
                                 aki::Value event_data,
                                 int32_t len) {
  if (!event_data.IsArray()) {
    LOGE("Failed to get device list: data format error.");
    return;
  }

  for (int32_t i = 0; i < len; i++) {
    USBDevice device;
    ConvertDevice(event_data[i], device);
    if (static_cast<EventType>(event_type) == EventType::DEVICE_ATTATCHED) {
      OnDeviceAttatched(device);
    } else if (static_cast<EventType>(event_type) ==
               EventType::DEVICE_DETATCHED) {
      OnDeviceRemoved(device);
    }
  }
}

void DeviceAdapter::RegisterHotplugCallback(HotplugCallback* hotplug_cb) const {
  auto device_update_func =
      ohos::adapter::GetJSFunction("DeviceAdapter.RegisterHotplugCallback");
  if (!device_update_func) {
    LOGE(
        "DeviceAdapter RegisterHotplugCallback js binding error: function "
        "undefined.");
    return;
  }

  std::function<void(int32_t, aki::Value, int32_t)> get_hotplug_cb =
      [=](int32_t event_type, aki::Value event_data, int32_t len) {
        if (hotplug_cb) {
          hotplug_cb->ParseEvent(event_type, event_data, len);
        }
      };

  device_update_func->Invoke<void>(get_hotplug_cb);
}

void DeviceAdapter::UnregisterHotplugCallback() const {
  auto unregister_hotplug_func =
      ohos::adapter::GetJSFunction("DeviceAdapter.UnregisterHotplugCallback");
  if (!unregister_hotplug_func) {
    LOGE(
        "DeviceAdapter UnregisterHotplugCallback js binding error: function "
        "undefined.");
    return;
  }
  unregister_hotplug_func->Invoke<void>();
}

// static
bool DeviceAdapter::OpenDevice(const USBDevice& device, USBDevicePipe& pipe) {
  if (auto open_device_func =
          aki::JSBind::GetJSFunction("DeviceAdapter.OpenDevice")) {
    std::function<void(aki::Value)> callback = [&](aki::Value result) {
      pipe.busNum = GetValue<int32_t>(result["busNum"], -1);
      pipe.devAddress = GetValue<int32_t>(result["devAddress"], -1);
    };
    return open_device_func->Invoke<bool>(device, callback);
  }
  return false;
}

// static
std::vector<uint8_t> DeviceAdapter::GetRawDescriptor(
    const USBDevicePipe& pipe) {
  std::vector<uint8_t> raw_descriptor;
  if (auto get_raw_descriptor_func =
          aki::JSBind::GetJSFunction("DeviceAdapter.GetRawDescriptor")) {
    std::function<void(std::vector<uint8_t>, int32_t)> callback =
        [&](std::vector<uint8_t> result, int32_t length) {
          if (result.size() > 0 && length > 0) {
            raw_descriptor = result;
            raw_descriptor.resize(length);
          }
        };
    get_raw_descriptor_func->Invoke<void>(pipe, callback);
  }
  return raw_descriptor;
}

// static
bool DeviceAdapter::HasRight(const std::string& device_name) {
  return GetAkiResult<bool, const std::string&>("DeviceAdapter.HasRight",
                                                device_name, false);
}

// static
bool DeviceAdapter::RequestRight(const std::string& device_name) {
  return GetAkiResultAsync<bool, const std::string&>(
      "DeviceAdapter.RequestRight", device_name, false);
}

// static
int32_t DeviceAdapter::GetFileDescriptor(const USBDevicePipe& pipe) {
  return GetAkiResult<int32_t, const USBDevicePipe&>(
      "DeviceAdapter.GetFileDescriptor", pipe, -1);
}

// static
bool DeviceAdapter::RemoveRight(const std::string& device_name) {
  return GetAkiResult<bool, const std::string&>("DeviceAdapter.RemoveRight",
                                                device_name, false);
}

// static
int32_t DeviceAdapter::ReleaseInterface(const USBDevicePipe& pipe,
                                        const USBInterface& iface) {
  return GetAkiResult<int32_t, const USBDevicePipe&, const USBInterface&>(
      "DeviceAdapter.ReleaseInterface", pipe, iface, -1);
}

// static
int32_t DeviceAdapter::ClosePipe(const USBDevicePipe& pipe) {
  return GetAkiResult<int32_t, const USBDevicePipe&>("DeviceAdapter.ClosePipe",
                                                     pipe, -1);
}

JSBIND_ENUM(USBRequestDirection) {
  JSBIND_ENUM_VALUE(USB_REQUEST_DIR_TO_DEVICE);
  JSBIND_ENUM_VALUE(USB_REQUEST_DIR_FROM_DEVICE);
}

JSBIND_CLASS(USBEndpoint) {
  JSBIND_CONSTRUCTOR<>();
  JSBIND_PROPERTY(address);
  JSBIND_PROPERTY(attributes);
  JSBIND_PROPERTY(interval);
  JSBIND_PROPERTY(maxPacketSize);
  JSBIND_PROPERTY(direction);
  JSBIND_PROPERTY(number);
  JSBIND_PROPERTY(type);
  JSBIND_PROPERTY(interfaceId);
}

JSBIND_CLASS(USBInterface) {
  JSBIND_CONSTRUCTOR<>();
  JSBIND_PROPERTY(id);
  JSBIND_PROPERTY(protocol);
  JSBIND_PROPERTY(clazz);
  JSBIND_PROPERTY(subClass);
  JSBIND_PROPERTY(alternateSetting);
  JSBIND_PROPERTY(name);
  JSBIND_PROPERTY(endpoints);
}

JSBIND_CLASS(USBConfiguration) {
  JSBIND_CONSTRUCTOR<>();
  JSBIND_PROPERTY(id);
  JSBIND_PROPERTY(attributes);
  JSBIND_PROPERTY(maxPower);
  JSBIND_PROPERTY(name);
  JSBIND_PROPERTY(isRemoteWakeup);
  JSBIND_PROPERTY(isSelfPowered);
  JSBIND_PROPERTY(interfaces);
}

JSBIND_CLASS(USBDevice) {
  JSBIND_CONSTRUCTOR<>();
  JSBIND_PROPERTY(busNum);
  JSBIND_PROPERTY(devAddress);
  JSBIND_PROPERTY(serial);
  JSBIND_PROPERTY(name);
  JSBIND_PROPERTY(manufacturerName);
  JSBIND_PROPERTY(productName);
  JSBIND_PROPERTY(version);
  JSBIND_PROPERTY(vendorId);
  JSBIND_PROPERTY(productId);
  JSBIND_PROPERTY(clazz);
  JSBIND_PROPERTY(subClass);
  JSBIND_PROPERTY(protocol);
  JSBIND_PROPERTY(configs);
}

JSBIND_CLASS(USBDevicePipe) {
  JSBIND_CONSTRUCTOR<>();
  JSBIND_PROPERTY(busNum);
  JSBIND_PROPERTY(devAddress);
}

}  // namespace ohos::adapter::device

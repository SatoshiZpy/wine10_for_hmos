// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_NOTIFICATION_NOTIFICATION_ADAPTER_H
#define OHOS_ADAPTER_NOTIFICATION_NOTIFICATION_ADAPTER_H

#include <cstdint>
#include <memory>
#include <string>

#include "ohos/adapter/aki_hook/aki_hook.h"
#include "ohos/adapter/export.h"

namespace ohos {
namespace adapter {

struct NotificationImage {
  int width = 0;
  int height = 0;
  std::unique_ptr<char[]> buff;
};

struct NotificationButton {
  std::string title = "";
  int buttonIndex = 0;
};

struct NotificationRequest {
  int notificationId = 0;
  std::string title = "";
  std::string message = "";
  bool requireInteraction;
  bool silent;
  int64_t timestamp;
  NotificationImage icon;
  NotificationImage image;
  std::vector<NotificationButton> buttons;
};

struct NotificationAdapterImage {
  int width = 0;
  int height = 0;
  aki::ArrayBuffer buff;
};

struct NotificationAdapterButton {
  std::string title = "";
  int buttonIndex = 0;
};

struct NotificationAdapterRequest {
  int notificationId = 0;
  std::string title = "";
  std::string message = "";
  bool requireInteraction;
  bool silent;
  int64_t timestamp;
  NotificationAdapterImage icon;
  NotificationAdapterImage image;
  std::vector<NotificationAdapterButton> buttons;
};

class ADAPTER_EXPORT_API NotificationAdapter {
 public:
  using OnClickCallback = std::function<void(int32_t)>;
  using OnCloseCallback = std::function<void(int32_t)>;
  using OnButtonClickCallback = std::function<void(int32_t, int32_t)>;
  static NotificationAdapter& GetInstance();
  NotificationAdapter(const NotificationAdapter&) = delete;
  NotificationAdapter(NotificationAdapter&&) = delete;
  NotificationAdapter& operator=(const NotificationAdapter&) = delete;
  virtual ~NotificationAdapter() = default;

  void SendNotification(NotificationRequest& request);
  void CloseNotification(int notificationId);
  std::vector<int> getAllDisplayedNotification();
  OnClickCallback GetOnClickCallback() { return onClickCallback_; }
  void RegisterOnClickCallback(OnClickCallback callback);
  OnCloseCallback GetOnCloseCallback() { return onCloseCallback_; }
  void RegisterOnCloseCallback(OnCloseCallback callback);
  OnButtonClickCallback GetOnButtonClickCallback() {
    return onButtonClickCallback_;
  }
  void RegisterOnButtonClickCallback(OnButtonClickCallback callback);
  void DestroyCallback();

 private:
  OnClickCallback onClickCallback_;
  OnCloseCallback onCloseCallback_;
  OnButtonClickCallback onButtonClickCallback_;
  NotificationAdapter();
  NotificationAdapterRequest ConvertNotificationRequest(
      NotificationRequest& request);
  NotificationAdapterImage ConvertNotificationImage(NotificationImage& image);
  std::vector<NotificationAdapterButton> ConvertNotificationButton(
      std::vector<NotificationButton>& buttons);
};

}  // namespace adapter
}  // namespace ohos

#endif  // OHOS_ADAPTER_NOTIFICATION_ADAPTER_NOTIFICATION_ADAPTER_H

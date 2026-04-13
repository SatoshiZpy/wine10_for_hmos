// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_TRACING_COMMON_EVENT_WRAPPER_H_
#define OHOS_ADAPTER_TRACING_COMMON_EVENT_WRAPPER_H_

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "BasicServicesKit/oh_commonevent.h"

namespace ohos::adapter::common_event {

class SubscribeInfoDeleter;
class EventData;
class SubscriberDeleter;

using SubscribeInfoPtr =
    std::unique_ptr<CommonEvent_SubscribeInfo, SubscribeInfoDeleter>;

class EventData {
 public:
  std::string event;
  std::string data_string;
  std::string bundle_name;
  int code = 0;

  std::string ToString() const;
  static EventData Build(const CommonEvent_RcvData* rcvData);
};

class SubscribeInfoDeleter {
 public:
  void operator()(CommonEvent_SubscribeInfo* info) const;
};

class SubscribeInfo {
 public:
  static SubscribeInfoPtr Build(const std::vector<std::string>& events);
};

class SubscriberDeleter {
 public:
  void operator()(CommonEvent_Subscriber* subscriber) const;
};

class Subscriber {
 public:
  static Subscriber& Instance();

  Subscriber(const Subscriber&) = delete;
  Subscriber& operator=(const Subscriber&) = delete;

  bool Subscribe(const std::vector<std::string>& events);
  void Unsubscribe();
  ~Subscriber();

  void RegisterEventListener(std::function<void(const EventData&)> listener);
  std::function<void(const EventData&)> GetObserver();

  static void OnCommonEvent(const CommonEvent_RcvData* data);

 private:
  Subscriber() = default;
  SubscribeInfoPtr subscriber_info_;
  std::unique_ptr<CommonEvent_Subscriber, SubscriberDeleter> subscriber_;
  std::function<void(const EventData&)> listener_;
};
}  // namespace ohos::adapter::common_event
#endif  // OHOS_ADAPTER_TRACING_COMMON_EVENT_WRAPPER_H_

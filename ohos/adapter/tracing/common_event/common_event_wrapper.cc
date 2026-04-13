// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/tracing/common_event/common_event_wrapper.h"

#include <sstream>

#include "BasicServicesKit/oh_commonevent.h"
#include "ohos/adapter/common/logging.h"

namespace ohos::adapter::common_event {

Subscriber& Subscriber::Instance() {
  static Subscriber instance;
  return instance;
}

void SubscribeInfoDeleter::operator()(CommonEvent_SubscribeInfo* info) const {
  if (info != nullptr) {
    OH_CommonEvent_DestroySubscribeInfo(info);
  }
}

EventData EventData::Build(const CommonEvent_RcvData* rcvData) {
  EventData event_data;
  event_data.code = OH_CommonEvent_GetCodeFromRcvData(rcvData);

  auto event_ptr = OH_CommonEvent_GetEventFromRcvData(rcvData);
  if (event_ptr != nullptr) {
    event_data.event = event_ptr;
  }

  auto data_string_ptr = OH_CommonEvent_GetDataStrFromRcvData(rcvData);
  if (data_string_ptr != nullptr) {
    event_data.data_string = data_string_ptr;
  }

  auto bundle_name_ptr = OH_CommonEvent_GetBundleNameFromRcvData(rcvData);
  if (bundle_name_ptr != nullptr) {
    event_data.bundle_name = bundle_name_ptr;
  }
  return event_data;
}

void SubscriberDeleter::operator()(CommonEvent_Subscriber* subscriber) const {
  if (subscriber != nullptr) {
    OH_CommonEvent_DestroySubscriber(subscriber);
  }
}

bool Subscriber::Subscribe(const std::vector<std::string>& events) {
  LOGI("Subscriber::Subscribe");
  subscriber_info_ = SubscribeInfo::Build(events);
  if (subscriber_info_ == nullptr) {
    LOGE("create SubscribeInfo failed");
    return false;
  }

  CommonEvent_Subscriber* subscriber = OH_CommonEvent_CreateSubscriber(
      subscriber_info_.get(), &Subscriber::OnCommonEvent);

  if (subscriber == nullptr) {
    LOGE("create subscriber failed");
    return false;
  }

  subscriber_.reset(subscriber);
  if (OH_CommonEvent_Subscribe(subscriber_.get()) != 0) {
    LOGE("OH_CommonEvent_Subscribe failed");
    return false;
  }

  return true;
}

Subscriber::~Subscriber() {
  if (subscriber_) {
    OH_CommonEvent_DestroySubscriber(subscriber_.get());
  }
}

void Subscriber::RegisterEventListener(
    std::function<void(const EventData&)> listener) {
  listener_ = listener;
}

SubscribeInfoPtr SubscribeInfo::Build(const std::vector<std::string>& events) {
  LOGI("SubscribeInfo::Build");
  std::vector<const char*> event_params;
  for (auto const& event : events) {
    event_params.push_back(event.c_str());
  }
  auto info_ptr =
      OH_CommonEvent_CreateSubscribeInfo(event_params.data(), events.size());
  return SubscribeInfoPtr(info_ptr);
}

std::string EventData::ToString() const {
  std::stringstream ss;
  ss << "event = " << event << ", ";
  ss << "data_string = " << data_string << ", ";
  ss << "bundle_name = " << bundle_name << ", ";
  ss << "code = " << code;
  return ss.str();
}

void Subscriber::OnCommonEvent(const CommonEvent_RcvData* data) {
  if (data == nullptr) {
    return;
  }

  EventData event = EventData::Build(data);
  LOGI("OnCommonEvent %{public}s", event.ToString().c_str());
  Subscriber::Instance().GetObserver()(event);
}

void Subscriber::Unsubscribe() {
  if (subscriber_) {
    OH_CommonEvent_UnSubscribe(subscriber_.get());
  }
}

std::function<void(const EventData&)> Subscriber::GetObserver() {
  return listener_;
}
}  // namespace ohos::adapter::common_event

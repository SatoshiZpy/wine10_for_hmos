// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/tracing/tracing_adapter.h"

#include "ohos/adapter/aki_hook/aki_hook.h"
#include "ohos/adapter/tracing/common_event/common_event_wrapper.h"

namespace ohos::adapter::tracing {

namespace {

std::atomic<bool> g_trace_enabled = false;
const std::string kWineTracingStart = "wine_tracing_start";
const std::string kWineTracingStop = "wine_tracing_stop";
const int32_t kWineTracingStartCode = 0;
const int32_t kWineTracingStopCode = 1;

const std::map<std::string, int32_t> kEventCodeMap = {
    {kWineTracingStart, kWineTracingStartCode},
    {kWineTracingStop, kWineTracingStopCode},
};

} // namespace

TracingAdapter& TracingAdapter::GetInstance() {
  static TracingAdapter helper;
  return helper;
}

void TracingAdapter::RegisterTracingCallback(
    const TraceControllerCallback& callback) {
  LOGI("RegisterTracingCallback ");
  callback_ = callback;
  common_event::Subscriber::Instance().RegisterEventListener(
      [](const common_event::EventData& data) {
        auto& callback = TracingAdapter::GetInstance().GetTracingCallback();
        LOGI("recv common_event %{public}s", data.ToString().c_str());
        if (callback) {
          // ignore the event from other app, just handle the event from hdc.
          if (!data.bundle_name.empty()) {
            return;
          }
          if (data.event == kWineTracingStart) {
            callback(kEventCodeMap.at(kWineTracingStart), data.data_string);
          } else if (data.event == kWineTracingStop) {
            callback(kEventCodeMap.at(kWineTracingStop), data.data_string);
          }
        }
      });
  common_event::Subscriber::Instance().Subscribe(
      {kWineTracingStart, kWineTracingStop});
}

void TracingAdapter::UnregisterTracingCallback() {
  common_event::Subscriber::Instance().Unsubscribe();
}

void TracingAdapter::EnableAdapterTrace() {
  g_trace_enabled = true;
}

void TracingAdapter::DisableAdapterTrace() {
  g_trace_enabled = false;
}

bool TracingAdapter::IsAdapterTraceEnabled() {
  return g_trace_enabled;
}

void TracingAdapter::EnableLongTracing(bool enable,
                                       const std::string& catagories) {
  enableLongTracing_ = enable;
  longTracingCategories_ = catagories;
}

bool TracingAdapter::IsLongTracingEnabled() const {
  return enableLongTracing_;
}

void TracingAdapter::LongTracingStartup() {
  TracingAdapter& instance = GetInstance();
  if (!instance.enableLongTracing_ || !instance.callback_) {
    return;
  }
  instance.enableLongTracing_ = false;
  LOGI("TracingAdapter::LongTracingStartup");
  instance.callback_(0, instance.longTracingCategories_);
}

const TraceControllerCallback& TracingAdapter::GetTracingCallback() const {
  return callback_;
}
} // namespace ohos::adapter::tracing

// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_TRACING_ADAPTER_H_
#define OHOS_ADAPTER_TRACING_ADAPTER_H_

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "ohos/adapter/export.h"

namespace ohos::adapter::tracing {

using TraceControllerCallback = std::function<void(int32_t, std::string)>;

class ADAPTER_EXPORT_API TracingAdapter {
 public:
  static TracingAdapter& GetInstance();
  // must invoke on ui thread, start long trace mode if enabled
  static void LongTracingStartup();

  TracingAdapter(const TracingAdapter&) = delete;
  TracingAdapter& operator=(const TracingAdapter&) = delete;

  void RegisterTracingCallback(const TraceControllerCallback& callback);
  void UnregisterTracingCallback();
  const TraceControllerCallback& GetTracingCallback() const;

  // used for control adapter level trace status
  void EnableAdapterTrace();
  void DisableAdapterTrace();
  bool IsAdapterTraceEnabled();
  // used for enable long tracing on start up
  void EnableLongTracing(bool enable, const std::string& catagories);
  // detect if long trace enabled
  bool IsLongTracingEnabled() const;

 private:
  TracingAdapter() = default;
  ~TracingAdapter() = default;

  // the `std::function` instance of callback,
  // first argument is `0`(start), `1`(end),
  // second argument is catagories like `--trace-start-up` flag.
  TraceControllerCallback callback_;
  // Wheather enable long tracing, if true will start long trace mode.
  bool enableLongTracing_ = false;
  // If set, will override default tracing catagories
  std::string longTracingCategories_;
};
} // namespace ohos::adapter::tracing
#endif // OHOS_ADAPTER_TRACING_ADAPTER_H_

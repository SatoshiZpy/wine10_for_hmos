// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/browser/browser_adapter.h"

#include <unistd.h>
#include <js_native_api.h>
#include <js_native_api_types.h>

#include "ohos/adapter/aki_hook/aki_hook.h"
#include "ohos/adapter/common/logging.h"
#include "ohos/adapter/xcomponent/adapter/window_adapter.h"

namespace ohos::adapter {

BrowserAdapter& BrowserAdapter::GetInstance() {
  static BrowserAdapter helper;
  return helper;
}

void BrowserAdapter::RegisterBrowserCallback(
  BrowserCallback callback) {
  if (callback != nullptr) {
    callback_ = callback;
  }
}

CommandResult BrowserAdapter::ExecuteCommand(const int opt_type,
                                             const aki::Value opt_value) {
  std::lock_guard<std::mutex> lock(mutex_);

  CommandParameter param;
  param.type = static_cast<CommandType>(opt_type);
  LOGI("BrowserAdapter::ExecuteCommand param: %{public}s", param.ToString().c_str());

  auto result = std::make_shared<CommandResult>();
  if (param.type == CommandType::kGetWidget) {
    result->ret_code = 0;
    result->widget_Id =
        xcomponent::WindowAdapter::GetInstance().NextWindowWidgetId();
    return *result;
  }

  if (param.type == CommandType::kGetPid) {
    result->ret_code = 0;
    result->pid = getpid();
    return *result;
  }

  auto callback = GetBrowserCallback();
  if (!callback) {
    LOGW("BrowserAdapter::ExecuteCommand no callback register.");
    return *result;
  }

  // asynchronous call
  if (!param.is_sync) {
    callback(param, result);
  // synchronous call
  } else {
    auto promise = std::make_shared<std::promise<bool>>();
    auto future = promise->get_future();
    result->async_callback = [promise](){
      promise->set_value(true);
    };
    callback(param, result);
    auto status = future.wait_for(std::chrono::seconds(3));
    if (status == std::future_status::timeout) {
      LOGE("BrowserAdapter::ExecuteCommand Wait timeout");
      return CommandResult{.ret_code = -1};
    }
    future.get();
  }

  return *result;
}

BrowserCloseResponse BrowserAdapter::GetBrowserCloseResponse(int32_t id) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (close_responses_.contains(id)) {
    return close_responses_[id];
  } else {
    return BrowserCloseResponse::kUndetermined;
  }
}

BrowserCloseResponse GetCloseResponse(int32_t id) {
  return BrowserAdapter::GetInstance().GetBrowserCloseResponse(id);
}

void BrowserAdapter::SetBrowserCloseResponse(int32_t id,
                                             BrowserCloseResponse response) {
  std::lock_guard<std::mutex> lock(mutex_);
  close_responses_[id] = response;
}

CommandResult ExecuteCommand(const int opt_type,
                             const aki::Value opt_value) {
  return BrowserAdapter::GetInstance().ExecuteCommand(opt_type,
                                                      std::move(opt_value));
}

JSBIND_CLASS(CommandResult) {
  JSBIND_PROPERTY(ret_code);
  JSBIND_PROPERTY(widget_Id);
  JSBIND_PROPERTY(last_widget_Id);
  JSBIND_PROPERTY(pid);
}

JSBIND_GLOBAL() {
  JSBIND_FUNCTION(ExecuteCommand);
  JSBIND_FUNCTION(GetCloseResponse);
}

}  // namespace ohos::adapter

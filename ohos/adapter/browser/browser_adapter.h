// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_BROWSER_BROWSER_ADAPTER_H_
#define OHOS_ADAPTER_BROWSER_BROWSER_ADAPTER_H_

#include <functional>
#include <mutex>
#include <string>

#include "aki/jsbind.h"
#include "ohos/adapter/export.h"

namespace ohos::adapter {

enum class CommandType {
  kNewWindow,
  kNewWebContent,
  kNewWidget,
  kGetWidget,
  kGetLastActiveWidget,
  kOpenURL,
  kGetPid,
};

enum class BrowserCloseResponse {
  kUndetermined,
  kClosingContinue,
  kClosingInterrupt,
  kClosed,
  kCloseCancelled,
  kClosedAnyway,
};

class ADAPTER_EXPORT_API CommandResult {
public:
  std::function<void()> async_callback = [](){};
  // Returns the result of command execution
  // 0 if success or errcode.
  int32_t ret_code = -1;
  // Returns the widget id.
  int32_t widget_Id = 0;
  // Returns the last widget id.
  int32_t last_widget_Id = 0;
  // Returns the current pid.
  int32_t pid = 0;
};

class ADAPTER_EXPORT_API CommandParameter {
public:
  // Tells desired Operator type
  CommandType type = CommandType::kNewWindow;
  // Tells the browser load Specify urls.
  std::string url;
  // Tells the browser whether to create new window.
  bool force_open;
  // user custom data.
  std::string user_data;
  // Specifies that it is a synchronous call
  bool is_sync = false;

  std::string ToString() const {
    std::stringstream stream;
    stream << "type = " << (int)type << ", url_len = " << url.length()
           << ", force_open = " << force_open << ", is_sync = " << is_sync;
    return stream.str();
  }
};

using BrowserCallback = std::function<void(CommandParameter&, std::shared_ptr<CommandResult> result)>;

class ADAPTER_EXPORT_API BrowserAdapter {
 public:
  static BrowserAdapter& GetInstance();

  void RegisterBrowserCallback(BrowserCallback callback);
  BrowserCallback GetBrowserCallback() { return callback_; }

  CommandResult ExecuteCommand(const int opt_type,
                               const aki::Value opt_value);

  BrowserCloseResponse GetBrowserCloseResponse(int32_t id);
  void SetBrowserCloseResponse(int32_t id, BrowserCloseResponse response);

 private:
  BrowserAdapter() = default;
  ~BrowserAdapter() = default;

  BrowserCallback callback_ = nullptr;
  std::mutex mutex_;

  std::unordered_map<int32_t, BrowserCloseResponse> close_responses_;
};
}

#endif // OHOS_ADAPTER_BROWSER_BROWSER_ADAPTER_H_

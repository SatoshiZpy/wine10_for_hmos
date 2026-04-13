// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_XCOMPONENT_XCOMPONENT_MANAGER_H_
#define OHOS_ADAPTER_XCOMPONENT_XCOMPONENT_MANAGER_H_

#include <ace/xcomponent/native_interface_xcomponent.h>
#include <js_native_api.h>
#include <js_native_api_types.h>
#include <napi/native_api.h>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <stdint.h>
#include <string>
#include <unordered_map>

#include "ohos/adapter/export.h"
#include "ohos/adapter/ipc/window_param.h"
#include "ohos/adapter/window/app_window_adapter.h"
#include "ohos/adapter/window/sub_window_adapter.h"
#include "ohos/adapter/window/system_floating_window_adapter.h"
#include "ohos/adapter/xcomponent/xcomponent_delegate.h"
#include "ohos/adapter/xcomponent/renderer/xcomponent_impl.h"

namespace ohos::adapter::xcomponent {

using namespace ohos::adapter::window;

class ADAPTER_EXPORT_API XComponentManager final : public XComponentDelegate {
 public:
  ~XComponentManager();
  XComponentManager(XComponentManager& other) = delete;
  void operator=(const XComponentManager&) = delete;
  static XComponentManager* GetInstance();
  std::shared_ptr<InputEventCallBack> GetCallBack();

  void Initialize(napi_env env, napi_value exports);

  std::shared_ptr<XComponentImpl> GetXComponent(int32_t widget_id);
  void RemoveXComponent(int32_t widget_id);
  int32_t CreateWindow(const ipc::WindowParam& param);
  void CloseWindow(int32_t widget_id);
  int32_t GetCreatingWindow() { return creating_window_; }
  void RequestLayout(int32_t widget_id);
  int32_t GetActivateWindow() const { return activate_window_; }
  int32_t UpdateWindow(int32_t widget_id, const ipc::WindowParam& param);
  int32_t SetWindowVisible(int widget_id, int visible);
  void SetWindowBounds(int32_t widget_id, const WindowRect& rect);
  
  // XComponentDelegate
  void OnActivationChanged(int32_t widget_id, bool active) override;
  void OnWidgetAvailable(int32_t widget_id) override;
  void OnWidgetDestroyed(int32_t widget_id) override;

 private:
  XComponentManager();

  std::shared_ptr<XComponentImpl> CreateXComponent(int32_t widget_id,
                                                   const XComponentInfo& info);
  int32_t CreateMainWindow(const NewWindowParam& param);
  int32_t CreateSubWindow(const NewWindowParam& param);
  void SetActivateWindow(int32_t widget_id);

  static XComponentManager* manager_;
  std::unordered_map<int32_t, std::shared_ptr<XComponentImpl>> render_map_;
  std::unordered_map<int32_t, std::promise<bool>> window_status_;
  std::queue<int32_t> reuse_window_;
  int32_t creating_window_;
  int32_t activate_window_;
  std::mutex create_mutex_;
  std::mutex mutex_;
  std::shared_ptr<InputEventCallBack> callback_;
};

}  // namespace ohos::adapter::xcomponent

#endif  // OHOS_ADAPTER_XCOMPONENT_XCOMPONENT_MANAGER_H_

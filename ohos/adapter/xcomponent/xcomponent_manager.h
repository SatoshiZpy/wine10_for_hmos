// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_XCOMPONENT_XCOMPONENT_MANAGER_H_
#define OHOS_ADAPTER_XCOMPONENT_XCOMPONENT_MANAGER_H_

#include <ace/xcomponent/native_interface_xcomponent.h>
#include <js_native_api.h>
#include <js_native_api_types.h>
#include <napi/native_api.h>
#include <memory>
#include <mutex>
#include <stdint.h>
#include <string>
#include <unordered_map>

#include "ohos/adapter/export.h"
#include "ohos/adapter/xcomponent/xcomponent_delegate.h"
#include "ohos/adapter/xcomponent/renderer/xcomponent_impl.h"

namespace ohos::adapter::xcomponent {

class ADAPTER_EXPORT_API XComponentManager final : public XComponentDelegate {
 public:
  ~XComponentManager();
  XComponentManager(XComponentManager& other) = delete;
  void operator=(const XComponentManager&) = delete;
  static XComponentManager* GetInstance();

  void Initialize(napi_env env, napi_value exports);

  std::shared_ptr<XComponentImpl> GetXComponent(int32_t widget_id);
  void RemoveXComponent(int32_t widget_id);
  void RequestLayout(int32_t widget_id);
  
  // XComponentDelegate
  void OnActivationChanged(int32_t widget_id, bool active) override;
  void OnWidgetAvailable(int32_t widget_id) override;
  void OnWidgetDestroyed(int32_t widget_id) override;

  int32_t GetActivateWindow();

 private:
  XComponentManager();

  std::shared_ptr<XComponentImpl> CreateXComponent(int32_t widget_id,
                                                   const XComponentInfo& info);
  void SetActivateWindow(int32_t widget_id);
  std::shared_ptr<InputEventCallBack> GetCallBack();

  static XComponentManager* manager_;
  std::unordered_map<int32_t, std::shared_ptr<XComponentImpl>> render_map_;
  std::mutex mutex_;
  std::shared_ptr<InputEventCallBack> callback_;
  int32_t widget_id_ = -1;
};

}  // namespace ohos::adapter::xcomponent

#endif  // OHOS_ADAPTER_XCOMPONENT_XCOMPONENT_MANAGER_H_

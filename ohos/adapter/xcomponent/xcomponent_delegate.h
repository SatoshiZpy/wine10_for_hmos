// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_XCOMPONENT_XCOMPONENT_DELEGATE_H_
#define OHOS_ADAPTER_XCOMPONENT_XCOMPONENT_DELEGATE_H_

#include <string>

namespace ohos::adapter::xcomponent {

enum class XComponentState {
  kUnknown,
  kStarting,
  kStarted,
  kDestroyed,
};

enum class XComponentType {
  kUnknown,
  kWindow,
  kSubWindow,
};

struct XComponentInfo {
  std::string render_id;
  int32_t widget_id;
  XComponentType type;
  int32_t pid;
};

class XComponentDelegate {
 public:

  XComponentDelegate() = default;
  virtual ~XComponentDelegate() = default;

  virtual void OnCloseRequest(int32_t widget_id) {}
  virtual void OnClosed(int32_t widget_id) {}
  virtual void OnWindowStateChanged(int32_t widget_id,
                                    XComponentState old_state,
                                    XComponentState new_state) {}
  virtual void OnWidgetAvailable(int32_t widget_id) = 0;
  virtual void OnWidgetDestroyed(int32_t widget_id) = 0;
  virtual void OnActivationChanged(int32_t widget_id,
                                   bool active) = 0;
};

}  // namespace ohos::adapter::xcomponent

#endif  // OHOS_ADAPTER_XCOMPONENT_XCOMPONENT_DELEGATE_H_

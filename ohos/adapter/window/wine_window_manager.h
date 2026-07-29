// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_WINDOW_WINE_WINDOW_MANAGER_H_
#define OHOS_ADAPTER_WINDOW_WINE_WINDOW_MANAGER_H_

#include "ohos/adapter/window/wine_window.h"

#include <string>
#include <unordered_map>

#include "ohos/adapter/common/mutex.h"
#include "ohos/adapter/xcomponent/adapter/window_adapter.h"

namespace ohos::adapter::window {

using namespace ohos::adapter::xcomponent;

class WineWindowManager : public WindowStatusListener {
  public:
    static WineWindowManager& GetInstance();

    WineWindowManager(WineWindowManager&&) = delete;
    WineWindowManager& operator=(WineWindowManager&&) = delete;
    WineWindowManager(const WineWindowManager&) = delete;
    WineWindowManager& operator=(const WineWindowManager&) = delete;

    // WindowStatusListener
    void OnWindowAdd(const WindowInfo& info) override;
    void OnWindowRemove(const WindowInfo& info) override;

    // Register a new window. Returns the window id.
    int32_t AddWindow(int32_t widget_id, std::shared_ptr<WineWindow> window);

    // Remove a window.
    void RemoveWindow(int32_t widget_id);

    // Find a window object by id;
    std::shared_ptr<WineWindow> GetWindow(int32_t widget_id);

    void SetLastActiveWidgetId(int32_t widget_id) {
      last_active_widget_id_ = widget_id;
    }

    int32_t GetLastActiveWidgetId() {
      return last_active_widget_id_;
    }

    bool IsEmpty();
    bool IsWindowAtLast();

  private:
    WineWindowManager();
    ~WineWindowManager();

    Mutex windows_mutex_;
    std::unordered_map<int32_t, std::shared_ptr<WineWindow>> windows_ GUARDED_BY(windows_mutex_);
    int32_t last_active_widget_id_ = -1;
};

}  // namespace ohos::adapter::window
#endif  //OHOS_ADAPTER_WINDOW_WINE_WINDOW_MANAGER_H_
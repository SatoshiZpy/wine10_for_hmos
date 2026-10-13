// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/window/wine_window_manager.h"

#include "ohos/adapter/common/logging.h"
#include "ohos/adapter/xcomponent/adapter/window_adapter.h"

namespace ohos::adapter::window {

WineWindowManager::WineWindowManager()
{
  WindowAdapter::GetInstance().RegistWindowStatus(this);
}

WineWindowManager::~WineWindowManager() {}

WineWindowManager& WineWindowManager::GetInstance()
{
  static WineWindowManager instance;
  return instance;
}

void WineWindowManager::OnWindowAdd(const WindowInfo& info)
{
  MutexLock lock_protect(windows_mutex_);
  if (!windows_.count(info.widget_id)) {
    return;
  }

  auto window = windows_[info.widget_id];
  if (window != nullptr) {
    window->OnSurfaceCreated();
  }
}

void WineWindowManager::OnWindowRemove(const WindowInfo& info)
{
  MutexLock lock_protect(windows_mutex_);
  if (!windows_.count(info.widget_id)) {
    return;
  }
  auto window = windows_[info.widget_id];
  if (window != nullptr) {
    window->OnSurfaceDestoryed();
  }
}

int32_t WineWindowManager::AddWindow(int32_t widget_id, std::shared_ptr<WineWindow> window)
{
  MutexLock lock_protect(windows_mutex_);
  if (widget_id < 0) {
    return -1;
  }
  auto it = windows_.find(widget_id);
  if (it == windows_.end()) {
    windows_[widget_id] = window;
  }
  return widget_id;
}

void WineWindowManager::RemoveWindow(int32_t widget_id)
{
  MutexLock lock_protect(windows_mutex_);
  auto it = windows_.find(widget_id);
  if (it == windows_.end()) {
    return;
  }

  windows_.erase(widget_id);
}

bool WineWindowManager::IsEmpty()
{
  MutexLock lock_protect(windows_mutex_);
  return windows_.empty();
}

std::shared_ptr<WineWindow> WineWindowManager::GetWindow(int32_t widget_id)
{
  MutexLock lock_protect(windows_mutex_);
  if (!windows_.count(widget_id)) {
    return nullptr;
  }
  return windows_[widget_id];
}

bool WineWindowManager::HasSingleToplevelWindow()
{
  int top_window_count = 0;
  for (const auto& pair : windows_) {
    WineWindow* window = pair.second.get();
    if (window && window->AsToplevelWindow()) {
      top_window_count++;
    }
  }
  return top_window_count == 1;
}

bool WineWindowManager::HasVisibleToplevelWindow()
{
  MutexLock lock_protect(windows_mutex_);
  for (const auto& pair : windows_) {
    WineWindow* window = pair.second.get();
    if (window && window->AsToplevelWindow() && window->IsVisible()) {
      return true;
    }
  }
  return false;
}

}  // namespace ohos::adapter::window

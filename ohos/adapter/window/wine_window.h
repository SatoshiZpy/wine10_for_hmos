// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_WINDOW_WINE_WINDOW_H_
#define OHOS_ADAPTER_WINDOW_WINE_WINDOW_H_

#include "ohos/adapter/window/window_common.h"

#include <native_window/external_window.h>

#include <atomic>
#include <future>
#include <string>
#include <unordered_map>

#include "ohos/adapter/window/window_state_monitor.h"

namespace ohos::adapter::window {

class WineFakeWindow;
class WineFloatWindow;
class WinePopupWindow;
class WineToplevelWindow;

class WineWindow {
  public:
    WineWindow();
    WineWindow(WineWindow&&) = delete;
    WineWindow& operator=(WineWindow&&) = delete;

    WineWindow(const WineWindow&) = delete;
    WineWindow& operator=(const WineWindow&) = delete;

    virtual ~WineWindow();

    static std::shared_ptr<WineWindow> Create(
      const NewWindowParam& param,
      int* widget_id);

    bool Initialize(const NewWindowParam& param);

    virtual WineFakeWindow* AsFakeWindow();
    virtual WineFloatWindow* AsFloatWindow();
    virtual WinePopupWindow* AsPopupWindow();
    virtual WineToplevelWindow* AsToplevelWindow();

    void set_parent_window(std::shared_ptr<WineWindow> parent_window) {
      parent_window_ = parent_window;
    }
    WineWindow* parent_window() const { return parent_window_.get(); }

    void SetWidget(int32_t new_widget);
    int32_t GetWidget() const { return widget_; }

    void set_child_window(std::shared_ptr<WineWindow> child_window) {
      child_window_ = child_window;
    }
    WineWindow* child_window() const { return child_window_.get(); }

    WindowInitType type() const { return type_; }
    int32_t hwnd() const { return hwnd_; }
    int32_t owner_pid() const { return owner_pid_; }

    void SetSurfaceId(uint64_t surface_id) { surface_id_ = surface_id; }
    uint64_t GetSurfaceId() const { return surface_id_; }
    
    OHNativeWindow* GetNativeWindow();

    const NewWindowParam& GetParam() const { return newParam_; }

    void OnSurfaceCreated();
    void OnSurfaceDestoryed();

    virtual void Show();
    virtual void Hide();
    virtual void Close();
    virtual bool IsVisible() const;
    virtual void SetTitle(const std::string& title);
    virtual void SetBounds(const WindowRect& rect);
    virtual void SetFullscreen(bool fullscreen, int64_t target_display_id);
    virtual void Maximize();
    virtual void Minimize();
    virtual void Restore();
    virtual void Activate();
    virtual void Deactivate();
    virtual std::string GetWindowUniqueId() const;

  protected:
    bool InitSurface();

  private:
    virtual void OnInitialize(const NewWindowParam& param) {}
    virtual int32_t OnCreateWindow() { return -1; }

    WindowStyle BuilderStyle(const NewWindowParam& param);

    void RegistWindowEvent() {}
    void UnRegistWindowEvent() {}

    bool HasSurface()
    {
      return monitor_.GetState() == WindowStateMonitor::State::WINDOW_ENABLE;
    }

    bool WaitSurface() {
      return monitor_.WaitForState(
        WindowStateMonitor::State::WINDOW_ENABLE, std::chrono::seconds(3));
    }
    
    void UpdateSurface(WindowStateMonitor::State newState) { monitor_.SetState(newState); } 


    NewWindowParam newParam_;
    OHNativeWindow* window_ = nullptr;
    int32_t widget_;
    std::string window_unique_id_;
    WindowInitType type_ = WindowInitType::kWindow;
    int64_t features_;
    int64_t window_state_;
    int32_t hwnd_;
    int32_t owner_pid_;
    
    uint64_t surface_id_;
    WindowRect restored_bounds_;
    WindowRect bounds_;
    WindowRect previous_bounds_;

    std::shared_ptr<WineWindow> parent_window_;
    std::shared_ptr<WineWindow> child_window_;

    int32_t buffer_format = -1;
    int32_t swap_interval = -1;
    WindowStateMonitor monitor_;
};

}  // namespace ohos::adapter::window
#endif  // OHOS_ADAPTER_WINDOW_WINE_WINDOW_H_
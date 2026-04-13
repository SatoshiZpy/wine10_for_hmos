// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_XCOMPONENT_ADAPTER_WINDOW_ADAPTER_H_
#define OHOS_ADAPTER_XCOMPONENT_ADAPTER_WINDOW_ADAPTER_H_

#include <ace/xcomponent/native_interface_xcomponent.h>
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "ohos/adapter/common/mutex.h"
#include "ohos/adapter/export.h"
#include "ohos/adapter/window/window_common.h"
#include "ohos/adapter/xcomponent/event/window_event_common.h"

namespace ohos::adapter::xcomponent {

using namespace ohos::adapter::window;
using KeyboardHeightCallBack = std::function<void(int32_t, int32_t)>;

class WindowInfo {
public:
    OHNativeWindow* window;
    std::string window_id;
    int32_t widget_id;
    int owner_pid;
    bool initialized = false;
};

enum class CrossProcessSyncResult { SUCCESS, FAIL };

class ADAPTER_EXPORT_API WindowStatusListener {
public:
    virtual ~WindowStatusListener() = default;
    virtual void OnWindowAdd(const WindowInfo& info) = 0;
    virtual void OnWindowRemove(const WindowInfo& info) = 0;
};

class ADAPTER_EXPORT_API WindowCachedEventQueue {
public:
    void AddCacheEventToQueue(std::shared_ptr<Event> event);
    std::vector<std::shared_ptr<Event>> GetCachedEvent()
    {
        return cached_events_;
    }

private:
    std::vector<std::shared_ptr<Event>> cached_events_;
    // first WINDOW_OCCLUDED event will not cached
    // when window is creating
    bool first_occluded_event_received_ = false;
};

class ADAPTER_EXPORT_API WindowCachedEventDispatcher {
public:
    void SaveCacheEvent(int32_t widget_id, std::shared_ptr<Event> event);
    void DispatchCachedEvent(int32_t widget_id, WindowEventCallBack& callback);

private:
    std::unordered_map<int32_t, std::unique_ptr<WindowCachedEventQueue>> cached_event_map_;
};

class ADAPTER_EXPORT_API WindowAdapter {
public:
    static WindowAdapter& GetInstance();

    OHNativeWindow* GetWindow(int32_t widget_id);
    std::string GetWindowId(const OHNativeWindow* window);
    int32_t GetWidgetId(const OHNativeWindow* window);
    int GetOwnerPid(int32_t widget_id);
    void AddWindow(int32_t widget_id, OHNativeWindow* window, int owner_pid);
    void RemoveWindow(int32_t widget_id);
    void SetWindowWidget(OHNativeWindow* window, int32_t widget_id);
    void RegistWindowEvent(int32_t widget_id, WindowEventCallBack callback);
    void UnregistWindowEvent(int32_t widget_id);
    void RegistWindowStatus(WindowStatusListener*);
    void NotifyWindowEvent(int32_t widget_id, std::shared_ptr<Event>);
    void RegisterGlobalWindowEvent(WindowEventCallBack callback);
    int32_t NextWindowWidgetId();
    int32_t GetWindowWidgetId();

    void RegistKeyboardHeightEvent(int32_t widget_id, KeyboardHeightCallBack callback);
    void UnRegistKeyboardHeightEvent(int32_t widget_id);
    void NotifyKeyboardHeightEvent(int32_t widget_id, int32_t height);

    WindowRect GetInitialBounds() const;
    void SetInitialBounds(const WindowRect& rect, const WindowRect& content_rect);
    WindowStatusType GetInitialState() const;
    void SetInitialState(const WindowStatusType state);
    WindowRect GetWindowBounds() const
    {
        return window_bounds_;
    }
    WindowRect GetContentBounds() const
    {
        return content_bounds_;
    }
    void SetSystemWindowLimits(WindowLimits window_limits);
    WindowLimits GetSystemWindowLimits() const;

    void OnWindowInitDone(int32_t widget_id);
    bool WindowHasInit(int32_t widget_id);

private:
    WindowAdapter() = default;
    ~WindowAdapter() = default;

    Mutex windows_mutex_;
    std::unordered_map<int32_t, WindowEventCallBack> window_event_callbacks_ GUARDED_BY(windows_mutex_);
    std::atomic<int32_t> next_window_id_ = 0;
    WindowStatusListener* window_status_observer_ = nullptr;
    std::unordered_map<int32_t, WindowInfo> windows_;
    WindowEventCallBack global_window_event_callback_ GUARDED_BY(windows_mutex_) = nullptr;

    Mutex keyboard_height_mutex_;
    std::unordered_map<int32_t, KeyboardHeightCallBack> keyboard_height_callbacks_ GUARDED_BY(keyboard_height_mutex_);

    WindowRect window_bounds_;
    WindowRect content_bounds_;
    WindowRect initial_bounds_;
    WindowStatusType initial_state_;
    WindowLimits system_window_limits_; // in vp
    WindowCachedEventDispatcher cached_event_dispatcher_;
};

} // namespace ohos::adapter::xcomponent

#endif // OHOS_ADAPTER_XCOMPONENT_ADAPTER_WINDOW_ADAPTER_H_

// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/xcomponent/adapter/window_adapter.h"

#include <cctype>
#include <cstddef>
#include <string>
#include <unordered_map>

#include "ohos/adapter/common/constants.h"
#include "ohos/adapter/common/mutex.h"
#include "ohos/adapter/web_entry/web_entry_init.h"
#include "ohos/adapter/xcomponent/xcomponent_utils.h"

using namespace ohos::adapter::web_entry;

namespace ohos::adapter::xcomponent {

OHNativeWindow* WindowAdapter::GetWindow(int32_t widget_id)
{
    MutexLock lock_protect(windows_mutex_);
    auto search = windows_.find(widget_id);
    if (search != windows_.end()) {
        return search->second.window;
    }
    return nullptr;
}

std::string WindowAdapter::GetWindowId(const OHNativeWindow* window)
{
    MutexLock lock_protect(windows_mutex_);
    std::string window_id;
    for (const auto& [index, window_info] : windows_) {
        if (window_info.window == window) {
            window_id = window_info.window_id;
            break;
        }
    }
    return window_id;
}

int32_t WindowAdapter::GetWidgetId(const OHNativeWindow* window)
{
    MutexLock lock_protect(windows_mutex_);
    int32_t widget_id = -1;
    for (const auto& [index, window_info] : windows_) {
        if (window_info.window == window) {
            widget_id = window_info.widget_id;
            break;
        }
    }
    return widget_id;
}

int WindowAdapter::GetOwnerPid(int32_t widget_id)
{
    MutexLock lock_protect(windows_mutex_);
    int owner_pid = -1;
    auto search = windows_.find(widget_id);
    if (search != windows_.end()) {
        owner_pid = search->second.owner_pid;
    }
    return owner_pid;
}

void WindowAdapter::AddWindow(int32_t widget_id, OHNativeWindow* window, int owner_pid)
{
    MutexLock lock_protect(windows_mutex_);
    WindowInfo window_info;
    window_info.window = window;
    window_info.window_id = XComponentUtils::ConvertWidgetIdToWindowId(widget_id);
    window_info.widget_id = widget_id;
    window_info.owner_pid = owner_pid;
    windows_.emplace(widget_id, window_info);
    if (window_status_observer_ != nullptr) {
        window_status_observer_->OnWindowAdd(window_info);
    }
}

void WindowAdapter::RemoveWindow(int32_t widget_id)
{
    MutexLock lock_protect(windows_mutex_);
    auto search = windows_.find(widget_id);
    if (search != windows_.end()) {
        if (window_status_observer_ != nullptr) {
            window_status_observer_->OnWindowRemove(search->second);
        }
        windows_.erase(search);
    }
}

void WindowAdapter::SetWindowWidget(OHNativeWindow* window, int32_t widget_id)
{
    MutexLock lock_protect(windows_mutex_);
    for (auto& [index, window_info] : windows_) {
        if (window_info.window == window) {
            window_info.widget_id = widget_id;
            return;
        }
    }
}

void WindowAdapter::RegistWindowStatus(WindowStatusListener* listener)
{
    window_status_observer_ = listener;
}

void WindowAdapter::RegistWindowEvent(int32_t widget_id, WindowEventCallBack callback)
{
    MutexLock lock(windows_mutex_);
    window_event_callbacks_.emplace(widget_id, callback);
    cached_event_dispatcher_.DispatchCachedEvent(widget_id, callback);
}

void WindowAdapter::UnregistWindowEvent(int32_t widget_id)
{
    MutexLock lock(windows_mutex_);
    window_event_callbacks_.erase(widget_id);
}

void WindowAdapter::RegisterGlobalWindowEvent(WindowEventCallBack callback)
{
    MutexLock lock(windows_mutex_);
    global_window_event_callback_ = callback;
}

void WindowAdapter::NotifyWindowEvent(int32_t widget_id, std::shared_ptr<Event> event)
{
    MutexLock lock(windows_mutex_);
    auto it = window_event_callbacks_.find(widget_id);
    if (it != window_event_callbacks_.end()) {
        auto callback = it->second;
        callback(widget_id, event);
    } else {
        cached_event_dispatcher_.SaveCacheEvent(widget_id, event);
    }

    // 同时调用全局窗口事件回调
    if (global_window_event_callback_ != nullptr) {
        global_window_event_callback_(widget_id, event);
    }
}

int32_t WindowAdapter::NextWindowWidgetId()
{
    return ++next_window_id_;
}

int32_t WindowAdapter::GetWindowWidgetId()
{
    return next_window_id_.load();
}

void WindowAdapter::RegistKeyboardHeightEvent(int32_t widget_id, KeyboardHeightCallBack callback)
{
    MutexLock lock(keyboard_height_mutex_);
    keyboard_height_callbacks_.emplace(widget_id, callback);
}

void WindowAdapter::UnRegistKeyboardHeightEvent(int32_t widget_id)
{
    MutexLock lock(keyboard_height_mutex_);
    keyboard_height_callbacks_.erase(widget_id);
}

void WindowAdapter::NotifyKeyboardHeightEvent(int32_t widget_id, int32_t height)
{
    MutexLock lock(keyboard_height_mutex_);
    auto itCallback = keyboard_height_callbacks_.find(widget_id);
    if (itCallback != keyboard_height_callbacks_.end()) {
        auto callback = keyboard_height_callbacks_[widget_id];
        callback(widget_id, height);
    }
}

WindowRect WindowAdapter::GetInitialBounds() const
{
    return initial_bounds_;
}

void WindowAdapter::SetInitialBounds(const WindowRect& rect, const WindowRect& content_rect)
{
    window_bounds_ = std::move(rect);
    content_bounds_ = std::move(content_rect);
    initial_bounds_ = {window_bounds_.left + content_rect.left, window_bounds_.top + content_rect.top,
                       content_rect.width, content_rect.height};
}

WindowStatusType WindowAdapter::GetInitialState() const
{
    return initial_state_;
}

void WindowAdapter::SetInitialState(const WindowStatusType state)
{
    initial_state_ = state;
}

void WindowAdapter::OnWindowInitDone(int32_t widget_id)
{
    MutexLock lock_protect(windows_mutex_);
    for (auto& [index, window_info] : windows_) {
        if (window_info.widget_id == widget_id) {
            window_info.initialized = true;
        }
    }
}

bool WindowAdapter::WindowHasInit(int32_t widget_id)
{
    MutexLock lock_protect(windows_mutex_);
    for (auto& [index, window_info] : windows_) {
        if (window_info.widget_id == widget_id) {
            return window_info.initialized;
        }
    }

    return false;
}

bool WindowAdapter::IsEmpty()
{
    MutexLock lock_protect(windows_mutex_);
    return windows_.empty();
}

void WindowAdapter::SetSystemWindowLimits(WindowLimits window_limits)
{
    system_window_limits_ = window_limits;
}

WindowLimits WindowAdapter::GetSystemWindowLimits() const
{
    return system_window_limits_;
}

WindowAdapter& WindowAdapter::GetInstance()
{
    static WindowAdapter instance;
    return instance;
}

void WindowCachedEventQueue::AddCacheEventToQueue(std::shared_ptr<Event> event)
{
    if (event->type() == EventType::ET_WINDOW_CHANGE && !first_occluded_event_received_) {
        auto window_event = static_pointer_cast<WindowChangeEvent>(event);
        if (window_event->window_event_type_ == WindowEventType::WINDOW_OCCLUDED) {
            first_occluded_event_received_ = true;
            return;
        }
    }
    cached_events_.push_back(event);
}

void WindowCachedEventDispatcher::SaveCacheEvent(int32_t widget_id, std::shared_ptr<Event> event)
{
    if (widget_id < 0) {
        return;
    }
    auto it = cached_event_map_.find(widget_id);
    if (it == cached_event_map_.end()) {
        auto event_queue = std::make_unique<WindowCachedEventQueue>();
        event_queue->AddCacheEventToQueue(event);
        cached_event_map_[widget_id] = std::move(event_queue);
    } else {
        it->second->AddCacheEventToQueue(event);
    }
}

void WindowCachedEventDispatcher::DispatchCachedEvent(int32_t widget_id, WindowEventCallBack& callback)
{
    auto it = cached_event_map_.find(widget_id);
    if (it != cached_event_map_.end()) {
        std::vector<std::shared_ptr<Event>> events = it->second->GetCachedEvent();
        for (auto event : events) {
            callback(widget_id, event);
        }
    }

    cached_event_map_.erase(widget_id);
}

} // namespace ohos::adapter::xcomponent

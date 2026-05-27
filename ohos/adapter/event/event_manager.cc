// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/event/event_manager.h"

namespace ohos::adapter::event {

EventManager::EventManager() {}

EventManager::~EventManager() {}

EventManager& EventManager::GetInstance()
{
    static EventManager instance;
    return instance;
}

void EventManager::RegisterInputEventCallBack(input_event_callback callback)
{
    if (callback == nullptr) {
        return;
    }
    input_event_dispatcher_.RegisterCallBack(callback);
}

void EventManager::RegisterWindowEventCallBack(input_event_callback callback)
{
    if (callback == nullptr) {
        return;
    }
    window_event_dispatcher_.RegisterCallBack(callback);
}

InputEventDispatcher& EventManager::InputDispatcher()
{
    return input_event_dispatcher_;
}

WindowEventDispatcher& EventManager::WindowDispatcher()
{
    return window_event_dispatcher_;
}

}  // namespace ohos::adapter::event
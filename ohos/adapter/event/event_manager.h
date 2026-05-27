// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_EVENT_EVENT_MANAGER_H_
#define OHOS_ADAPTER_EVENT_EVENT_MANAGER_H_

#include "ohos/adapter/event/input_event_dispatcher.h"
#include "ohos/adapter/event/window_event_dispatcher.h"

namespace ohos::adapter::event {

class ADAPTER_EXPORT_API EventManager {
public:
    EventManager(EventManager& other) = delete;
    void operator=(const EventManager&) = delete;
    EventManager(EventManager&&) = delete;
    void operator=(EventManager&&) = delete;

    static EventManager& GetInstance();

    void RegisterInputEventCallBack(input_event_callback callback);
    void RegisterWindowEventCallBack(input_event_callback callback);

    InputEventDispatcher& InputDispatcher();
    WindowEventDispatcher& WindowDispatcher();

private:
    EventManager();
    ~EventManager();

    InputEventDispatcher input_event_dispatcher_;
    WindowEventDispatcher window_event_dispatcher_;
};

}  // namespace ohos::adapter::event

#endif  // OHOS_ADAPTER_EVENT_EVENT_MANAGER_H_
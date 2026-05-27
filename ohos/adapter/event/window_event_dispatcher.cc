// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/event/window_event_dispatcher.h"

#include "ohos/adapter/common/logging.h"
#include "ohos/adapter/event/event_manager.h"
#include "ohos/adapter/xcomponent/adapter/window_adapter.h"

namespace ohos::adapter::event {

using namespace ohos::adapter::xcomponent;

static void WindowEventCallback(int32_t widget_id, std::shared_ptr<Event> window_event)
{
    if (widget_id <= 0) {
        LOGE("invalid widget_id");
        return;
    }

    input_event event;
    if (window_event->type() == EventType::ET_WINDOW_RECT_CHANGE) {
        auto rect_event = std::static_pointer_cast<WindowRectChangeEvent>(window_event);
        if (rect_event == nullptr || rect_event->reason != RectChangeReason::DRAG_END) {
            return;
        }

        event.type = OHOS_WINDOW_RECT_CHANGE_EVENT;
        event.widget_id = widget_id;
        event.data.window_rect_change_event.left = rect_event->left;
        event.data.window_rect_change_event.top = rect_event->top;
        event.data.window_rect_change_event.width = static_cast<int32_t>(rect_event->width);
        event.data.window_rect_change_event.height = static_cast<int32_t>(rect_event->height);
    } else if (window_event->type() == EventType::ET_SURFACE_FOCUS) {
        event.type = OHOS_WINDOW_FOCUS_EVENT;
        event.widget_id = widget_id;
        event.data.window_focus_event.widget_id = widget_id;
    } else if (window_event->type() == EventType::ET_SURFACE_BLUR) {
        event.type = OHOS_WINDOW_BLUR_EVENT;
        event.widget_id = widget_id;
        event.data.window_focus_event.widget_id = widget_id;
    } else {
        LOGW("invalid event type: %{public}d", window_event->type());
        return;
    }

    EventManager::GetInstance().WindowDispatcher().ExecuteCallback(event);
}

WindowEventDispatcher::WindowEventDispatcher() {}

WindowEventDispatcher::~WindowEventDispatcher() {}

void WindowEventDispatcher::RegisterCallBack(input_event_callback callback)
{
    callback_ = callback;
    WindowAdapter::GetInstance().RegisterGlobalWindowEvent(&WindowEventCallback);
}

void WindowEventDispatcher::ExecuteCallback(const input_event& event)
{
    if (callback_ == nullptr) {
        return;
    }
    callback_(&event);
}

}  // namespace ohos::adapter::event
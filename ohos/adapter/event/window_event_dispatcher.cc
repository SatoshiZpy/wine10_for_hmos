// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/event/window_event_dispatcher.h"

#include "ohos/adapter/common/logging.h"
#include "ohos/adapter/event/event_manager.h"

namespace ohos::adapter::event {

using namespace ohos::adapter::xcomponent;

WindowEventDispatcher::WindowEventDispatcher() {}

WindowEventDispatcher::~WindowEventDispatcher() {}

void WindowEventDispatcher::RegisterCallBack(input_event_callback callback)
{
    callback_ = callback;
}

void WindowEventDispatcher::ExecuteCallback(const input_event& event)
{
    if (callback_ == nullptr) {
        return;
    }
    callback_(&event);
}

}  // namespace ohos::adapter::event
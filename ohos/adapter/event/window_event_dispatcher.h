// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_EVENT_WINDOW_EVENT_DISPATCHER_H_
#define OHOS_ADAPTER_EVENT_WINDOW_EVENT_DISPATCHER_H_

#include <memory>

#include "ohos/adapter/export.h"
#include "ohos/adapter/include/input_event_callback.h"
#include "ohos/adapter/xcomponent/event/window_event_common.h"

namespace ohos::adapter::event {

class ADAPTER_EXPORT_API WindowEventDispatcher {
public:
    WindowEventDispatcher();
    ~WindowEventDispatcher();

    void RegisterCallBack(input_event_callback callback);
    void ExecuteCallback(const input_event& event);

private:
    input_event_callback callback_ = nullptr;
};

}  // namespace ohos::adapter::event

#endif  // OHOS_ADAPTER_EVENT_WINDOW_EVENT_DISPATCHER_H_
// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_EVENT_INPUT_EVENT_DISPATCHER_H_
#define OHOS_ADAPTER_EVENT_INPUT_EVENT_DISPATCHER_H_

#include <memory>

#include "ohos/adapter/export.h"
#include "ohos/adapter/include/input_event_callback.h"
#include "ohos/adapter/xcomponent/event/input_event_common.h"

namespace ohos::adapter::event {

using namespace ohos::adapter::xcomponent;

class ADAPTER_EXPORT_API InputEventDispatcher {
public:
    InputEventDispatcher();
    ~InputEventDispatcher() = default;

    void RegisterCallBack(input_event_callback callback);
    void Dispatch(const input_event& event);
    void ExecuteCallback(const input_event& event);
    std::shared_ptr<InputEventCallBack> GetCallBack() { return xcomponent_callback_; }

private:
    std::shared_ptr<InputEventCallBack> xcomponent_callback_;
    input_event_callback callback_ = nullptr;
    int pid_ = -1;
};

}  // namespace ohos::adapter::event

#endif  // OHOS_ADAPTER_EVENT_INPUT_EVENT_DISPATCHER_H_
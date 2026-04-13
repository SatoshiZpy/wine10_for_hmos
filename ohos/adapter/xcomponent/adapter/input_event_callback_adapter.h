// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef OHOS_ADAPTER_XCOMPONENT_EVENT_INPUT_EVENT_CALLBACK_ADAPTER_H_
#define OHOS_ADAPTER_XCOMPONENT_EVENT_INPUT_EVENT_CALLBACK_ADAPTER_H_

#include <mutex>

#include <ace/xcomponent/native_interface_xcomponent.h>
#include <ace/xcomponent/native_xcomponent_key_event.h>
#include <multimodalinput/oh_input_manager.h>

#include "ohos/adapter/include/input_event_callback.h"
#include "ohos/adapter/xcomponent/event/input_event_common.h"

namespace ohos::adapter::xcomponent {

class ADAPTER_EXPORT_API InputEventCallbackAdapter {
public:
    InputEventCallbackAdapter(InputEventCallbackAdapter& other) = delete;
    void operator=(const InputEventCallbackAdapter &) = delete;
    InputEventCallbackAdapter(InputEventCallbackAdapter &&) = delete;
    void operator=(InputEventCallbackAdapter &&) = delete;
    static InputEventCallbackAdapter& GetInstance();

    std::shared_ptr<InputEventCallBack> GetCallBack();
    void RegisterInputEventCallBack(input_event_callback callback);
    const input_event_callback GetInputEventCallBack();
    bool IsHandleInCurrentProcess(const int32_t widget_id);
    void OnInputEventCallBack(const input_event& event);

private:
    InputEventCallbackAdapter();
    ~InputEventCallbackAdapter();

    std::shared_ptr<InputEventCallBack> callback_;
    input_event_callback input_event_callback_;
    int pid = -1;
};

}  // namespace ohos::adapter::xcomponent
#endif  // OHOS_ADAPTER_XCOMPONENT_EVENT_INPUT_EVENT_CALLBACK_ADAPTER_H_
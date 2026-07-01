/*
 * Copyright (c) 2023-2026 Huawei Device Co., Ltd.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef OHOS_ADAPTER_IPC_INPUT_EVENT_CALLBACK_PARAM_H_
#define OHOS_ADAPTER_IPC_INPUT_EVENT_CALLBACK_PARAM_H_

#include "ohos/adapter/ipc/native_ipc.h"

#include <ace/xcomponent/native_interface_xcomponent.h>
#include <ace/xcomponent/native_xcomponent_key_event.h>

#include "ohos/adapter/include/input_event_callback.h"

namespace ohos::adapter::ipc {

struct InputEventParam {
    static void WriteMouseEventParam(multiprocess::NativeIpcParcel& parcel, const input_event& event)
    {
        parcel.Write(event.data.mouse_event.x);
        parcel.Write(event.data.mouse_event.y);
        parcel.Write(event.data.mouse_event.screen_x);
        parcel.Write(event.data.mouse_event.screen_y);
        parcel.Write(event.data.mouse_event.timestamp);
        parcel.Write(event.data.mouse_event.action);
        parcel.Write(event.data.mouse_event.button);
        parcel.Write(event.data.mouse_event.wheel_x);
        parcel.Write(event.data.mouse_event.wheel_y);
        parcel.Write(event.data.mouse_event.source_tool);
    }

    static void ReadMouseEventParam(const multiprocess::NativeIpcParcel& parcel, input_event& event)
    {
        parcel.Read(event.data.mouse_event.x);
        parcel.Read(event.data.mouse_event.y);
        parcel.Read(event.data.mouse_event.screen_x);
        parcel.Read(event.data.mouse_event.screen_y);
        parcel.Read(event.data.mouse_event.timestamp);
        parcel.Read(event.data.mouse_event.action);
        parcel.Read(event.data.mouse_event.button);
        parcel.Read(event.data.mouse_event.wheel_x);
        parcel.Read(event.data.mouse_event.wheel_y);
        parcel.Read(event.data.mouse_event.source_tool);
    }

    static void WriteKeyEventParam(multiprocess::NativeIpcParcel& parcel, const input_event& event)
    {
        parcel.Write(event.data.key_event.action);
        parcel.Write(event.data.key_event.code);
        parcel.Write(event.data.key_event.source_type);
        parcel.Write(event.data.key_event.device_id);
        parcel.Write(event.data.key_event.timestamp);
        parcel.Write(static_cast<int64_t>(event.data.key_event.modifier_key_states));
        parcel.Write(static_cast<int8_t>(event.data.key_event.is_num_lockon));
        parcel.Write(static_cast<int8_t>(event.data.key_event.is_caps_lockon));
        parcel.Write(static_cast<int8_t>(event.data.key_event.is_scroll_lockon));
    }

    static void ReadKeyEventParam(const multiprocess::NativeIpcParcel& parcel, input_event& event)
    {
        parcel.Read(event.data.key_event.action);
        parcel.Read(event.data.key_event.code);
        parcel.Read(event.data.key_event.source_type);
        parcel.Read(event.data.key_event.device_id);
        parcel.Read(event.data.key_event.timestamp);

        int64_t modifierKeyStates = 0;
        parcel.Read(modifierKeyStates);
        event.data.key_event.modifier_key_states = static_cast<uint64_t>(modifierKeyStates);

        int8_t isNumLockOn = 0;
        parcel.Read(isNumLockOn);
        event.data.key_event.is_num_lockon = static_cast<bool>(isNumLockOn);

        int8_t isCapsLockOn = 0;
        parcel.Read(isCapsLockOn);
        event.data.key_event.is_caps_lockon = static_cast<bool>(isCapsLockOn);

        int8_t isScrollLockOn = 0;
        parcel.Read(isScrollLockOn);
        event.data.key_event.is_scroll_lockon = static_cast<bool>(isScrollLockOn);
    }

    static void ToParcel(multiprocess::NativeIpcParcel& parcel, const input_event& event)
    {
        int type = static_cast<int32_t>(event.type);
        parcel.Write(type);
        parcel.Write(event.widget_id);
        switch (type) {
            case OHOS_MOUSE_EVENT:
                WriteMouseEventParam(parcel, event);
                break;
            case OHOS_KEY_EVENT:
                WriteKeyEventParam(parcel, event);
                break;
            dafault:
                return;
        }
    }

    static void FromParcel(const multiprocess::NativeIpcParcel& parcel, input_event& event)
    {
        int32_t type = 0;
        parcel.Read(type);
        event.type = static_cast<input_event_type>(type);
        parcel.Read(event.widget_id);
        switch (type) {
            case OHOS_MOUSE_EVENT:
                ReadMouseEventParam(parcel, event);
                break;
            case OHOS_KEY_EVENT:
                ReadKeyEventParam(parcel, event);
                break;
            dafault:
                return;
        }
    }
};
} // namespace ohos::adapter::ipc

#endif // OHOS_ADAPTER_IPC_INPUT_EVENT_CALLBACK_PARAM_H_

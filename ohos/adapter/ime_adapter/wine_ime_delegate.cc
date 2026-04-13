// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wine_ime_delegate.h"

#include <cstring>
#include "ohos/adapter/common/logging.h"

namespace ohos {
namespace adapter {

const int32_t MAX_TEXT_LENGTH = 256;

void init_ime_event(ime_event& event)
{
    event.type = OHOS_IME_UNKNOWN;
    event.data.input_text_len = 0;
    event.data.input_text = nullptr;
    event.data.delete_text_len = 0;
    event.data.move_direction = 0;
}

WineIMEDelegate::~WineIMEDelegate()
{
    ime_event_callback_ = nullptr;
}

WineIMEDelegate& WineIMEDelegate::GetInstance()
{
    static WineIMEDelegate delegate;
    return delegate;
}

void WineIMEDelegate::RegisterIMEEventCallBack(ime_event_callback callback)
{
    ime_event_callback_ = callback;
}

void WineIMEDelegate::InsertText(const std::string& text) {
    if (!ime_event_callback_) return;

    ime_event event;
    init_ime_event(event);
    event.type = OHOS_IME_INSERT_TEXT;
    size_t size = text.size();
    if (size >= MAX_TEXT_LENGTH) {
        size = MAX_TEXT_LENGTH - 1;
    }
    event.data.input_text_len = size;
    if (size > 0) {
        event.data.input_text = (char*)malloc((size + 1) * sizeof(char));
        if (!event.data.input_text) {
            LOGE("malloc failed");
            return;
        }
        strncpy(event.data.input_text, text.c_str(), size);
        event.data.input_text[size] = '\0';
    }
    ime_event_callback_(&event);
    if (event.data.input_text) {
        free(event.data.input_text);
    }
}

void WineIMEDelegate::DeleteBackward(int32_t length) {
    if (!ime_event_callback_) return;

    ime_event event;
    init_ime_event(event);
    event.type = OHOS_IME_DELETE_BACKWARD;
    event.data.delete_text_len = length;
    ime_event_callback_(&event);
}

void WineIMEDelegate::DeleteForward(int32_t length) {
    if (!ime_event_callback_) return;

    ime_event event;
    init_ime_event(event);
    event.type = OHOS_IME_DELETE_FORWARD;
    event.data.delete_text_len = length;
    ime_event_callback_(&event);
}

void WineIMEDelegate::SendEnterKeyEvent(const int enterKeyType) {
    if (!ime_event_callback_) return;

    ime_event event;
    init_ime_event(event);
    event.data.enter_key_type = enterKeyType;
    event.type = OHOS_IME_SEND_FUNCTION_KEY;
    ime_event_callback_(&event);
}

void WineIMEDelegate::MoveCursor(int direction) {
    if (!ime_event_callback_) return;

    ime_event event;
    init_ime_event(event);
    event.type = OHOS_IME_MOVE_CURSOR;
    event.data.move_direction = direction;
    ime_event_callback_(&event);
}

void WineIMEDelegate::ExitFullscreenEvent() {}

}  // namespace adapter
}  // namespace ohos
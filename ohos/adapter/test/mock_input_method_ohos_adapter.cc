/*
 * Copyright (c) 2023-2025 Huawei Device Co., Ltd.
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

#include "mock_input_method_ohos_adapter.h"

namespace ohos::adapter {
InputMethodOHOSAdapter& InputMethodOHOSAdapter::GetInstance() {
  static InputMethodOHOSAdapter instance;
  return instance;
}

void InputMethodOHOSAdapter::AttachTextInput(IMFAdapterTextConfig textConfig,
                                             int32_t requestKeyboardReason) {
  MockImeAdapter* instance = MockImeAdapter::GetInstance();
  instance->AttachTextInput(textConfig, requestKeyboardReason);
}

void InputMethodOHOSAdapter::DetachTextInput() {
  MockImeAdapter* instance = MockImeAdapter::GetInstance();
  instance->DetachTextInput();
}
void InputMethodOHOSAdapter::OffListenIME() {
  MockImeAdapter* instance = MockImeAdapter::GetInstance();
  instance->OffListenIME();
}

void InputMethodOHOSAdapter::ShowTextInput(int32_t requestKeyboardReason) {
  MockImeAdapter* instance = MockImeAdapter::GetInstance();
  instance->ShowTextInput(requestKeyboardReason);
}

void InputMethodOHOSAdapter::UpdateAttribute(
    IMFAdapterInputAttribute inputAttribute) {
  MockImeAdapter* instance = MockImeAdapter::GetInstance();
  instance->UpdateAttribute(inputAttribute);
}

void InputMethodOHOSAdapter::RegisterInsertTextCallback(
    InsertTextCallback callback) {
  MockImeAdapter* instance = MockImeAdapter::GetInstance();
  instance->RegisterInsertTextCallback(callback);
}

void InputMethodOHOSAdapter::RegisterDeleteBackwardCallback(
    DeleteBackCallback callback) {
  MockImeAdapter* instance = MockImeAdapter::GetInstance();
  instance->RegisterDeleteBackwardCallback(callback);
}

void InputMethodOHOSAdapter::RegisterDeleteForwardCallback(
    DeleteForwardCallback callback) {
  MockImeAdapter* instance = MockImeAdapter::GetInstance();
  instance->RegisterDeleteForwardCallback(callback);
}

void InputMethodOHOSAdapter::RegisterSendEnterKeyEventCallback(
    SendEnterKeyEventCallback callback) {
  MockImeAdapter* instance = MockImeAdapter::GetInstance();
  instance->RegisterSendEnterKeyEventCallback(callback);
}

void InputMethodOHOSAdapter::RegisterExitFullscreenEventCallback(
    ExitFullscreenEventCallback callback) {
  MockImeAdapter* instance = MockImeAdapter::GetInstance();
  instance->RegisterExitFullscreenEventCallback(callback);
}

void InputMethodOHOSAdapter::RegisterMoveCursorCallback(
    MoveCursorCallback callback) {
  MockImeAdapter* instance = MockImeAdapter::GetInstance();
  instance->RegisterMoveCursorCallback(callback);
}

void InputMethodOHOSAdapter::NotifyCursorUpdate(
    const IMFAdapterCursorInfo cursorInfo) {
  MockImeAdapter* instance = MockImeAdapter::GetInstance();
  instance->NotifyCursorUpdate(cursorInfo);
}
}  // namespace ohos::adapter

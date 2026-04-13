// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "input_method_adapter.h"
#include "ohos/adapter/aki_hook/aki_hook.h"
#include "wine_ime_delegate.h"

namespace ohos {
namespace adapter {
InputMethodAdapter& InputMethodAdapter::GetInstance() {
  static InputMethodAdapter helper;
  return helper;
}

void InputMethodAdapter::AttachTextInput(bool showKeyboard,
                                             IMFAdapterTextConfig textConfig,
                                             int32_t requestKeyboardReason) {
  if (auto insert = ohos::adapter::GetJSFunction("IMFAdapter.AttachTextInput")) {
    auto insert_promise = std::make_shared<std::promise<bool>>();
    auto future = insert_promise->get_future();
    std::function<void(bool)> callback = [insert_promise](bool successful) {
      insert_promise->set_value(successful);
    };
    insert->Invoke<void>(showKeyboard, textConfig, requestKeyboardReason, callback);
    auto status = future.wait_for(std::chrono::seconds(3));
    if (status == std::future_status::timeout) {
      LOGE("IMFAdapter.AttachTextInput wait timeout");
      return;
    }
    bool result = future.get();
    if (!result) {
      LOGE("IMFAdapter.AttachTextInput run fail");
    }
  }
}

void InputMethodAdapter::DetachTextInput() {
  if (auto func = ohos::adapter::GetJSFunction("IMFAdapter.DetachTextInput")) {
    std::promise<bool> detach_promise;
    std::function<void(bool)> callback = [&detach_promise](bool successful) {
      detach_promise.set_value(successful);
    };
    func->Invoke<void>(callback);
    detach_promise.get_future().get();
  }
}

void InputMethodAdapter::OffListenIME() {
  if (auto func = ohos::adapter::GetJSFunction("IMFAdapter.OffListenIME")) {
    func->Invoke<void>();
  }
}

void InputMethodAdapter::UpdateAttribute(
    IMFAdapterInputAttribute inputAttribute) {
  if (auto func = ohos::adapter::GetJSFunction("IMFAdapter.UpdateAttribute")) {
    func->Invoke<void>(inputAttribute);
  }
}

void InputMethodAdapter::ShowTextInput(int32_t requestKeyboardReason) {
  if (auto func = ohos::adapter::GetJSFunction("IMFAdapter.ShowTextInput")) {
    func->Invoke<void>(requestKeyboardReason);
  }
}

void InputMethodAdapter::NotifyCursorUpdate(
    const IMFAdapterCursorInfo cursorInfo) {
  if (auto func = ohos::adapter::GetJSFunction("IMFAdapter.CursorUpdate")) {
    func->Invoke<void>(cursorInfo);
  }
}

void InputMethodAdapter::ExitFullscreenEvent() {
  ohos::adapter::WineIMEDelegate::GetInstance().ExitFullscreenEvent();
}

void InputMethodAdapter::InsertTextCallback(const std::string& text) {
  ohos::adapter::WineIMEDelegate::GetInstance().InsertText(text);
}

void InputMethodAdapter::DeleteBackCallback(int32_t length) {
  ohos::adapter::WineIMEDelegate::GetInstance().DeleteBackward(length);
}

void InputMethodAdapter::DeleteForwardCallback(int32_t length) {
  ohos::adapter::WineIMEDelegate::GetInstance().DeleteForward(length);
}

void InputMethodAdapter::SendEnterKeyEventCallback(const int enterKeyType) {
  ohos::adapter::WineIMEDelegate::GetInstance().SendEnterKeyEvent(enterKeyType);
}

void InputMethodAdapter::MoveCursorCallback(const int direction) {
  ohos::adapter::WineIMEDelegate::GetInstance().MoveCursor(direction);
}

void InsertTextCallback(const std::string& text) {
  ohos::adapter::InputMethodAdapter::GetInstance().InsertTextCallback(text);
}

void DeleteBackCallback(int32_t length) {
  ohos::adapter::InputMethodAdapter::GetInstance().DeleteBackCallback(length);
}

void DeleteForwardCallback(int32_t length) {
  ohos::adapter::InputMethodAdapter::GetInstance().DeleteForwardCallback(length);
}

void SendEnterKeyEventCallback(const int enterKeyType) {
  ohos::adapter::InputMethodAdapter::GetInstance().SendEnterKeyEventCallback(enterKeyType);
}

void MoveCursorCallback(const aki::Value direction) {
  int input_direction = direction.As<int>();
  ohos::adapter::InputMethodAdapter::GetInstance().MoveCursorCallback(input_direction);
}

JSBIND_CLASS(IMFAdapterTextConfig) {
  JSBIND_PROPERTY(inputAttribute);
  JSBIND_PROPERTY(cursorInfo);
}

JSBIND_CLASS(IMFAdapterInputAttribute) {
  JSBIND_PROPERTY(inputPattern);
  JSBIND_PROPERTY(enterKeyType);
}

JSBIND_CLASS(IMFAdapterCursorInfo) {
  JSBIND_PROPERTY(left);
  JSBIND_PROPERTY(top);
  JSBIND_PROPERTY(width);
  JSBIND_PROPERTY(height);
}

JSBIND_GLOBAL() {
  JSBIND_FUNCTION(InsertTextCallback);
  JSBIND_FUNCTION(DeleteBackCallback);
  JSBIND_FUNCTION(DeleteForwardCallback);
  JSBIND_FUNCTION(SendEnterKeyEventCallback);
  JSBIND_FUNCTION(MoveCursorCallback);
}
}  // namespace adapter
}  // namespace ohos

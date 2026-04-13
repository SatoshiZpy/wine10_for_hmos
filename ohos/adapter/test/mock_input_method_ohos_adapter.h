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
 
#ifndef OHOS_ADAPTER_TEST_MOCK_INPUT_METHOD_OHOS_ADAPTER_H_
#define OHOS_ADAPTER_TEST_MOCK_INPUT_METHOD_OHOS_ADAPTER_H_
 
#include <cstdint>
 
#include "mock_utils.h"
#include "ohos/adapter/ime_adapter/input_method_ohos_adapter.h"
 
namespace ohos::adapter {
 
class IImeAdapter {
 public:
  using InsertTextCallback = InputMethodOHOSAdapter::InsertTextCallback;
  using DeleteBackCallback = InputMethodOHOSAdapter::DeleteBackCallback;
  using DeleteForwardCallback = InputMethodOHOSAdapter::DeleteForwardCallback;
  using SendEnterKeyEventCallback =
      InputMethodOHOSAdapter::SendEnterKeyEventCallback;
  using ExitFullscreenEventCallback =
      InputMethodOHOSAdapter::ExitFullscreenEventCallback;
  using MoveCursorCallback = InputMethodOHOSAdapter::MoveCursorCallback;
 
  virtual void AttachTextInput(IMFAdapterTextConfig textConfig,
                               int32_t requestKeyboardReason) = 0;
  virtual void DetachTextInput() = 0;
  virtual void OffListenIME() = 0;
  virtual void UpdateAttribute(IMFAdapterInputAttribute inputAttribute) = 0;
  virtual void ShowTextInput(int32_t requestKeyboardReason) = 0;
  virtual void ExitFullscreenEvent() = 0;
  virtual InsertTextCallback GetInsertTextCallbcak() = 0;
  virtual DeleteBackCallback GetDeleteBackCallbcak() = 0;
  virtual DeleteForwardCallback GetDeleteForwardCallbcak() = 0;
  virtual SendEnterKeyEventCallback GetSendEnterKeyEventCallback() = 0;
  virtual ExitFullscreenEventCallback GetExitFullscreenEventCallback() = 0;
  virtual MoveCursorCallback GetMoveCursorCallbcak() = 0;
  virtual void RegisterSendEnterKeyEventCallback(
      SendEnterKeyEventCallback callback) = 0;
  virtual void RegisterInsertTextCallback(InsertTextCallback callback) = 0;
  virtual void RegisterDeleteForwardCallback(
      DeleteForwardCallback callback) = 0;
  virtual void RegisterDeleteBackwardCallback(DeleteBackCallback callback) = 0;
  virtual void RegisterMoveCursorCallback(MoveCursorCallback callback) = 0;
  virtual void RegisterExitFullscreenEventCallback(
      ExitFullscreenEventCallback callback) = 0;
  virtual void NotifyCursorUpdate(const IMFAdapterCursorInfo cursorInfo) = 0;
};
 
class MockImeAdapter : public IImeAdapter,
                       public SingletonHolder<MockImeAdapter> {
 public:
  MOCK_METHOD2(AttachTextInput, void(IMFAdapterTextConfig, int32_t));
  MOCK_METHOD0(DetachTextInput, void());
  MOCK_METHOD0(OffListenIME, void());
  MOCK_METHOD1(UpdateAttribute, void(IMFAdapterInputAttribute));
  MOCK_METHOD1(ShowTextInput, void(int32_t));
  MOCK_METHOD0(ExitFullscreenEvent, void());
  MOCK_METHOD0(GetInsertTextCallbcak, InsertTextCallback());
  MOCK_METHOD0(GetDeleteBackCallbcak, DeleteBackCallback());
  MOCK_METHOD0(GetDeleteForwardCallbcak, DeleteForwardCallback());
  MOCK_METHOD0(GetSendEnterKeyEventCallback, SendEnterKeyEventCallback());
  MOCK_METHOD0(GetExitFullscreenEventCallback, ExitFullscreenEventCallback());
  MOCK_METHOD0(GetMoveCursorCallbcak, MoveCursorCallback());
  MOCK_METHOD1(RegisterSendEnterKeyEventCallback,
               void(SendEnterKeyEventCallback));
  MOCK_METHOD1(RegisterInsertTextCallback, void(InsertTextCallback));
  MOCK_METHOD1(RegisterDeleteForwardCallback, void(DeleteForwardCallback));
  MOCK_METHOD1(RegisterDeleteBackwardCallback, void(DeleteBackCallback));
  MOCK_METHOD1(RegisterMoveCursorCallback, void(MoveCursorCallback));
  MOCK_METHOD1(RegisterExitFullscreenEventCallback,
               void(ExitFullscreenEventCallback));
  MOCK_METHOD1(NotifyCursorUpdate, void(const IMFAdapterCursorInfo));
};
}  // namespace ohos::adapter
 
#endif
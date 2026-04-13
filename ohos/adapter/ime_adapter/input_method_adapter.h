// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INPUT_METHOD_OHOS_ADAPTER_H_
#define INPUT_METHOD_OHOS_ADAPTER_H_

#include <cstdint>
#include <mutex>
#include <string>

#include "ohos/adapter/export.h"

namespace ohos {
namespace adapter {
enum IMFAdapterTextInputType {
  NONE_INPUT = -1,
  TEXT = 0,
  MULTILINE,
  NUMBER,
  PHONE,
  DATETIME,
  EMAIL_ADDRESS,
  URL,
  VISIBLE_PASSWORD,
};

struct IMFAdapterCursorInfo {
  double left = 0.0;
  double top = 0.0;
  double width = 0.0;
  double height = 0.0;
};

struct IMFAdapterInputAttribute {
  int32_t inputPattern = 0;
  int32_t enterKeyType = 0;
};

struct IMFAdapterSelectionRange {
  int32_t start = -1;
  int32_t end = -1;
};

struct IMFAdapterTextConfig {
  IMFAdapterInputAttribute inputAttribute = {};
  IMFAdapterCursorInfo cursorInfo = {};
  IMFAdapterSelectionRange range = {};
  uint32_t windowId = -1;
};

enum IMFAdapterEnterKeyType {
  UNSPECIFIED = 0,
  NONE_KEY,
  GO,
  SEARCH,
  SEND,
  NEXT,
  DONE,
  PREVIOUS
};

enum IMFAdapterDirection {
  NONE = 0,
  UP = 1,
  DOWN,
  LEFT,
  RIGHT,
};

class ADAPTER_EXPORT_API InputMethodAdapter {
public:
  InputMethodAdapter(InputMethodAdapter& other) = delete;
  void operator=(const InputMethodAdapter &) = delete;
  InputMethodAdapter(InputMethodAdapter &&) = delete;
  void operator=(InputMethodAdapter &&) = delete;
  static InputMethodAdapter& GetInstance();

  void AttachTextInput(bool showKeyboard, 
                       IMFAdapterTextConfig textConfig,
                       int32_t requestKeyboardReason);
  void DetachTextInput();
  void OffListenIME();
  void UpdateAttribute(IMFAdapterInputAttribute inputAttribute);
  void ShowTextInput(int32_t requestKeyboardReason);
  void ExitFullscreenEvent();
  void NotifyCursorUpdate(const IMFAdapterCursorInfo cursorInfo);
  void InsertTextCallback(const std::string& text);
  void DeleteBackCallback(int32_t length);
  void DeleteForwardCallback(int32_t length);
  void SendEnterKeyEventCallback(const int enterKeyType);
  void MoveCursorCallback(const int direction);

private:
  InputMethodAdapter() = default;
  ~InputMethodAdapter() = default;
};
}  // namespace adapter
}  // namespace ohos
#endif  // INPUT_METHOD_OHOS_ADAPTER_H_

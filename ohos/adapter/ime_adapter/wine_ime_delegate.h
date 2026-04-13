// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WINE_IME_DELEGATE_H_
#define WINE_IME_DELEGATE_H_

#include <cstdint>
#include <string>

#include "ime_event_callback.h"

namespace ohos {
namespace adapter {

class WineIMEDelegate {
public:
    WineIMEDelegate(WineIMEDelegate& other) = delete;
    void operator=(const WineIMEDelegate &) = delete;
    WineIMEDelegate(WineIMEDelegate &&) = delete;
    void operator=(WineIMEDelegate &&) = delete;
    static WineIMEDelegate& GetInstance();
    void RegisterIMEEventCallBack(ime_event_callback callback);
    void InsertText(const std::string& text);
    void DeleteBackward(int32_t length);
    void DeleteForward(int32_t length);
    void SendEnterKeyEvent(const int enterKeyType);
    void MoveCursor(int direction);
    void ExitFullscreenEvent();

private:
    WineIMEDelegate() = default;
    ~WineIMEDelegate();
    ime_event_callback ime_event_callback_;
};
}  // namespace adapter
}  // namespace ohos
#endif  // WINE_IME_DELEGATE_H_
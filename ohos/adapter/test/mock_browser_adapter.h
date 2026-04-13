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
#ifndef OHOS_ADAPTER_TEST_MOCK_BROWSER_ADAPTER_H_
#define OHOS_ADAPTER_TEST_MOCK_BROWSER_ADAPTER_H_

#include <cstdint>

#include "mock_utils.h"
#include "ohos/adapter/browser/browser_adapter.h"
#include "ohos/adapter/window/app_window_adapter.h"

namespace ohos::adapter {
class IBrowserAdapter {
 public:
  virtual void RegisterBrowserCallback(BrowserCallback callback) = 0;
  virtual BrowserCallback GetBrowserCallback() = 0;

  virtual CommandResult ExecuteCommand(const int opt_type,
                                       const aki::Value opt_value) = 0;

  virtual BrowserCloseResponse GetBrowserCloseResponse(int32_t id) = 0;
  virtual void SetBrowserCloseResponse(int32_t id,
                                       BrowserCloseResponse response) = 0;
};

class MockBrowserAdapter : public IBrowserAdapter,
                           public SingletonHolder<MockBrowserAdapter> {
 public:
  MOCK_METHOD1(RegisterBrowserCallback, void(BrowserCallback));
  MOCK_METHOD0(GetBrowserCallback, BrowserCallback());
  MOCK_METHOD2(ExecuteCommand, CommandResult(const int, const aki::Value));
  MOCK_METHOD1(GetBrowserCloseResponse, BrowserCloseResponse(int32_t));
  MOCK_METHOD2(SetBrowserCloseResponse, void(int32_t, BrowserCloseResponse));
};

}  // namespace ohos::adapter
#endif  // OHOS_ADAPTER_TEST_MOCK_BROWSER_ADAPTER_H_
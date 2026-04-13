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

#include "mock_browser_adapter.h"

namespace ohos::adapter {

void BrowserAdapter::RegisterBrowserCallback(BrowserCallback callback) {
  MockBrowserAdapter* instance = MockBrowserAdapter::GetInstance();
  instance->RegisterBrowserCallback(callback);
}

CommandResult BrowserAdapter::ExecuteCommand(const int opt_type,
                                             const aki::Value opt_value) {
  MockBrowserAdapter* instance = MockBrowserAdapter::GetInstance();
  return instance->ExecuteCommand(opt_type, opt_value);
}

BrowserCloseResponse BrowserAdapter::GetBrowserCloseResponse(int32_t id) {
  MockBrowserAdapter* instance = MockBrowserAdapter::GetInstance();
  return instance->GetBrowserCloseResponse(id);
}

void BrowserAdapter::SetBrowserCloseResponse(int32_t id,
                                             BrowserCloseResponse response) {
  MockBrowserAdapter* instance = MockBrowserAdapter::GetInstance();
  return instance->SetBrowserCloseResponse(id, response);
}
}  // namespace ohos::adapter
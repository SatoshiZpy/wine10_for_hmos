/*
 * Copyright (c) 2023-2025 Huawei Device Co., Ltd.
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of
 *    conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list
 *    of conditions and the following disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used
 *    to endorse or promote products derived from this software without specific prior written
 *    permission.
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
 
#ifndef OHOS_ADAPTER_TEST_MOCK_APP_WINDOW_ADAPTER_H_
#define OHOS_ADAPTER_TEST_MOCK_APP_WINDOW_ADAPTER_H_
 
#include <cstdint>
 
#include "mock_utils.h"
#include "ohos/adapter/window/adapter/app_window_adapter.h"
 
namespace ohos::adapter::window {
class IAppWindowAdapter {
 public:
  using ChangeSizeCallback = AppWindowAdapter::ChangeSizeCallback;

  virtual void Create(const NewWindowParam& param) = 0;
  virtual void Close(int32_t id) = 0;
  virtual void Show(int32_t id) = 0;
  virtual void Hide(int32_t id) = 0;
  virtual void Activate(int32_t id) = 0;
  virtual void SetFullscreen(int32_t id) = 0;
  virtual void SetBounds(int32_t id,
                         const WindowRect& rect,
                         ChangeSizeCallback callback) = 0;
  virtual void SetEnabled(bool enabled, int32_t id) = 0;
  virtual void Maximize(int32_t id) = 0;
  virtual void UnMaximize(int32_t id) = 0;
  virtual void Minimize(int32_t id) = 0;
  virtual void SetWindowLimits(int32_t min_width,
                       int32_t min_height,
                       int32_t max_width,
                       int32_t max_height,
                       int32_t id) = 0;
  virtual void SetTitle(const std::string& title, int32_t id) = 0;
  virtual bool Relaunch() = 0;
  virtual void StartWindowMoving(int32_t id) = 0;
  virtual std::vector<std::string> GetWindowsByCoordinate(
      const PointCoordinate& coordinate) = 0;
  virtual void Restore(int32_t id) = 0;
  virtual std::vector<int32_t> GetOriginWindowIds(std::vector<int32_t> window_ids) = 0;
  virtual bool ShiftWindowEvent(const int32_t source_window_id,
                        const int32_t target_window_id) = 0;
  virtual void StartWindowMovingWithOffset(const int32_t id,
                                   const float offset_x,
                                   const float offset_y) = 0;
};
 
class MockAppWindowAdapter : public IAppWindowAdapter, public ohos::adapter::SingletonHolder<MockAppWindowAdapter> {
 public:
  MOCK_METHOD1(Create, void(const NewWindowParam&));
  MOCK_METHOD1(Close, void(int32_t));
  MOCK_METHOD1(Show, void(int32_t));
  MOCK_METHOD1(Hide, void(int32_t));
  MOCK_METHOD1(Activate, void(int32_t));
  MOCK_METHOD1(SetFullscreen, void(int32_t));
  MOCK_METHOD3(SetBounds, void(int32_t, const WindowRect&, ChangeSizeCallback));
  MOCK_METHOD2(SetEnabled, void(bool, int32_t));
  MOCK_METHOD1(Maximize, void(int32_t));
  MOCK_METHOD1(UnMaximize, void(int32_t));
  MOCK_METHOD1(Minimize, void(int32_t));
  MOCK_METHOD5(SetWindowLimits,
               void(int32_t, int32_t, int32_t, int32_t, int32_t));
  MOCK_METHOD2(SetTitle, void(const std::string&, int32_t));
  MOCK_METHOD0(Relaunch, bool());
  MOCK_METHOD1(StartWindowMoving, void(int32_t));
  MOCK_METHOD1(GetWindowsByCoordinate,
               std::vector<std::string>(const PointCoordinate&));
  MOCK_METHOD1(Restore, void(int32_t));
  MOCK_METHOD1(GetOriginWindowIds, std::vector<int32_t>(std::vector<int32_t>));
  MOCK_METHOD2(ShiftWindowEvent, bool(const int32_t, const int32_t));
  MOCK_METHOD3(StartWindowMovingWithOffset, void(const int32_t, const float, const float));
};
 
}
#endif // OHOS_ADAPTER_TEST_MOCK_APP_WINDOW_ADAPTER_H_
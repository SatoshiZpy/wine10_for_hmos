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
 
#include "mock_app_window_adapter.h"
#include "ohos/adapter/xcomponent/adapter/window_adapter.h"
#include "ohos/adapter/xcomponent/xcomponent_manager.h"
 
namespace ohos::adapter::window {
using xcomponent::XComponentManager;
using xcomponent::WindowAdapter;
 
void AppWindowAdapter::Create(const NewWindowParam& param) {
  MockAppWindowAdapter* instance = MockAppWindowAdapter::GetInstance();
  instance->Create(param);
  XComponentManager* manager = XComponentManager::GetInstance();
  manager->GetOrCreateXComponent(param.window_id, "xcomponent");
  manager->OnWidgetAvailable(param.window_id);
  // There is no need for OH_NativeWindow instance, so just pass a nullptr.
  // It will make ohos_utils print window not exists log.
  WindowAdapter::GetInstance().AddWindow(param.window_id, nullptr);
}
 
void AppWindowAdapter::Close(int32_t id) {
  MockAppWindowAdapter* instance = MockAppWindowAdapter::GetInstance();
  instance->Close(id);
}
 
void AppWindowAdapter::Show(int32_t id) {
  MockAppWindowAdapter* instance = MockAppWindowAdapter::GetInstance();
  instance->Show(id);
}
 
void AppWindowAdapter::Hide(int32_t id) {
  MockAppWindowAdapter* instance = MockAppWindowAdapter::GetInstance();
  instance->Hide(id);
}
 
void AppWindowAdapter::Activate(int32_t id) {
  MockAppWindowAdapter* instance = MockAppWindowAdapter::GetInstance();
  instance->Activate(id);
}
 
void AppWindowAdapter::SetFullscreen(int32_t id) {
  MockAppWindowAdapter* instance = MockAppWindowAdapter::GetInstance();
  instance->SetFullscreen(id);
}

void AppWindowAdapter::SetBounds(int32_t id,
                                 const WindowRect& rect,
                                 ChangeSizeCallback callback) {
  MockAppWindowAdapter* instance = MockAppWindowAdapter::GetInstance();
  instance->SetBounds(id, rect, callback);
}

void AppWindowAdapter::SetEnabled(bool enable, int32_t id) {
  MockAppWindowAdapter* instance = MockAppWindowAdapter::GetInstance();
  instance->SetEnabled(enable, id);
}
 
void AppWindowAdapter::Maximize(int32_t id) {
  MockAppWindowAdapter* instance = MockAppWindowAdapter::GetInstance();
  instance->Maximize(id);
}
 
void AppWindowAdapter::UnMaximize(int32_t id) {
  MockAppWindowAdapter* instance = MockAppWindowAdapter::GetInstance();
  instance->UnMaximize(id);
}
 
void AppWindowAdapter::Minimize(int32_t id) {
  MockAppWindowAdapter* instance = MockAppWindowAdapter::GetInstance();
  instance->Minimize(id);
}
 
void AppWindowAdapter::SetWindowLimits(int32_t min_width,
                                       int32_t min_height,
                                       int32_t max_width,
                                       int32_t max_height,
                                       int32_t id) {
  MockAppWindowAdapter* instance = MockAppWindowAdapter::GetInstance();
  instance->SetWindowLimits(min_width, min_height, max_width, max_height, id);
}
 
void AppWindowAdapter::SetTitle(const std::string& title, int32_t id) {
  MockAppWindowAdapter* instance = MockAppWindowAdapter::GetInstance();
  instance->SetTitle(title, id);
}
 
bool AppWindowAdapter::Relaunch() {
  MockAppWindowAdapter* instance = MockAppWindowAdapter::GetInstance();
  return instance->Relaunch();
}
 
void AppWindowAdapter::StartWindowMoving(int32_t id) {
  MockAppWindowAdapter* instance = MockAppWindowAdapter::GetInstance();
  instance->StartWindowMoving(id);
}
 
std::vector<std::string> AppWindowAdapter::GetWindowsByCoordinate(
    const PointCoordinate& coordinate) {
  MockAppWindowAdapter* instance = MockAppWindowAdapter::GetInstance();
  return instance->GetWindowsByCoordinate(coordinate);
}
 
void AppWindowAdapter::Restore(int32_t id) {
  MockAppWindowAdapter* instance = MockAppWindowAdapter::GetInstance();
  instance->Restore(id);
}
 
void AppWindowAdapter::StartWindowMovingWithOffset(const int32_t id,
                                                   const float offset_x,
                                                   const float offset_y) {
  MockAppWindowAdapter* instance = MockAppWindowAdapter::GetInstance();
  instance->StartWindowMovingWithOffset(id, offset_x, offset_y);
}
 
std::vector<int32_t> AppWindowAdapter::GetOriginWindowIds(
    std::vector<int32_t> window_ids) {
  MockAppWindowAdapter* instance = MockAppWindowAdapter::GetInstance();
  return instance->GetOriginWindowIds(window_ids);
}
 
bool AppWindowAdapter::ShiftWindowEvent(const int32_t source_id,
                                        const int32_t target_id) {
  MockAppWindowAdapter* instance = MockAppWindowAdapter::GetInstance();
  return instance->ShiftWindowEvent(source_id, target_id);
}
 
}  // namespace ohos::adapter::window
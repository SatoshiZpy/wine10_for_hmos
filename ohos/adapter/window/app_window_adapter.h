// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_APP_WINDOW_APP_WINDOW_ADAPTER_H_
#define OHOS_ADAPTER_APP_WINDOW_APP_WINDOW_ADAPTER_H_

#include <cstdint>
#include <map>
#include <string>

#include "ohos/adapter/export.h"
#include "ohos/adapter/window/window_common.h"

namespace ohos::adapter::window {

class ADAPTER_EXPORT_API AppWindowAdapter {
 public:
  using ChangeSizeCallback = std::function<void()>;
  static AppWindowAdapter& GetInstance();

  void Create(const NewWindowParam& param);
  void Close(int32_t id);
  void Show(int32_t id);
  void Hide(int32_t id);
  void Activate(int32_t id);
  void SetFullscreen(int32_t id);
  void SetBounds(int32_t id,
                 const WindowRect& rect,
                 ChangeSizeCallback callback);
  void SetEnabled(bool enabled, int32_t id);
  void Maximize(int32_t id);
  void UnMaximize(int32_t id);
  void Minimize(int32_t id);
  void SetWindowLimits(int32_t min_width,
                       int32_t min_height,
                       int32_t max_width,
                       int32_t max_height,
                       int32_t id);
  void SetTitle(const std::string& title, int32_t id);
  bool Relaunch();
  void StartWindowMoving(int32_t id);
  std::vector<int32_t> GetWindowsByCoordinate(const PointCoordinate& coordinate);
  void Restore(int32_t id);
  std::vector<int32_t> GetOriginWindowIds(std::vector<int32_t> window_ids);
  bool ShiftWindowEvent(const int32_t source_window_id,
                        const int32_t target_window_id);
  void StartWindowMovingWithOffset(const int32_t id,
                                  const float offset_x,
                                  const float offset_y);
  void SetAlwaysOnTop(int32_t id, bool onTop);

 private:
  AppWindowAdapter() = default;
  ~AppWindowAdapter() = default;
};

}  // namespace ohos::adapter::window
#endif  // OHOS_ADAPTER_APP_WINDOW_APP_WINDOW_ADAPTER_H_

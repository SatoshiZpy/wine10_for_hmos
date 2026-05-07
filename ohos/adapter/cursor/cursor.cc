// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/cursor/cursor.h"
#include "ohos/adapter/aki_hook/aki_hook.h"
#include "ohos/adapter/common/trace.h"

namespace ohos::adapter {

constexpr int IMAGE_PIXEL_MAP = 4;

void SetCursorVisible(const bool visible) {
  if (auto func =
          ohos::adapter::GetJSFunction("CursorAdapter.SetCursorVisible")) {
    func->InvokeAsync<void>(visible);
  }
}

void SetCursor(int32_t id, const OhosCursorType cursor_type) {
  if (cursor_type == ohos::adapter::OhosCursorType::kNone) {
    SetCursorVisible(false);
  } else {
    if (auto func = ohos::adapter::GetJSFunction("CursorAdapter.SetCursor")) {
      SetCursorVisible(true);
      func->InvokeAsync<void>(id, cursor_type);
    }
  }
}

void SetCustomCursor(CustomCusorInfo& cursor_info) {
  if (auto func = ohos::adapter::GetJSFunction("CursorAdapter.SetCustomCursor")) {
    aki::ArrayBuffer arrayBuffer(
        reinterpret_cast<uint8_t*>(cursor_info.buff.get()),
        cursor_info.width * cursor_info.height * IMAGE_PIXEL_MAP);
    func->Invoke<void>(cursor_info.id, cursor_info.width, cursor_info.height,
                       cursor_info.hotspot_x, cursor_info.hotspot_y,
                       std::move(arrayBuffer));
  }
}

}  // namespace ohos::adapter

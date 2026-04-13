// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_DRAG_ADAPTER_DRAG_DROP_OHOS_ADAPTER_H
#define OHOS_ADAPTER_DRAG_ADAPTER_DRAG_DROP_OHOS_ADAPTER_H

#include <cstdint>
#include <memory>
#include <string>

#include <multimedia/image_framework/image_mdk_common.h>
#include <multimedia/image_framework/image_pixel_map_mdk.h>

#include "ohos/adapter/export.h"
#include "third_party/aki/include/aki/value/array_buffer.h"

namespace ohos {
namespace adapter {

struct OhosBasicDragData {
  std::string text;
  std::string url;
  std::string urlTitle;
  std::string html;
};

struct OhosDragInfo {
  OhosBasicDragData basicData;
  std::vector<std::string> filePaths;
  std::string imageTempUri;
  std::vector<uint8_t> bookmarkData;
  std::vector<uint8_t> webCustomData;
  int touchX;
  int touchY;
};

struct OhosDragParamToJs {
  OhosBasicDragData basicData;
  std::string imageTempUri;
  aki::ArrayBuffer bookmarkBuffer;
  aki::ArrayBuffer webCustomBuffer;
  aki::ArrayBuffer pixelBuffer;
  int height;
  int width;
  int widgetId;
  int touchX;
  int touchY;
};

class ADAPTER_EXPORT_API DragDropOhosAdapter {
 public:
  static DragDropOhosAdapter& GetInstance();
  DragDropOhosAdapter(const DragDropOhosAdapter&) = delete;
  DragDropOhosAdapter(DragDropOhosAdapter&&) = delete;
  DragDropOhosAdapter& operator=(const DragDropOhosAdapter&) = delete;
  virtual ~DragDropOhosAdapter() = default;

  void ExecuteDrag(int width,
                   int height,
                   std::shared_ptr<char[]> buff,
                   int32_t widget_id);
  void SetOhosDragInfo(OhosDragInfo drag_info) { drag_info_ = drag_info; }

  std::string GetDraggedExtensionFileName();
  void SetDraggedExtensionFileName(const std::string& file_name);
  bool GetDragTabCreatingWindow();
  void SetDragTabCreatingWindow(bool drag_tab_creating_window);

 private:
  DragDropOhosAdapter();
  OhosDragInfo drag_info_;
  std::string dragged_extension_file_name_;
};

}  // namespace adapter
}  // namespace ohos

#endif  // OHOS_ADAPTER_DRAG_ADAPTER_DRAG_DROP_OHOS_ADAPTER_H

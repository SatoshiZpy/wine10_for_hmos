// Copyright (c) 2025 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_DRAG_ADAPTER_DRAG_DROP_COMMON_H
#define OHOS_ADAPTER_DRAG_ADAPTER_DRAG_DROP_COMMON_H

#include <iostream>
#include <sstream>
#include <string>

#include "third_party/aki/include/aki/value/array_buffer.h"

namespace ohos {
namespace adapter {

struct OhosBasicDragData {
  std::string text;
  std::string url;
  std::string url_title;
  std::string html;
  std::vector<uint8_t> bookmark_data;
  std::vector<uint8_t> web_custom_data;
  std::string ToString() const {
    std::ostringstream oss;
    oss << "(text-length:" << text.length() << ", url-length:" << url.length() <<
           ", url_title-length:" << url_title.length() <<
           ", html-length:" << html.length() <<
           ", bookmark_data-size:" << bookmark_data.size() <<
           ", web_custom_data-size:" << web_custom_data.size() << ")";
    return oss.str();
  }
};
 
struct OhosStartDragParam {
  OhosBasicDragData basic_data;
  std::string web_image_file_path;
  std::shared_ptr<char[]> pixelmap_buffer;
  int pixelmap_width;
  int pixelmap_height;
  int pixelmap_touch_x;
  int pixelmap_touch_y;
  std::string ToString() const {
    std::ostringstream oss;
    oss << "(basic_data:" << basic_data.ToString() <<
           ", web_image_file_path-length:" << web_image_file_path.length() <<
           ", pixelmap_buffer.size: " << pixelmap_buffer.use_count() <<
           ",pixelmap_width:" << pixelmap_width <<
           ", pixelmap_height:" << pixelmap_height <<
           ", pixelmap_touch_x:" << pixelmap_touch_x <<
           ", pixelmap_touch_y:" << pixelmap_touch_y << ")";
    return oss.str();
  }
};

struct OhosDropData {
  OhosBasicDragData basic_data;
  std::vector<std::string> file_paths;
  std::string ToString() const {
    std::ostringstream oss;
    oss << "(basic_data:" << basic_data.ToString() <<
           ", file_paths.size: " << file_paths.size() << ")";
    return oss.str();
  }
};

}  // namespace adapter
}  // namespace ohos

#endif  // OHOS_ADAPTER_DRAG_ADAPTER_DRAG_DROP_COMMON_H

// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "ohos/adapter/export.h"

namespace ohos::adapter::ShapeDetectionAdapter {
class PixelPoint {
 public:
  PixelPoint() = default;
  PixelPoint(float x, float y) : x(x), y(y) {}

  float x;
  float y;
};

class ADAPTER_EXPORT_API TextLine {
 public:
  TextLine() = default;
  ~TextLine() = default;

  PixelPoint topLeft;
  PixelPoint topRight;
  PixelPoint bottomRight;
  PixelPoint bottomLeft;
  std::string value;
};

class ADAPTER_EXPORT_API Face {
 public:
  Face() = default;
  ~Face() = default;

  PixelPoint leftEye;
  PixelPoint rightEye;
  PixelPoint nose;
  PixelPoint mouth;
  float x;
  float y;
  float width;
  float height;
};

std::vector<TextLine> ADAPTER_EXPORT_API TextDetect(void* buffer,
                                                    size_t len,
                                                    int width,
                                                    int height);
std::vector<Face> ADAPTER_EXPORT_API FaceDetect(void* buffer,
                                                size_t len,
                                                int width,
                                                int height);
}  // namespace ohos::adapter::ShapeDetectionAdapter

// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_OCR_OCR_ADAPTER_H
#define OHOS_ADAPTER_OCR_OCR_ADAPTER_H

#include <cstdint>
#include <memory>
#include <string>

#include "ohos/adapter/aki_hook/aki_hook.h"
#include "ohos/adapter/export.h"

namespace ohos {
namespace adapter {

struct PixelPoint {
  int x;
  int y;
};

struct TextWord {
  std::string value;
  std::vector<PixelPoint> cornerPoints;
};

struct OcrImage {
  int width = 0;
  int height = 0;
  std::unique_ptr<char[]> buff;
};

struct OcrAdapterImage {
  int width = 0;
  int height = 0;
  aki::ArrayBuffer buff;
};

class ADAPTER_EXPORT_API OcrAdapter {
 public:
  using OnTextRecognizedCallback = std::function<void(std::vector<TextWord>)>;

  static OcrAdapter& GetInstance();
  OcrAdapter(const OcrAdapter&) = delete;
  OcrAdapter(OcrAdapter&&) = delete;
  OcrAdapter& operator=(const OcrAdapter&) = delete;
  virtual ~OcrAdapter() = default;
  std::vector<TextWord> OcrFunction(const OcrImage& image);

 private:
  OcrAdapter();
};

}  // namespace adapter
}  // namespace ohos
#endif

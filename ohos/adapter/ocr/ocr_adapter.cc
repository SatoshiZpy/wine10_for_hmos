// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/ocr/ocr_adapter.h"
#include "ohos/adapter/common/logging.h"

namespace ohos {
namespace adapter {

namespace {
const int kImagePixelMap = 4;
const int kWordCornerNum = 4;
}  // namespace

OcrAdapter::OcrAdapter() {}

OcrAdapter& OcrAdapter::GetInstance() {
  static OcrAdapter adapter;
  return adapter;
}

std::vector<TextWord> OcrAdapter::OcrFunction(const OcrImage& image) {
  if (image.width == 0 || image.height == 0) {
    LOGE("OcrAdapter::OcrFunction image is null");
    return {};
  }
  auto func = ohos::adapter::GetJSFunction("OcrAdapter.OcrFunction");
  if (!func) {
    LOGE("OcrAdapter::OcrFunction GetJSFunction OcrAdapter.OcrFunction fail!");
    return {};
  }

  aki::ArrayBuffer arrayBuffer((uint8_t*)image.buff.get(),
                               image.width * image.height * kImagePixelMap);
  OcrAdapterImage ocr_adapter_image = {image.width, image.height, arrayBuffer};
  std::promise<std::vector<TextWord>> ocr_promise;
  std::function<void(aki::Value, int32_t)> callback =
    [&](aki::Value words, int32_t length) -> void {
    std::vector<TextWord> recognizeWords;
    if (!words.IsArray()) {
      ocr_promise.set_value(recognizeWords);
      return;
    }
    for (int32_t i = 0; i < length; i++) {
      TextWord word;
      word.value = words[i]["value"].As<std::string>();
      aki::Value cornerPoints = words[i]["cornerPoints"];
      for (int j = 0; j < kWordCornerNum; j++) {
        PixelPoint point;
        point.x = cornerPoints[j]["x"].As<int>();
        point.y = cornerPoints[j]["y"].As<int>();
        word.cornerPoints.push_back(point);
      }
      recognizeWords.push_back(word);
    }
    ocr_promise.set_value(recognizeWords);
  };
  func->Invoke<void>(callback, ocr_adapter_image);
  std::vector<TextWord> words = ocr_promise.get_future().get();

  return words;
}

JSBIND_CLASS(PixelPoint) {
  JSBIND_PROPERTY(x);
  JSBIND_PROPERTY(y);
}

JSBIND_CLASS(TextWord) {
  JSBIND_PROPERTY(value);
  JSBIND_PROPERTY(cornerPoints);
}

JSBIND_CLASS(OcrAdapterImage) {
  JSBIND_PROPERTY(width);
  JSBIND_PROPERTY(height);
  JSBIND_PROPERTY(buff);
}

}  // namespace adapter
}  // namespace ohos

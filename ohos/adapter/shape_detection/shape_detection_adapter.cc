// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/shape_detection/shape_detection_adapter.h"

#include <functional>
#include <future>

#include "ohos/adapter/aki_hook/aki_hook.h"
#include "ohos/adapter/common/logging.h"

namespace ohos::adapter::ShapeDetectionAdapter {
std::vector<TextLine> TextDetect(void* buffer,
                                 size_t len,
                                 int width,
                                 int height) {
  std::vector<TextLine> textLines;
  std::promise<bool> promise;
  std::function<void(aki::Value, int32_t)> set_value =
      [&](aki::Value lineInfo, int32_t count) -> void {
    for (int32_t i = 0; i < count; ++i) {
      TextLine line;
      line.topLeft.x = lineInfo[i]["topLeft"]["x"].As<float>();
      line.topLeft.y = lineInfo[i]["topLeft"]["y"].As<float>();
      line.topRight.x = lineInfo[i]["topRight"]["x"].As<float>();
      line.topRight.y = lineInfo[i]["topRight"]["y"].As<float>();
      line.bottomRight.x = lineInfo[i]["bottomRight"]["x"].As<float>();
      line.bottomRight.y = lineInfo[i]["bottomRight"]["y"].As<float>();
      line.bottomLeft.x = lineInfo[i]["bottomLeft"]["x"].As<float>();
      line.bottomLeft.y = lineInfo[i]["bottomLeft"]["y"].As<float>();
      line.value = lineInfo[i]["value"].As<std::string>();
      textLines.push_back(line);
    }
    promise.set_value(true);
  };
  if (auto getDirFunc = GetJSFunction("ShapeDetectionAdapter.TextDetect")) {
    uint8_t* ptr = static_cast<uint8_t*>(buffer);
    aki::ArrayBuffer arr_buffer(ptr, len);
    getDirFunc->Invoke<void>(arr_buffer, width, height, set_value);
    auto result = promise.get_future().get();
    return textLines;
  }
  LOGE("Get ShapeDetectionAdapter.TextDetect js function failed.");
  return textLines;
}

std::vector<Face> FaceDetect(void* buffer, size_t len, int width, int height) {
  std::vector<Face> faceVec;
  std::promise<bool> promise;
  std::function<void(aki::Value, int32_t)> set_value =
      [&](aki::Value faceInfo, int32_t count) -> void {
    for (int32_t i = 0; i < count; ++i) {
      Face face;
      face.leftEye.x = faceInfo[i]["leftEye"]["x"].As<float>();
      face.leftEye.y = faceInfo[i]["leftEye"]["y"].As<float>();
      face.rightEye.x = faceInfo[i]["rightEye"]["x"].As<float>();
      face.rightEye.y = faceInfo[i]["rightEye"]["y"].As<float>();
      face.nose.x = faceInfo[i]["nose"]["x"].As<float>();
      face.nose.y = faceInfo[i]["nose"]["y"].As<float>();
      face.mouth.x = faceInfo[i]["mouth"]["x"].As<float>();
      face.mouth.y = faceInfo[i]["mouth"]["y"].As<float>();
      face.x = faceInfo[i]["x"].As<float>();
      face.y = faceInfo[i]["y"].As<float>();
      face.width = faceInfo[i]["width"].As<float>();
      face.height = faceInfo[i]["height"].As<float>();
      faceVec.push_back(face);
    }
    promise.set_value(true);
  };
  if (auto getDirFunc = GetJSFunction("ShapeDetectionAdapter.FaceDetect")) {
    uint8_t* ptr = static_cast<uint8_t*>(buffer);
    aki::ArrayBuffer arr_buffer(ptr, len);
    getDirFunc->Invoke<void>(arr_buffer, width, height, set_value);
    auto result = promise.get_future().get();
    return faceVec;
  }
  LOGE("Get ShapeDetectionAdapter.FaceDetect js function failed.");
  return faceVec;
}
}  // namespace ohos::adapter::ShapeDetectionAdapter
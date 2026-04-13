// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media_adapter.h"

#include <fstream>
#include <iosfwd>
#include <iostream>
#include <thread>

#include "ohos/adapter/aki_hook/aki_hook.h"
#include "ohos/adapter/common/logging.h"

namespace ohos {
namespace adapter {

MediaAdapter& MediaAdapter::GetInstance() {
  static MediaAdapter helper;
  return helper;
}
void MediaAdapter::RegisterBufferAvailableCallback(
    BufferAvailableCallback callback) {
  if (callback != nullptr) {
    bufferAvailableCallback_ = callback;
  }
}
std::string MediaAdapter::GetPreviewSurfaceId() {
  std::string surfaceId;
  std::thread t([&] {
    std::promise<std::string> promise;
    std::function<void(std::string)> callback = [&](std::string message) {
      promise.set_value(message);
    };
    auto insert =
        ohos::adapter::GetJSFunction("mediaAdapter.getPreviewSurfaceId");
    if (insert) {
      insert->Invoke<void>(callback);
      surfaceId = promise.get_future().get();
    }
  });
  t.join();
  return surfaceId;
}

napi_value MediaAdapter::ReadImageFromReceiver(napi_env env,
                                               napi_callback_info info) {
  size_t argc = 1;
  napi_value args[2] = {nullptr};
  napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  napi_valuetype valuetype0;
  napi_typeof(env, args[0], &valuetype0);
  napi_ref reference;
  napi_create_reference(env, args[0], 1, &reference);
  napi_value imgReceiver_js;
  napi_get_reference_value(env, reference, &imgReceiver_js);

  ImageReceiverNative* imgReceiver_c =
      OH_Image_Receiver_InitImageReceiverNative(env, imgReceiver_js);

  int32_t capacity;
  OH_Image_Receiver_GetCapacity(imgReceiver_c, &capacity);

  int32_t format;
  OH_Image_Receiver_GetFormat(imgReceiver_c, &format);

  OhosImageSize size;
  OH_Image_Receiver_GetSize(imgReceiver_c, &size);

  int32_t ret;
  napi_value nextImage;
  ret = OH_Image_Receiver_ReadNextImage(imgReceiver_c, &nextImage);

  ImageNative* nextImage_native = OH_Image_InitImageNative(env, nextImage);

  OhosImageSize imageSize;
  OH_Image_Size(nextImage_native, &imageSize);

  OhosImageComponent imgComponent;
  ret = OH_Image_GetComponent(nextImage_native, OHOS_IMAGE_COMPONENT_FORMAT_JPEG, &imgComponent);

  if (MediaAdapter::GetInstance().GetBufferAvailableCallback() != nullptr) {
    MediaAdapter::GetInstance().GetBufferAvailableCallback()(
        imgComponent.byteBuffer, imgComponent.size);
  }

  ret = OH_Image_Release(nextImage_native);
  ret = OH_Image_Receiver_Release(imgReceiver_c);
  return nextImage;
}

void MediaAdapter::InitImageReceiver() {
  auto insert = ohos::adapter::GetJSFunction("mediaAdapter.getImageReceiver");
  if (insert) {
    insert->Invoke<void>();
  }
} 

int32_t MediaAdapter::GetCameraOrientation() {
  auto insert = ohos::adapter::GetJSFunction("mediaAdapter.getCameraOrientation");
  if (insert) {
    return insert->Invoke<int32_t>();
  }
  LOGE("GetJSFunction mediaAdapter.getCameraOrientation failed");
  return 0;
}
}  // namespace adapter
}  // namespace ohos

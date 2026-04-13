// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_window_parcel.h"

#include "ohos/adapter/common/logging.h"

#include <cstdint>

namespace ohos::adapter::multiprocess {

static NativeWindowOperation nativeWindowGetInt32Ops[] = {
    GET_FORMAT,
    GET_STRIDE,
    GET_SWAP_INTERVAL,
    GET_TIMEOUT,
    GET_COLOR_GAMUT,
    GET_TRANSFORM,
    GET_SOURCE_TYPE
};

static NativeWindowOperation nativeWindowSetInt32Ops[] = {
    SET_FORMAT,
    SET_STRIDE,
    SET_SWAP_INTERVAL,
    SET_TIMEOUT,
    SET_COLOR_GAMUT,
    SET_TRANSFORM,
    SET_SOURCE_TYPE
};

static NativeWindowOperation nativeWindowGetInt64Ops[] = {
    GET_USAGE
};

static NativeWindowOperation nativeWindowSetInt64Ops[] = {
    SET_USAGE
};

static int WriteInt32AttrToParcel(NativeWindowOperation op,
                                  void* window,
                                  OHIPCParcel* data_parcel) {
  int32_t val;
  int32_t ret = OH_NativeWindow_NativeWindowHandleOpt((OHNativeWindow*)window, op, &val);
  if (ret != 0) {
    LOGE("Native window %{public}d error: %{public}d", op, ret);
    return OH_IPC_PARCEL_WRITE_ERROR;
  }
  ret = OH_IPCParcel_WriteInt32(data_parcel, val);
  if (ret != OH_IPC_SUCCESS) {
    LOGE("Native window write %{public}d into parcel error: %{public}d", op,
         ret);
    return ret;
  }

  return 0;
}

static int ReadInt32AttrFromParcel(NativeWindowOperation op,
                                   void* window,
                                   const OHIPCParcel* data_parcel) {
  int32_t val;
  int32_t ret = OH_IPCParcel_ReadInt32(data_parcel, &val);
  if (ret != OH_IPC_SUCCESS) {
    LOGE("Native window read %{public}d from parcel error: %{public}d", op,
         ret);
    return OH_IPC_PARCEL_READ_ERROR;
  }

  ret = OH_NativeWindow_NativeWindowHandleOpt((OHNativeWindow*)window, op, val);
  if (ret != 0) {
    LOGE("Native window %{public}d error: %{public}d", op, ret);
    return ret;
  }

  return 0;
}

static int WriteInt64AttrToParcel(NativeWindowOperation op,
                                  void* window,
                                  OHIPCParcel* data_parcel) {
  int64_t val;
  int32_t ret = OH_NativeWindow_NativeWindowHandleOpt((OHNativeWindow*)window, op, &val);
  if (ret != 0) {
    LOGE("Native window %{public}d error: %{public}d", op, ret);
    return OH_IPC_PARCEL_WRITE_ERROR;
  }
  ret = OH_IPCParcel_WriteInt64(data_parcel, val);
  if (ret != OH_IPC_SUCCESS) {
    LOGE("Native window write %{public}d into parcel error: %{public}d", op,
         ret);
    return ret;
  }

  return 0;
}

static int ReadInt64AttrFromParcel(NativeWindowOperation op,
                                   void* window,
                                   const OHIPCParcel* data_parcel) {
  int64_t val;
  int32_t ret = OH_IPCParcel_ReadInt64(data_parcel, &val);
  if (ret != OH_IPC_SUCCESS) {
    LOGE("Native window read %{public}d from parcel error: %{public}d", op,
         ret);
    return OH_IPC_PARCEL_READ_ERROR;
  }

  ret = OH_NativeWindow_NativeWindowHandleOpt((OHNativeWindow*)window, op, val);
  if (ret != 0) {
    LOGE("Native window %{public}d error: %{public}d", op, ret);
    return ret;
  }

  return 0;
}

int NativeWindowAttrToParcel(void* window, OHIPCParcel* data_parcel) {
  int32_t height;
  int32_t width;
  int32_t ret = OH_NativeWindow_NativeWindowHandleOpt(
      (OHNativeWindow*)window, GET_BUFFER_GEOMETRY, &height, &width);
  if (ret != 0) {
    LOGE("Native window GET_BUFFER_GEOMETRY error: %{public}d", ret);
    return OH_IPC_PARCEL_WRITE_ERROR;
  }

  ret = OH_IPCParcel_WriteInt32(data_parcel, height);
  if (ret != OH_IPC_SUCCESS) {
    LOGE("Native window write height into parcel error: %{public}d", ret);
    return ret;
  }
  ret = OH_IPCParcel_WriteInt32(data_parcel, width);
  if (ret != OH_IPC_SUCCESS) {
    LOGE("Native window write width into parcel error: %{public}d", ret);
    return ret;
  }

  for (NativeWindowOperation op : nativeWindowGetInt32Ops) {
    if (WriteInt32AttrToParcel(op, window, data_parcel) != 0) {
      return OH_IPC_PARCEL_WRITE_ERROR;
    }
  }
  for (NativeWindowOperation op : nativeWindowGetInt64Ops) {
    if (WriteInt64AttrToParcel(op, window, data_parcel) != 0) {
      return OH_IPC_PARCEL_WRITE_ERROR;
    }
  }

  char app_framework_type[64] = {'\0'};
  ret = OH_NativeWindow_NativeWindowHandleOpt(
      (OHNativeWindow*)window, GET_APP_FRAMEWORK_TYPE, app_framework_type);
  if (ret != 0) {
    LOGE("Native window %{public}d error: %{public}d", GET_APP_FRAMEWORK_TYPE, ret);
    return OH_IPC_PARCEL_WRITE_ERROR;
  }
  ret = OH_IPCParcel_WriteString(data_parcel, app_framework_type);
  if (ret != OH_IPC_SUCCESS) {
    LOGE("Native window write %{public}d into parcel error: %{public}d",
         GET_APP_FRAMEWORK_TYPE, ret);
    return ret;
  }

  return OH_IPC_SUCCESS;
}

int NativeWindowAttrFromParcel(const OHIPCParcel* data_parcel, void* window) {
  int32_t height;
  int32_t width;
  int32_t ret = OH_IPCParcel_ReadInt32(data_parcel, &height);
  if (ret != OH_IPC_SUCCESS) {
    LOGE("Native window read height from parcel error: %{public}d", ret);
    return static_cast<OH_IPC_ErrorCode>(ret);
  }

  ret = OH_IPCParcel_ReadInt32(data_parcel, &width);
  if (ret != OH_IPC_SUCCESS) {
    LOGE("Native window read width from parcel error: %{public}d", ret);
    return static_cast<OH_IPC_ErrorCode>(ret);
  }

  ret = OH_NativeWindow_NativeWindowHandleOpt(
      (OHNativeWindow*)window, SET_BUFFER_GEOMETRY, width, height);
  if (ret != 0) {
    LOGE("Native window set window size error: %{public}d", ret);
    return OH_IPC_PARCEL_READ_ERROR;
  }

  for (NativeWindowOperation op : nativeWindowSetInt32Ops) {
    if (ReadInt32AttrFromParcel(op, window, data_parcel) != 0) {
      return OH_IPC_PARCEL_READ_ERROR;
    }
  }
  for (NativeWindowOperation op : nativeWindowSetInt64Ops) {
    if (ReadInt64AttrFromParcel(op, window, data_parcel) != 0) {
      return OH_IPC_PARCEL_READ_ERROR;
    }
  }

  const char* app_framework_type = OH_IPCParcel_ReadString(data_parcel);
  if (app_framework_type == nullptr) {
    LOGE("Native window read appFrameworkType from parcel failed.");
    return OH_IPC_PARCEL_READ_ERROR;
  }
  ret = OH_NativeWindow_NativeWindowHandleOpt(
      (OHNativeWindow*)window, SET_APP_FRAMEWORK_TYPE, app_framework_type);
  if (ret != 0) {
    LOGE("Native window %{public}d error: %{public}d", SET_APP_FRAMEWORK_TYPE, ret);
    return OH_IPC_PARCEL_READ_ERROR;
  }

  return OH_IPC_SUCCESS;
}

int NativeWindowWriteToParcel(void* window, OHIPCParcel* data_parcel) {
  int32_t ret = OH_NativeWindow_WriteToParcel((OHNativeWindow*)window, data_parcel);
  if (ret != 0) {
    LOGE("Write native window into data parcel error: %{public}d", ret);
    return OH_IPC_PARCEL_WRITE_ERROR;
  }

  if (NativeWindowAttrToParcel(window, data_parcel) != OH_IPC_SUCCESS) {
    LOGE("Native window write attr to parcel error.");
    return OH_IPC_PARCEL_WRITE_ERROR;
  }

  return OH_IPC_SUCCESS;
}

int NativeWindowReadFromParcel(const OHIPCParcel* data_parcel, void** window) {
  OHNativeWindow* native_window;
  int32_t ret = OH_NativeWindow_ReadFromParcel(
      const_cast<OHIPCParcel*>(data_parcel), &native_window);
  if (ret != 0 || native_window == nullptr) {
    LOGE("Native window read from parcel error: %{public}d, "
        "pointer is null? %{public}d", ret, native_window == nullptr);
    return OH_IPC_PARCEL_READ_ERROR;
  }

  if (NativeWindowAttrFromParcel(data_parcel, native_window) != OH_IPC_SUCCESS) {
    LOGE("Native window setup attr from parcel error.");
    return OH_IPC_PARCEL_READ_ERROR;
  }

  *window = native_window;

  return OH_IPC_SUCCESS;
}
}  // namespace ohos::adapter::multiprocess

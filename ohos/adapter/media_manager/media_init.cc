// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/media_manager/media_init.h"

#include <js_native_api.h>
#include <js_native_api_types.h>

#include "ohos/adapter/common/constants.h"
#include "ohos/adapter/common/logging.h"
#include "ohos/adapter/media_manager/media_adapter.h"

namespace ohos::adapter::media {

static napi_value ReadImageFromReceiver(napi_env env, napi_callback_info info) {
  return ohos::adapter::MediaAdapter::ReadImageFromReceiver(env, info);
}

void Register(napi_env env, napi_value exports) {
  napi_property_descriptor desc[] = {
      DECLARE_NAPI_METHOD("readImageFromReceiver", media::ReadImageFromReceiver)
  };
  if (napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]),
                             desc) != napi_ok) {
    LOGE("napi_define_properties failed");
    return;
  }
}

} // namespace ohos::adapter::media

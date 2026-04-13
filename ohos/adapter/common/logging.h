// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_COMMON_LOGGING_H_
#define OHOS_ADAPTER_COMMON_LOGGING_H_

#include <hilog/log.h>

#define APP_LOG_DOMAIN 0x0001
#define APP_LOG_TAG "OHWineAdapter"

#define LOGI(fmt, ...)                                                                                             \
    ((void)OH_LOG_Print(LOG_APP, LOG_INFO, APP_LOG_DOMAIN, APP_LOG_TAG, "[(%{public}s:%{public}d)%{public}s]" fmt, \
                        __FILE_NAME__, __LINE__, __FUNCTION__, ##__VA_ARGS__))

#define LOGD(fmt, ...)                                                                                              \
    ((void)OH_LOG_Print(LOG_APP, LOG_DEBUG, APP_LOG_DOMAIN, APP_LOG_TAG, "[(%{public}s:%{public}d)%{public}s]" fmt, \
                        __FILE_NAME__, __LINE__, __FUNCTION__, ##__VA_ARGS__))

#define LOGW(fmt, ...)                                                                                             \
    ((void)OH_LOG_Print(LOG_APP, LOG_WARN, APP_LOG_DOMAIN, APP_LOG_TAG, "[(%{public}s:%{public}d)%{public}s]" fmt, \
                        __FILE_NAME__, __LINE__, __FUNCTION__, ##__VA_ARGS__))

#define LOGE(fmt, ...)                                                                                              \
    ((void)OH_LOG_Print(LOG_APP, LOG_ERROR, APP_LOG_DOMAIN, APP_LOG_TAG, "[(%{public}s:%{public}d)%{public}s]" fmt, \
                        __FILE_NAME__, __LINE__, __FUNCTION__, ##__VA_ARGS__))

#endif // OHOS_ADAPTER_COMMON_LOGGING_H_

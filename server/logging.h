// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef __WINE_SERVER_LOGGING_H_
#define __WINE_SERVER_LOGGING_H_

#ifdef __OHOS__
#include <hilog/log.h>
#define LOG_DOMAIN 0x3200
#define LOG_TAG "OHWineServer"
#define LOGI(...)                                                     \
  ((void)OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, LOG_TAG, \
                      __VA_ARGS__))

#define LOGD(...)                                                      \
  ((void)OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_DOMAIN, LOG_TAG, \
                      __VA_ARGS__))

#define LOGW(...)                                                     \
  ((void)OH_LOG_Print(LOG_APP, LOG_WARN, LOG_DOMAIN, LOG_TAG, \
                      __VA_ARGS__))

#define LOGE(...)                                                      \
  ((void)OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_DOMAIN, LOG_TAG, \
                      __VA_ARGS__))
#else
#define LOGI(...)   do { } while(0)
#define LOGD(...)   do { } while(0)
#define LOGW(...)   do { } while(0)
#define LOGE(...)   do { } while(0)
#endif

#endif  // __WINE_SERVER_LOGGING_H_

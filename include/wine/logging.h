// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if 0
#pragma makedep install
#endif

#ifndef __WINE_WINE_LOGGING_H
#define __WINE_WINE_LOGGING_H

#ifdef __OHOS__
#include <hilog/log.h>
#define LOG_DOMAIN 0x3200
#define LOG_TAG "OHWine"
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

#endif  // __WINE_WINE_LOGGING_H

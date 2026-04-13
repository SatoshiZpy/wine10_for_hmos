// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_EXPORT_API_H_
#define OHOS_ADAPTER_EXPORT_API_H_

#if defined(WIN32)
#define ADAPTER_EXPORT_API __declspec(dllexport)
#else  // defined(WIN32)
#define ADAPTER_EXPORT_API __attribute__((visibility("default")))
#endif

#endif  // OHOS_ADAPTER_EXPORT_API_H_

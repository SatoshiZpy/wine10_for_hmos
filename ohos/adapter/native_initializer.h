// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_NATIVE_INITIALIZER_H_
#define OHOS_ADAPTER_NATIVE_INITIALIZER_H_
extern "C" __attribute__((constructor)) void RegisterEntryModule(void);

#endif // OHOS_ADAPTER_NATIVE_INITIALIZER_H_
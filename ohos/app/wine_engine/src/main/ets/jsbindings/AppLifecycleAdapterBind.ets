// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import JsBindingUtils from '../utils/JsBindingUtils';
import Inject from '../common/InjectModule';
import lazy { AppLifecycleAdapter } from '../adapter/AppLifecycleAdapter';


function implAppLifecycleAdapter(): AppLifecycleAdapter {
  return Inject.getOrCreate(AppLifecycleAdapter);
}

function onCreate() {
  implAppLifecycleAdapter().onCreate();
}

function onDestroy() {
  implAppLifecycleAdapter().onDestroy();
}

export class AppLifecycleAdapterBind {
  static bind() {
    JsBindingUtils.bindFunction("AppLifecycleAdapter.onCreate", onCreate);
    JsBindingUtils.bindFunction("AppLifecycleAdapter.onDestroy", onDestroy);
  }
}

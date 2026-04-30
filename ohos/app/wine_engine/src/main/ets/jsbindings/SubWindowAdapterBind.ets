// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import JsBindingUtils from '../utils/JsBindingUtils';
import Inject from '../common/InjectModule';
import lazy { SubWindowAdapter } from '../adapter/SubWindowAdapter';
import type { NewWindowParam, WindowBound } from '../interface/CommonInterface';

function implSubWindowAdapter(): SubWindowAdapter {
  return Inject.getOrCreate(SubWindowAdapter);
}

function createSubWindow(param: NewWindowParam, callback: (id: number) => void) {
  implSubWindowAdapter().createSubWindow(param, callback)
}

function showSubWindow(id: number) {
  implSubWindowAdapter().showSubWindow(id);
}

function hideSubWindow(id: number) {
  implSubWindowAdapter().hideSubWindow(id);
}

function closeSubWindow(id: number) {
  implSubWindowAdapter().closeSubWindow(id);
}

function setSubWindowBounds(id: number, r: WindowBound) {
  implSubWindowAdapter().setSubWindowBounds(id, r);
}

export class SubWindowAdapterBind {
  static bind() {
    JsBindingUtils.bindFunction("SubWindow.Create", createSubWindow);
    JsBindingUtils.bindFunction("SubWindow.Show", showSubWindow);
    JsBindingUtils.bindFunction("SubWindow.Hide", hideSubWindow);
    JsBindingUtils.bindFunction("SubWindow.Close", closeSubWindow);
    JsBindingUtils.bindFunction("SubWindow.SetBounds", setSubWindowBounds);
  }
}
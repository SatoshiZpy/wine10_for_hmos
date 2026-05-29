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

function createWindow(param: NewWindowParam, callback: (id: number) => void) {
  implSubWindowAdapter().createWindow(param, callback)
}

function showWindow(id: number) {
  implSubWindowAdapter().showWindow(id);
}

function hideWindow(id: number) {
  implSubWindowAdapter().hideWindow(id);
}

function closeWindow(id: number) {
  implSubWindowAdapter().closeWindow(id);
}

function setBounds(id: number, bounds: WindowBound) {
  implSubWindowAdapter().setBounds(id, bounds);
}

export class SubWindowAdapterBind {
  static bind() {
    JsBindingUtils.bindFunction("SubWindow.CreateWindow", createWindow);
    JsBindingUtils.bindFunction("SubWindow.ShowWindow", showWindow);
    JsBindingUtils.bindFunction("SubWindow.HideWindow", hideWindow);
    JsBindingUtils.bindFunction("SubWindow.CloseWindow", closeWindow);
    JsBindingUtils.bindFunction("SubWindow.SetBounds", setBounds);
  }
}
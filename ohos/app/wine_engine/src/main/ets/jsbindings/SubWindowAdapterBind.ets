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

function destroyWindow(id: number) {
  implSubWindowAdapter().destroyWindow(id);
}

function setBounds(id: number, bounds: WindowBound) {
  implSubWindowAdapter().setBounds(id, bounds);
}

function setFullscreen(id: number) {
  implSubWindowAdapter().setFullscreen(id);
}

function maximize(id: number) {
  implSubWindowAdapter().maximize(id);
}

function unMaximize(id: number) {
  implSubWindowAdapter().unMaximize(id);
}

function minimize(id: number) {
  implSubWindowAdapter().minimize(id);
}

function restore(id: number) {
  implSubWindowAdapter().restore(id);
}

function activateWindow(id: number) {
  implSubWindowAdapter().activateWindow(id);
}

export class SubWindowAdapterBind {
  static bind() {
    JsBindingUtils.bindFunction("SubWindow.CreateWindow", createWindow);
    JsBindingUtils.bindFunction("SubWindow.ShowWindow", showWindow);
    JsBindingUtils.bindFunction("SubWindow.HideWindow", hideWindow);
    JsBindingUtils.bindFunction("SubWindow.CloseWindow", closeWindow);
    JsBindingUtils.bindFunction("SubWindow.DestroyWindow", destroyWindow);
    JsBindingUtils.bindFunction("SubWindow.SetBounds", setBounds);
    JsBindingUtils.bindFunction("SubWindow.SetFullscreen", setFullscreen);
    JsBindingUtils.bindFunction("SubWindow.Maximize", maximize);
    JsBindingUtils.bindFunction("SubWindow.UnMaximize", unMaximize);
    JsBindingUtils.bindFunction("SubWindow.Minimize", minimize);
    JsBindingUtils.bindFunction("SubWindow.Restore", restore);
    JsBindingUtils.bindFunction("SubWindow.ActivateWindow", activateWindow);
  }
}
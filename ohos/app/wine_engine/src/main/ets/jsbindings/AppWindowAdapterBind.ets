// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import JsBindingUtils from '../utils/JsBindingUtils';
import Inject from '../common/InjectModule';
import lazy { AppWindowAdapter } from '../adapter/AppWindowAdapter';
import type { NewWindowParam, PointCoordinate, WindowBound } from '../interface/CommonInterface';

function implAppWindowAdapter(): AppWindowAdapter {
  return Inject.getOrCreate(AppWindowAdapter);
}

function createWindow(param: NewWindowParam) {
  implAppWindowAdapter().createWindow(param);
}

function closeWindow(id: number) {
  implAppWindowAdapter().closeWindow(id);
}

function showWindow(id: number) {
  implAppWindowAdapter().showWindow(id);
}

function hideWindow(id: number) {
  implAppWindowAdapter().hideWindow(id);
}

function activateWindow(id: number) {
  implAppWindowAdapter().activateWindow(id);
}

function setBackgroundColor(id: number, argbColor: number) {
  implAppWindowAdapter().setBackgroundColor(id, argbColor);
}

function setMaximizable(id: number, maximizable: boolean) {
  implAppWindowAdapter().setMaximizable(id, maximizable);
}

function setMinimizable(id: number, minimizable: boolean) {
  implAppWindowAdapter().setMinimizable(id, minimizable);
}

function setClosable(id: number, closable: boolean) {
  implAppWindowAdapter().setClosable(id, closable);
}

function setAlwaysOnTop(id: number, onTop: boolean) {
  implAppWindowAdapter().setAlwaysOnTop(id, onTop);
}

function setFullscreen(id: number) {
  implAppWindowAdapter().setFullscreen(id);
}

function setBounds(id: number,
                   bounds: WindowBound,
                   callback: () => void) {
  implAppWindowAdapter().setBounds(id, bounds, callback);
}

function setEnabled(enable: boolean, id: number) {
  implAppWindowAdapter().setEnabled(enable, id);
}

function setWindowLimits(minWidth: number,
                         minHeight: number,
                         maxWidth: number,
                         maxHeight: number,
                         id: number) {
  implAppWindowAdapter().setWindowLimits(minWidth, minHeight, maxWidth, maxHeight, id);
}

function maximize(id: number) {
  implAppWindowAdapter().maximize(id);
}

function unMaximize(id: number) {
  implAppWindowAdapter().unMaximize(id);
}

function minimize(id: number) {
  implAppWindowAdapter().minimize(id);
}

function setTitle(title:string, id: number) {
  implAppWindowAdapter().setTitle(title, id);
}

function relaunch() : boolean {
  return implAppWindowAdapter().relaunch();
}

function setMovable(id: number, movable: boolean) {
  implAppWindowAdapter().setMovable(id, movable);
}

function setResizable(id: number, resizable: boolean) {
  implAppWindowAdapter().setResizable(id, resizable);
}

function startWindowMoving(id: number) {
  implAppWindowAdapter().startWindowMoving(id);
}

function getWindowsByCoordinate(coordinate: PointCoordinate,
                                callback: (windowIds: Array<number>) => void) {
  implAppWindowAdapter().getWindowsByCoordinate(coordinate, callback);
}

function restore(id: number) {
  implAppWindowAdapter().restore(id);
}

function shiftWindowEvent(sourceId: number, targetId: number,
                          callback: (result: boolean) => void) {
  implAppWindowAdapter().shiftWindowEvent(sourceId, targetId, callback);
}

function startWindowMovingWithOffset(id: number, offsetX: number, offsetY: number) {
  implAppWindowAdapter().startWindowMovingWithOffset(id, offsetX, offsetY);
}

function getOriginWindowIds(windowIds: Array<number>): Array<number> {
  return implAppWindowAdapter().getOriginWindowIds(windowIds);
}

function setWindowModal(modal: boolean, id: number) {
  implAppWindowAdapter().setWindowModal(modal, id);
}

function setFocusable(isFocusable: boolean, id: number) {
  implAppWindowAdapter().setFocusable(isFocusable, id);
}

function setParent(id: number, parent_id: number) {
  implAppWindowAdapter().setParent(id, parent_id);
}

export class AppWindowAdapterBind {
  static bind() {
    JsBindingUtils.bindFunction("AppWindow.CreateWindow", createWindow);
    JsBindingUtils.bindFunction("AppWindow.CloseWindow", closeWindow);
    JsBindingUtils.bindFunction("AppWindow.SetFullscreen", setFullscreen);
    JsBindingUtils.bindFunction("AppWindow.SetBounds", setBounds);
    JsBindingUtils.bindFunction("AppWindow.SetEnabled", setEnabled);
    JsBindingUtils.bindFunction("AppWindow.SetWindowLimits", setWindowLimits);
    JsBindingUtils.bindFunction("AppWindow.Maximize", maximize);
    JsBindingUtils.bindFunction("AppWindow.UnMaximize", unMaximize);
    JsBindingUtils.bindFunction("AppWindow.Minimize", minimize);
    JsBindingUtils.bindFunction("AppWindow.SetTitle", setTitle);
    JsBindingUtils.bindFunction("AppWindow.ShowWindow", showWindow);
    JsBindingUtils.bindFunction("AppWindow.HideWindow", hideWindow);
    JsBindingUtils.bindFunction("AppWindow.ActivateWindow", activateWindow);
    JsBindingUtils.bindFunction("AppWindow.SetBackgroundColor", setBackgroundColor);
    JsBindingUtils.bindFunction("AppWindow.SetMaximizable", setMaximizable);
    JsBindingUtils.bindFunction("AppWindow.SetMinimizable", setMinimizable);
    JsBindingUtils.bindFunction("AppWindow.SetClosable", setClosable);
    JsBindingUtils.bindFunction("AppWindow.SetAlwaysOnTop", setAlwaysOnTop);
    JsBindingUtils.bindFunction("AppWindow.Relaunch", relaunch);
    JsBindingUtils.bindFunction("AppWindow.SetMovable", setMovable);
    JsBindingUtils.bindFunction("AppWindow.SetResizable", setResizable);
    JsBindingUtils.bindFunction("AppWindow.StartWindowMoving", startWindowMoving);
    JsBindingUtils.bindFunction("AppWindow.GetWindowsByCoordinate", getWindowsByCoordinate);
    JsBindingUtils.bindFunction("AppWindow.Restore", restore);
    JsBindingUtils.bindFunction("AppWindow.ShiftWindowEvent", shiftWindowEvent);
    JsBindingUtils.bindFunction("AppWindow.StartWindowMovingWithOffset", startWindowMovingWithOffset);
    JsBindingUtils.bindFunction("AppWindow.GetOriginWindowIds", getOriginWindowIds);
    JsBindingUtils.bindFunction("AppWindow.SetWindowModal", setWindowModal);
    JsBindingUtils.bindFunction("AppWindow.SetFocusable", setFocusable);
    JsBindingUtils.bindFunction("AppWindow.setParent", setParent);
  }
}

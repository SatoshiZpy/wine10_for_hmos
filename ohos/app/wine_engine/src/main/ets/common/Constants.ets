// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export class ConfigData {
  static DOMAIN: number = 0x0000;
  static TAG: string = '[WineEngine]';
  static WINDOW_PREFIX: string = 'surface';
  static DEFAULT_WINDOW_ID: string = 'surface1';
  static DEFAULT_WIDGET_ID: number = 1;
}
let configData = new ConfigData();
export default configData as ConfigData;

export enum PanAction {
  kStart = 0,
  kUpdate,
  kEnd,
  kCancel,
}

export enum ContextType {
  kMainProcess = 0,
  kRenderProcess,
  kCrashpadHandler,
}

export enum WineStatus {
  kInit = 0,
  kStart,
  kDestroy,
}

export enum CommandType {
  kNewWindow,
  kNewWineContent,
  kNewWidget,
  kGetWidget,
  kGetLastActiveWidget,
  kOpenURL,
  kGetPid,
};
// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import window from '@ohos.window';
import { injectable } from 'inversify';

interface IFloatWindowParams {
  id: number,
  window: window.Window
}

@injectable()
export class SystemFloatingWindowManager {
  private storage: Map<number, (IFloatWindowParams)[]> =
    new Map<number, (IFloatWindowParams)[]>();

  public addSystemFloatingWindow(windowId: number, data: IFloatWindowParams) {
    let windows = this.storage.get(windowId);
    if (windows) {
      windows.push(data);
    } else {
      this.storage.set(windowId, [data]);
    }
  }

  public removeSystemFloatingWindow(windowId: number, floatWindowId: number) {
    let windows = this.storage.get(windowId);
    if (windows) {
      for (let i = 0; i < windows.length; i++) {
        if (windows[i].id === floatWindowId) {
          windows.splice(i, 1);
        }
      }
    }
  }

  public getMainWindowId(windowId: number): number {
    for (let key of this.storage.keys()) {
      let value = this.storage.get(key);
      if (value) {
        for (let i = 0; i < value.length; i++) {
          if (windowId === value[i].id) {
            return key;
          }
        }
      }
    }
    return -1;
  }

  public getFloatWindow(windowId: number): window.Window | undefined {
    for (let key of this.storage.keys()) {
      let value = this.storage.get(key);
      if (value) {
        for (let i = 0; i < value.length; i++) {
          if (windowId === value[i].id) {
            return value[i].window;
          }
        }
      }
    }
    return undefined;
  }
};

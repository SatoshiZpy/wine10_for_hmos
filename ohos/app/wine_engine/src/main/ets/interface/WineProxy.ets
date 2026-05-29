// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { common } from '@kit.AbilityKit';
import { window } from '@kit.ArkUI';
import { WindowStyles } from './CommonInterface';

export interface WineProxy {
  getWindowStage: () => window.WindowStage | undefined;
  getWindow: () => window.Window | undefined;
  getWindowContext: () => common.Context | undefined;
  getWindowId: () => number;
  closeWindow: () => void;
  setWindowTitle: (title:string) => void;
  createSubWindow: (name: string) => Promise<window.Window>;
  getOriginWindowId: () => number;
  updateAvoidAreaStatus: (isFullScreen: boolean) => void;
  isUIAbility: () => boolean;
  getWindowStyles: () => WindowStyles;
  setWindowStyles: (styles: WindowStyles) => void;
};

export const dwStyles: WindowStyles = {
  hide_title_bar: true,
  closable: false,
  minimizable: false,
  maximizable: false,
  use_floating_window: false,
  use_dark_mode: false,
  always_on_top: false,
  resizable: true,
  focusable: true,
  touchable: true,
  show_window: false,
  has_shadow: false,
};

export class FakeWineProxy implements WineProxy {
  getWindowStage(): window.WindowStage | undefined {
    return undefined;
  }

  getWindow(): window.Window | undefined {
    return undefined;
  }

  getWindowContext(): common.Context | undefined {
    return undefined;
  }

  getWindowId(): number {
    return -1;
  }

  closeWindow() {}

  setWindowTitle(title:string) {}

  createSubWindow(name: string): Promise<window.Window> {
    return new Promise<window.Window>((resolve, reject) => {
      reject('fake window.')
    });
  }

  getOriginWindowId(): number {
    return -1;
  }

  updateAvoidAreaStatus(isFullScreen: boolean) {}

  isUIAbility(): boolean {
    return true;
  }

  getWindowStyles(): WindowStyles {
    return dwStyles;
  }

  setWindowStyles(styles: WindowStyles) {}
}
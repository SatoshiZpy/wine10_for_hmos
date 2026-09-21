// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { WineWindow } from 'wine_engine';
import CustomChildProcess from '../process/CustomChildProcess';
import { KeyCode } from '@kit.InputKit';
import LogUtil from 'wine_engine/src/main/ets/utils/LogUtil';
import { uiObserver } from '@kit.ArkUI';

const TAG: string = 'Index';

CustomChildProcess.toString();
let storage = LocalStorage.getShared();
@Entry(storage)
@Component
struct Index {
  @LocalStorageProp('statusBarHeight') statusBarHeight: number = 0;
  @State density: number = 0;

  aboutToAppear(): void {
    uiObserver.on('densityUpdate', this.getUIContext(), (info: uiObserver.DensityInfo) => {
      this.density = info.density;
    });
  }

  aboutToDisappear(): void {
    uiObserver.off('densityUpdate', this.getUIContext());
  }

  build() {
    Row() {
      WineWindow()
    }.onKeyEvent((event?: KeyEvent) => {
      if (event) {
        if (event.type == KeyType.Down && event.keyCode == KeyCode.KEYCODE_ESCAPE) {
          event.stopPropagation();
        }
      }
    })
    .width('100%')
    .height('100%')
    .padding({
      // System status bar avoid area; the value from avoidArea is already in
      // px, pass it through as a px string without a vp round trip to avoid
      // sub-pixel drift.
      top: `${this.statusBarHeight}px`,
    })
  }
}

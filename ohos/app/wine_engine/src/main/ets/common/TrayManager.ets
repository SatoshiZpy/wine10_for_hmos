// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { injectable } from 'inversify';
import { JSON } from '@kit.ArkTS';
import { statusBarManager } from '@kit.StatusBarExtensionKit';
import { image } from '@kit.ImageKit';
import { emitter } from '@kit.BasicServicesKit';
import LogUtil from '../utils/LogUtil';
import { BusinessError } from '@kit.BasicServicesKit';

const TAG: string = 'TrayManager';

export interface StatusBarCallback {
  iconClick: (data: emitter.EventData) => void,
  rightMenuClick: (data: emitter.EventData) => void,
};

export interface StatusBarConfig {
  statusBarCallback: StatusBarCallback,
  title: string,
  height: number,
  abilityName: string,
  moduleName: string,
  whiteIcon: image.PixelMap,
  blackIcon: image.PixelMap
};

@injectable()
export class TrayManager {
  private callback: StatusBarCallback = {
    iconClick: (data: emitter.EventData) => {},
    rightMenuClick: (data: emitter.EventData) => {},
  }

  private quickOperationHeight: number = 100;
  private isCreate: boolean;

  constructor() {
    this.isCreate = false;
  }

  private addTray(context: Context, config: StatusBarConfig) {
    try {
      statusBarManager.addToStatusBar(context, {
        icons: {
          white: config.whiteIcon,
          black: config.blackIcon
        },
        quickOperation: {
          title: config.title,
          height: config.height,
          abilityName: config.abilityName,
          moduleName: config.moduleName
        },
        statusBarGroupMenu: [],
      })

      this.callback = config.statusBarCallback;
      statusBarManager.on('statusBarIconClick', (eventData) => {
        LogUtil.info(TAG, 'statusBar icon Click eventData: ' + JSON.stringify(eventData));
        this.callback.iconClick(eventData);
      });

      statusBarManager.on('rightMenuClick', (eventData) => {
        LogUtil.info(TAG, 'right Menu Click eventData: ' + JSON.stringify(eventData));
        this.callback.rightMenuClick(eventData);
      });

    } catch (err){
      LogUtil.error(TAG, `addToStatusBar failed. err: ${JSON.stringify(err)}`);
    }
  }

  updateTray(context: Context, icon: statusBarManager.StatusBarIcon) {
    try {
      statusBarManager.updateStatusBarIcon(context, icon);
    } catch (err){
      LogUtil.error(TAG, `updateStatusBarIcon failed. err: ${JSON.stringify(err)}`);
    }
  }

  updateTrayMenu(context: Context, menus: Array<statusBarManager.StatusBarGroupMenu>) {
    try {
      statusBarManager.updateStatusBarMenu(context, menus);
    } catch (err){
      LogUtil.error(TAG, `updateStatusBarIcon failed. err: ${JSON.stringify(err)}`);
    }
  }

  private removeTray(context: Context, callback: (ret: boolean) => void) {
    try {
      statusBarManager.off('statusBarIconClick');
      statusBarManager.off('rightMenuClick');
      statusBarManager.removeFromStatusBar(context);
      callback(true);
    } catch (err) {
      LogUtil.error(TAG, 'removeFromStatusBar failed. err: ' + JSON.stringify(err));
      callback(false);
    }
  }

  public async addToStatusBar(context: Context, callback: StatusBarCallback) {
    if (this.isCreate) {
      return;
    }

    const resourceMgr = context.resourceManager;
    const whiteFileData = resourceMgr.getRawFileContentSync('white.png');
    const whiteBuffer = whiteFileData.buffer;
    const whiteImageSource = image.createImageSource(whiteBuffer);
    let whitePixelMap = await whiteImageSource.createPixelMap()
      .catch((error: BusinessError) => {
        LogUtil.error(TAG, 'Failed to create pixel map. Cause: ' + JSON.stringify(error));
      });

    const blackFileData = resourceMgr.getRawFileContentSync('black.png');
    const blackBuffer = blackFileData.buffer;
    const blackImageSource = image.createImageSource(blackBuffer);
    let blackPixelMap = await blackImageSource.createPixelMap()
      .catch((error: BusinessError) => {
        LogUtil.error(TAG, 'Failed to create pixel map. Cause: ' + JSON.stringify(error));
      });

    if (!whitePixelMap || !blackPixelMap) {
      return;
    }

    let config: StatusBarConfig = {
      statusBarCallback: callback,
      title: '',
      height: this.quickOperationHeight,
      abilityName: '',
      moduleName: 'entry',
      whiteIcon: whitePixelMap,
      blackIcon: blackPixelMap
    }

    try {
      this.addTray(context, config);
      this.isCreate = true;
      LogUtil.info(TAG, 'addToStatusBar ok');
    } catch (error) {
      LogUtil.error(TAG, 'addToStatusBar failed:' + JSON.stringify(error));
    }
  }

  public removeFromStatusBar(context: Context) {
    try {
      this.removeTray(context, ()=>{});
    } catch (error) {
      LogUtil.error(TAG, 'removeFromStatusBar failed:' + JSON.stringify(error));
    }
  }
}
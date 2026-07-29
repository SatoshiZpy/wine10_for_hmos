// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import LogUtil from '../utils/LogUtil';
import uiExtension from '@ohos.arkui.uiExtension';
import Want from '@ohos.app.ability.Want';
import AbilityConstant from '@ohos.app.ability.AbilityConstant';
import { NativeThemeAdapter } from '../adapter/NativeThemeAdapter';
import { Configuration } from '@ohos.app.ability.Configuration';
import Inject from '../common/InjectModule';
import { WindowBound } from '../interface/CommonInterface';
import { PermissionManagerAdapter } from '../adapter/PermissionManagerAdapter';
import { DisplayAdapter } from  '../adapter/DisplayAdapter';
import { UIExtensionContentSession } from '@kit.AbilityKit';
import { JSON } from '@kit.ArkTS';
import { AbilityManager } from '../common/AbilityManager';
import { WineBaseExtensionAbility } from './WineBaseExtensionAbility';
import { WineLoaderAdapter } from '../adapter/WineLoaderAdapter';

const TAG: string = 'WineBaseEmbeddedAbility';

export class WineBaseEmbeddedAbility extends WineBaseExtensionAbility {
  private static WINDOW_CLOSE: number = 1000;
  private abilityManager: AbilityManager | undefined = undefined;
  private displayAdapter : DisplayAdapter | undefined = undefined;
  private nativeThemeAdapter: NativeThemeAdapter | undefined = undefined;
  private wineLoader: WineLoaderAdapter | undefined = undefined;

  onConfigurationUpdate(config: Configuration) {
    this.nativeThemeAdapter?.setSystemNativeTheme(config.colorMode);
    this.displayAdapter?.onSystemFontSizeChange(config.fontSizeScale);
  }

  onCreate(launchParam: AbilityConstant.LaunchParam) {
    this.abilityManager = Inject.get(AbilityManager);
    this.nativeThemeAdapter = Inject.get(NativeThemeAdapter);
    this.displayAdapter = Inject.get(DisplayAdapter);
    Inject.get(PermissionManagerAdapter).initPermissions();
    this.wineLoader = Inject.get(WineLoaderAdapter);
  }

  onSessionCreate(want: Want, session: UIExtensionContentSession) {
    super.onSessionCreate(want, session);

    this.abilityManager?.addProxy(this);

    let windowProxy = session.getUIExtensionWindowProxy();
    try {
      windowProxy.on('windowSizeChange', (size) => {
        const prop = windowProxy.properties;
        let windowBound: WindowBound = {
          left: prop.uiExtensionHostWindowProxyRect.left,
          top: prop.uiExtensionHostWindowProxyRect.top,
          width: size.width,
          height: size.height,
        };
        this.nativeContext.OnWindowSizeChange(this.widgetId, windowBound);
      });
    } catch (exception) {
      LogUtil.error(TAG, 'Failed to listener for window size changes: ' + JSON.stringify(exception));
    }

    try {
      windowProxy.on("rectChange", 0, (option) => {
        if (option.reason === uiExtension.RectChangeReason.HOST_WINDOW_RECT_CHANGE) {
          this.nativeContext.OnWindowSizeChange(this.widgetId, option.rect);
        }
      });
    } catch (exception) {
      LogUtil.error(TAG,'Failed to listener for window rect changes: ' + JSON.stringify(exception));
    }

    let param: Record<string, Object> = {
      'widgetId': this.widgetId,
      'startUri': this.startUri,
      'session': session,
      'userData': this.userData,
      'ownerPid': this.ownerPid,
    };
    let storage: LocalStorage = new LocalStorage(param);
    session.loadContent('pages/EmbeddedWindow', storage);

    this.wineLoader?.loadWine(this.exePath, this.sockPath, this.startUri)
      .catch((err: Object) => {
        LogUtil.error(TAG, `loadWine failed: ${JSON.stringify(err)}`);
      });
  }

  onSessionDestroy(session: UIExtensionContentSession) {
    LogUtil.info(TAG, `onSessionDestroy, id: ${this.widgetId}`);
    this.nativeContext.OnWindowEvent(this.widgetId, WineBaseEmbeddedAbility.WINDOW_CLOSE);
  }

  onForeground() {
    LogUtil.info(TAG, `onForeground, id: ${this.widgetId}`);
  }

  onBackground() {
    LogUtil.info(TAG, `onBackground, id: ${this.widgetId}`);
  }

  onDestroy() {
    LogUtil.info(TAG, `onDestroy, id: ${this.widgetId}`);
  }

  closeWindow() {
    LogUtil.info(TAG, `closeWindow, id: ${this.widgetId}`);

    // Main window is destroyed, release UI related resources
    this.abilityManager?.removeProxy(this.widgetId);
    // this.UnRegisterEvent();
    super.closeWindow();
  }
};

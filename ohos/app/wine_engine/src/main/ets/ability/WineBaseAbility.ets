// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import UIAbility from '@ohos.app.ability.UIAbility';
import LogUtil from '../utils/LogUtil';
import window from '@ohos.window';
import Want from '@ohos.app.ability.Want';
import AbilityConstant from '@ohos.app.ability.AbilityConstant';
import JsBindingUtils from '../utils/JsBindingUtils';
import ArgvUtil from '../utils/ArgvUtil';
import Inject from '../common/InjectModule';
import lazy { NativeContext, WindowStyles } from '../interface/CommonInterface';
import { CommandType, ContextType } from '../common/Constants';
import { WineProxy, dwStyles } from '../interface/WineProxy';
import { common, ConfigurationConstant } from '@kit.AbilityKit';
import { deviceInfo, BusinessError } from '@kit.BasicServicesKit';
import { WindowStyle } from '../common/WindowStyle';
import { fileUri } from '@kit.CoreFileKit';

const TAG: string = 'WineBaseAbility';

export class WineBaseAbility extends UIAbility implements WineProxy {
  protected widgetId: number = -1;
  protected startUri: string = '';
  protected windowStage: window.WindowStage | undefined = undefined;
  protected nativeContext: NativeContext =
    JsBindingUtils.getNativeContext(ContextType.kMainProcess);
  protected originWindowId: number = -1;
  protected styles: WindowStyles = dwStyles;
  protected processKey: string = '';
  protected ownerPid: number = -1;
  protected exePath: string = '';
  protected args: string = '';
  protected sockPath: string = '';

  private initParameters(want: Want) {
    if (want && want.parameters) {
      if (want.parameters['widgetId'] !== undefined) {
        this.widgetId = want.parameters['widgetId'] as number;
      }
      if (want.parameters['features'] !== undefined &&
        want.parameters['window_state'] !== undefined) {
        let features = want.parameters['features'] as number;
        let window_state = want.parameters['window_state'] as number;
        this.styles = Inject.get(WindowStyle).buildWindowStyles(features, window_state);
      }
      if (want.parameters['processKey'] !== undefined) {
        this.processKey = want.parameters['processKey'] as string;
      }
      if (want.parameters['ownerPid'] !== undefined) {
        this.ownerPid = want.parameters['ownerPid'] as number;
      }
      if (want.parameters['wineExePath'] !== undefined) {
        this.exePath = want.parameters['wineExePath'] as string;
      }
      if (want.parameters['wineArgs'] !== undefined) {
        this.args = want.parameters['wineArgs'] as string;
      }
      if (want.parameters['sockPath'] !== undefined) {
        this.sockPath = want.parameters['sockPath'] as string;
      }
    }

    if (want["uri"]) {
      let storePath = want['uri'] as string;
      const fileUriObj = new fileUri.FileUri(storePath);
      this.startUri = fileUriObj.path;
    }
  }

  protected buildWineArgv(): string[] | undefined {
    return ArgvUtil.parseArgs(this.args);
  }

  protected getContentPath(): string {
    return 'pages/Index';
  }

  protected computeWindowStyles(windowClass: window.Window, styles: WindowStyles) {
    try {
      windowClass.setWindowDecorVisible(false);
      windowClass.setWindowTitleButtonVisible(false, false, false);
      if (styles.use_dark_mode && deviceInfo.deviceType === '2in1') {
        let buttonStyle: window.DecorButtonStyle = {
          colorMode: ConfigurationConstant.ColorMode.COLOR_MODE_DARK
        };
        windowClass.setDecorButtonStyle(buttonStyle);
      }
      windowClass.setWindowTitleMoveEnabled(!styles.hide_title_bar);
      windowClass.setWindowTopmost(styles.always_on_top)
        .catch((err: BusinessError) => {
          LogUtil.error(TAG, 'Failed to setTopmost. Cause:' + JSON.stringify(err));
        });
      windowClass.setResizeByDragEnabled(styles.resizable)
        .catch((err: BusinessError) => {
          LogUtil.error(TAG, 'Failed to setResizeable. Cause:' + JSON.stringify(err));
        });
      windowClass.setWindowFocusable(styles.focusable)
        .catch((err: BusinessError) => {
          LogUtil.error(TAG, 'Failed to setFocusable. Cause:' + JSON.stringify(err));
        });
      windowClass.setWindowTouchable(styles.touchable)
        .catch((err: BusinessError) => {
          LogUtil.error(TAG, 'Failed to setTouchable. Cause:' + JSON.stringify(err));
        });
      windowClass.setWindowShadowEnabled(styles.has_shadow)
        .catch((err: BusinessError) => {
          LogUtil.error(TAG, 'Failed to setShadowEnabled. Cause:' + JSON.stringify(err));
        });
    } catch (err) {
      LogUtil.error(TAG, 'Failed to compute window styles. Cause:' + JSON.stringify(err));
    }
  }

  onCreate(want: Want, launchParam: AbilityConstant.LaunchParam) {
    this.initParameters(want);

    if (this.widgetId <= 0) {
      this.widgetId = this.nativeContext.ExecuteCommand(CommandType.kGetWidget, {}).widget_Id;
      this.ownerPid = this.nativeContext.ExecuteCommand(CommandType.kGetPid, {}).pid;
    }
    LogUtil.info(TAG, `onCreate widgetId:${this.widgetId}, ownerPid:${this.ownerPid},
      styles:${JSON.stringify(this.styles)}, startUri:${this.startUri}`);
  }

  onWindowStageCreate(windowStage: window.WindowStage) {
    try {
      this.windowStage = windowStage;
      this.originWindowId = windowStage.getMainWindowSync().getWindowProperties().id;
    } catch (err) {
      LogUtil.error(TAG, 'Failed to getMainWindowSync. Cause:' + JSON.stringify(err));
    }
  }

  // implements WineProxy interface
  getWindowStage(): window.WindowStage | undefined {
    return this.windowStage;
  }

  getWindow(): window.Window | undefined {
    try {
      return this.windowStage?.getMainWindowSync();
    } catch (err) {
      LogUtil.error(TAG, 'Failed to getMainWindowSync. Cause:' + JSON.stringify(err));
      return undefined;
    }
  }

  getWindowContext(): common.Context {
    return this.context;
  }

  getWindowId(): number {
    return this.widgetId;
  }

  closeWindow() {
    this.context.terminateSelf()
      .then(() => {
        LogUtil.info(TAG, 'terminate Self id: ' + this.getWindowId());
      })
      .catch((err: BusinessError) => {
        LogUtil.error(TAG, 'Failed to terminate Self: ' + JSON.stringify(err));
      });
  }

  setWindowTitle(title:string) {
    this.context.setMissionLabel(title)
      .catch((err: BusinessError) => {
        LogUtil.error(TAG, 'Failed to set title: ' + JSON.stringify(err));
      });
  }

  createSubWindow(name: string): Promise<window.Window> {
    return new Promise<window.Window>((resolve, reject) => {
      if (!this.windowStage) {
        reject('Failed to get windowStage');
        return;
      }
      const subWindowOpts: window.SubWindowOptions = {
        title: 'This is a subwindow',
        decorEnabled: true,
        maximizeSupported: true,
        zLevelAboveParentLoosened: true
      };
      this.windowStage.createSubWindowWithOptions(name, subWindowOpts)
        .then((data: window.Window) => {
          resolve(data);
        }).catch((error: BusinessError) => {
        reject(error);
      })
    });
  }

  getOriginWindowId(): number {
    return this.originWindowId;
  }

  public updateAvoidAreaStatus(isFullScreen: boolean) {
    let windowClass = this.getWindow();

    // Set window to full screen.
    windowClass?.setWindowLayoutFullScreen(true).then(() => {
      LogUtil.info(TAG, 'Succeeded in setting the window layout to full-screen mode.');
    }).catch((err: BusinessError) => {
      LogUtil.error(TAG, 'Failed to set the window layout to full-screen mode. Cause:' + JSON.stringify(err));
    });

    // Hide the navigation bar here.
    windowClass?.setSpecificSystemBarEnabled('navigationIndicator', false).then(() => {
      LogUtil.info(TAG, 'Succeeded in setting the navigation indicator to be invisible.');
    }).catch((err: BusinessError) => {
      LogUtil.error(TAG, `Failed to set the navigation indicator to be invisible. Code is ${err.code}, message is ${err.message}`);
    });

    // The top status bar is set up to be hidden when in full-screen mode and displayed when maximized.
    windowClass?.setSpecificSystemBarEnabled('status', !isFullScreen).then(() => {
      LogUtil.info(TAG, 'Succeeded in setting the status bar to be invisible.');
    }).catch((err: BusinessError) => {
      LogUtil.error(TAG, `Failed to set the status bar to be invisible. Code is ${err.code}, message is ${err.message}`);
    });
  }

  isUIAbility(): boolean {
    return true;
  }

  getWindowStyles(): WindowStyles {
    return this.styles;
  }

  setWindowStyles(styles: WindowStyles) {
    this.styles = styles;

    if (!this.styles.hide_title_bar)
      this.getWindow()?.setWindowTitleButtonVisible(
        styles.maximizable, styles.minimizable, styles.closable);
  }
};

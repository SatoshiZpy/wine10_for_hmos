// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import LogUtil from '../utils/LogUtil';
import window from '@ohos.window';
import Want from '@ohos.app.ability.Want';
import JsBindingUtils from '../utils/JsBindingUtils';
import ArgvUtil from '../utils/ArgvUtil';
import { NativeContext, WindowStyles } from '../interface/CommonInterface';
import { CommandType, ContextType } from '../common/Constants';
import { WineProxy, dwStyles } from '../interface/WineProxy';
import { common, EmbeddedUIExtensionAbility, UIExtensionContentSession } from '@kit.AbilityKit';
import { BusinessError } from '@kit.BasicServicesKit';
import { fileUri } from '@kit.CoreFileKit';

const TAG: string = 'WineBaseExtensionAbility';

export class WineBaseExtensionAbility extends EmbeddedUIExtensionAbility implements WineProxy {
  protected widgetId: number = -1;
  protected startUri: string = '';
  protected hostModuleId: number = 0; // Host module id
  protected hostModuleType: number = 0; // Host module type, Ability or SubWindow
  protected userData: string = ''; // Host module specified
  protected session: UIExtensionContentSession | undefined = undefined;
  protected nativeContext: NativeContext =
    JsBindingUtils.getNativeContext(ContextType.kMainProcess);
  protected hideTitleBar: boolean = true;
  protected ownerPid: number = -1;
  protected exePath: string = '';
  protected args: string = '';
  protected sockPath: string = '';

  private initParameters(want: Want) {
    if (want.parameters) {
      if (want.parameters['widgetId'] !== undefined) {
        this.widgetId = want.parameters['widgetId'] as number;
      }
      if (want.parameters['hostModuleId'] !== undefined) {
        this.hostModuleId = want.parameters['hostModuleId'] as number;
      }
      if (want.parameters['hostModuleType'] !== undefined) {
        this.hostModuleType = want.parameters['hostModuleType'] as number;
      }
      if (want.parameters['userData'] !== undefined) {
        this.userData = want.parameters['userData'] as string;
      }
      if (want.parameters["ownerPid"] !== undefined) {
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

    if (want.uri) {
      let storePath = want.uri as string;
      const fileUriObj = new fileUri.FileUri(storePath);
      this.startUri = fileUriObj.path;
    }
  }

  protected buildWineArgv(): string[] | undefined {
    return ArgvUtil.parseArgs(this.args);
  }

  onSessionCreate(want: Want, session: UIExtensionContentSession) {
    this.initParameters(want);

    if (this.widgetId <= 0) {
      this.widgetId = this.nativeContext.ExecuteCommand(CommandType.kGetWidget, {}).widget_Id;
      this.ownerPid = this.nativeContext.ExecuteCommand(CommandType.kGetPid, {}).pid;
    }
    LogUtil.info(TAG, `widgetId:${this.widgetId}, ownerPid:${this.ownerPid}, startUri:${this.startUri}`);

    this.session = session;
  }

  // implements WineProxy interface
  getWindowStage(): window.WindowStage | undefined {
    return undefined;
  }

  getWindow(): window.Window | undefined {
    return undefined;
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

  setWindowTitle(title:string) {}

  createSubWindow(name: string): Promise<window.Window> {
    return new Promise<window.Window>((resolve, reject) => {
      if (!this.session) {
        reject('Failed to get Session');
        return;
      }
      const windowProxy = this.session.getUIExtensionWindowProxy();
      const subWindowOpts: window.SubWindowOptions = {
        title: 'This is a subwindow',
        decorEnabled: true,
        maximizeSupported: true,
        zLevelAboveParentLoosened: true
      };
      windowProxy?.createSubWindowWithOptions(name, subWindowOpts)
        .then((data: window.Window) => {
          resolve(data);
        }).catch((error: BusinessError) => {
        reject(error);
      })
    });
  }

  getOriginWindowId(): number {
    return -1;
  }

  updateAvoidAreaStatus(isFullScreen: boolean) {}

  isUIAbility(): boolean {
    return false;
  }

  getWindowStyles(): WindowStyles {
    return dwStyles;
  }

  setWindowStyles(styles: WindowStyles) {}
};

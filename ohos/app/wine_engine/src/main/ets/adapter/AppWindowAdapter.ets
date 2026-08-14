// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import bundleManager from '@ohos.bundle.bundleManager';
import { inject, injectable } from 'inversify';
import { BusinessError } from '@ohos.base';
import { window } from '@kit.ArkUI';
import Want from '@ohos.app.ability.Want';
import { BaseAdapter } from '../common/BaseAdapter';
import LogUtil from '../utils/LogUtil';
import { AbilityManager } from '../common/AbilityManager';
import LogMethod from '../common/LogDecorator';
import { contextConstant, StartOptions } from '@kit.AbilityKit';
import { NewWindowParam,
  PointCoordinate,
  WindowBound,
  WindowState,
  WindowFeatures,
  AbilityType,
  IWindowInfo} from '../interface/CommonInterface';
import LaunchHelper from '../utils/LaunchHelper';
import { ContextAdapter } from './ContextAdapter';
import { DeviceInfoAdapter } from '../adapter/DeviceInfoAdapter';
import { NativeThemeAdapter } from './NativeThemeAdapter';
import common from '@ohos.app.ability.common';
import { WindowStyle } from '../common/WindowStyle';

const TAG: string = 'AppWindow';

const kAbilityMap = new Map<AbilityType, string>([
  [AbilityType.kEntryAbility, 'EntryAbility'],
  [AbilityType.kEmbeddedAbility, 'EmbeddedAbility']
])

@injectable()
export class AppWindowAdapter extends BaseAdapter {
  private abilityManager: AbilityManager;
  private bundleName: string = 'com.huawei.ohos_wine';
  private bundleFlags = bundleManager.BundleFlag.GET_BUNDLE_INFO_WITH_APPLICATION |
  bundleManager.BundleFlag.GET_BUNDLE_INFO_WITH_METADATA;
  private queryWindowNum: number = 10;
  private ctxAdapter: ContextAdapter;
  private deviceInfoAdapter: DeviceInfoAdapter;
  private themeAdapter: NativeThemeAdapter;
  private windowStyle: WindowStyle;
  private windowList: (IWindowInfo)[] = [];

  constructor(
    @inject(AbilityManager) abilityManager: AbilityManager,
    @inject(ContextAdapter) ctxAdapter: ContextAdapter,
    @inject(DeviceInfoAdapter) deviceInfoAdapter: DeviceInfoAdapter,
    @inject(NativeThemeAdapter) themeAdapter: NativeThemeAdapter,
    @inject(WindowStyle) windowStyle: WindowStyle
  ) {
    super();
    this.abilityManager = abilityManager;
    this.ctxAdapter = ctxAdapter;
    this.deviceInfoAdapter = deviceInfoAdapter;
    this.themeAdapter = themeAdapter;
    this.windowStyle = windowStyle;

    try {
      this.bundleName = bundleManager.getBundleInfoForSelfSync(this.bundleFlags).name;
    } catch (err) {
      let message = (err as BusinessError).message;
      LogUtil.error(TAG, `getBundleInfoForSelf failed.: ${message}`);
    }
  }

  private getWindow(id: number): window.Window | undefined {
    return this.abilityManager.getProxy(id)?.getWindow();
  }

  private backgroundColor(useDarkMode: boolean): string {
    if (useDarkMode || this.themeAdapter.isDarkMode()) {
      return NativeThemeAdapter.DARK_BACKGROUND_COLOR;
    }
    return NativeThemeAdapter.LIGHT_BACKGROUND_COLOR;
  }

  @LogMethod
  createWindow(param: NewWindowParam, callback: (id: number) => void) {
    let abilityName: string = kAbilityMap.get(param.ability_type) ?? 'EntryAbility';
    const want: Want = {
      bundleName: this.bundleName,
      abilityName: abilityName,
      parameters: {
        'widgetId': param.widget_id,
        'features': param.feature_mask,
        'window_state': param.window_state_mask,
        'instanceKey': `${param.process_key}:${param.widget_id}`,
        'ownerPid': param.owner_pid,
      }
    };
    let windowClass = this.ctxAdapter.getActiveProxy().getWindow();
    const windowProp = windowClass?.getWindowProperties();
    let xComBorder: number = windowProp?.drawableRect.left || 0;
    let xComTop: number = windowProp?.drawableRect.top || 0;
    let visibility = this.windowStyle.hasValue(WindowState.WINDOW_VISIBLE, param.window_state_mask);
    let useDarkMode = this.windowStyle.hasValue(WindowFeatures.USE_DARK_MODE, param.window_state_mask);
    const activeCtx = this.ctxAdapter.getActiveContext() as common.UIAbilityContext;
    if (!activeCtx) {
      LogUtil.error(TAG, 'createWindow failed. Cause: UIAbilityContext required');
      callback(-1);
      return;
    }
    const options: StartOptions = {
      processMode: contextConstant.ProcessMode.ATTACH_TO_STATUS_BAR_ITEM,
      startupVisibility: visibility ? contextConstant.StartupVisibility.STARTUP_SHOW :
        contextConstant.StartupVisibility.STARTUP_HIDE,
      windowLeft: param.bounds.left - xComBorder,
      windowTop: param.bounds.top - xComTop,
      windowWidth: param.bounds.width + xComBorder + xComBorder,
      windowHeight: param.bounds.height + xComBorder + xComTop,
      startWindowBackgroundColor: this.backgroundColor(useDarkMode),
    };
    LogUtil.info(TAG, 'createWindow want:' + JSON.stringify(want) + 'options:' + JSON.stringify(options));
    let subStorage: LocalStorage = new LocalStorage(want);
    LaunchHelper.LaunchWithOptions(activeCtx, want, options)
      .then((result) => {
        if (result) {
          this.windowList.push({
            id: param.widget_id,
            localStorage: subStorage
          });
          LogUtil.info(TAG, 'createWindow id:' + param.widget_id +
            ',result:' + result + ',length:' + this.windowList.length);
          callback(param.widget_id);
        } else {
          LogUtil.error(TAG, 'createWindow failed. Cause: LaunchWithOptions returned false');
          callback(-1);
        }
      }).catch((err: BusinessError) => {
        LogUtil.error(TAG, 'Failed to create window. Cause:' + JSON.stringify(err));
        callback(-1);
      });
  }

  @LogMethod
  closeWindow(id: number) {
    let proxy = this.abilityManager.getProxy(id);
    proxy?.closeWindow();
    this.removeWindow(id);
    LogUtil.info(TAG, 'closeWindow id:' + id + ',length:' + this.windowList.length);
  }

  @LogMethod
  showWindow(id: number) {
    let context = this.abilityManager.getProxy(id)?.getWindowContext() as common.UIAbilityContext;
    context?.showAbility().then(() => {
      LogUtil.info(TAG, 'showWindow success, id:' + id);
    }).catch((err: BusinessError) => {
      LogUtil.error(TAG, 'Failed to show window. Cause:' + JSON.stringify(err));
    });
  }

  @LogMethod
  hideWindow(id: number) {
    let context = this.abilityManager.getProxy(id)?.getWindowContext() as common.UIAbilityContext;
    context?.hideAbility().then(() => {
      LogUtil.info(TAG, 'hideWindow success, id:' + id);
    }).catch((err: BusinessError) => {
      LogUtil.error(TAG, 'Failed to hide window. Cause:' + JSON.stringify(err));
    });
  }

  @LogMethod
  activateWindow(id: number) {
    this.showWindow(id);
  }

  @LogMethod
  setBackgroundColor(id: number, argbColor: number) {
    try {
      let windowClass = this.getWindow(id);
      windowClass?.setWindowBackgroundColor(`#${this.windowStyle.decimalColorToHex(argbColor)}`);
    } catch (exception) {
      LogUtil.error(TAG, 'Failed to setBackgroundColor. Cause:' + JSON.stringify(exception));
    }
  }

  @LogMethod
  setMaximizable(id: number, maximizable: boolean) {
    let proxy = this.abilityManager.getProxy(id);
    let styles = proxy?.getWindowStyles();
    if (styles) {
      styles.maximizable = maximizable;
      proxy?.setWindowStyles(styles);
    }
  }

  @LogMethod
  setMinimizable(id: number, minimizable: boolean) {
    let proxy = this.abilityManager.getProxy(id);
    let styles = proxy?.getWindowStyles();
    if (styles) {
      styles.minimizable = minimizable;
      proxy?.setWindowStyles(styles);
    }
  }

  @LogMethod
  setClosable(id: number, closable: boolean) {
    let proxy = this.abilityManager.getProxy(id);
    let styles = proxy?.getWindowStyles();
    if (styles) {
      styles.closable = closable;
      proxy?.setWindowStyles(styles);
    }
  }

  @LogMethod
  setAlwaysOnTop(id: number, onTop: boolean) {
    let windowClass = this.getWindow(id);
    windowClass?.setWindowTopmost(onTop)
      .then(() => {
        LogUtil.info(TAG, `Succeeded in setting the window to be topmost.`);
      }).catch((err: BusinessError) => {
      LogUtil.error(TAG, 'Failed to set the window to be topmost. Cause:' + JSON.stringify(err));
    });
  }

  @LogMethod
  setFullscreen(id: number) {
    let windowClass = this.getWindow(id);
    if (!windowClass) {
      LogUtil.error(TAG, 'Failed to obtain the main window.');
      return;
    }

    if (this.deviceInfoAdapter.isNormalWindowMode()) {
      this.abilityManager.getProxy(id)?.updateAvoidAreaStatus(true);
    }

    windowClass.maximize(window.MaximizePresentation.ENTER_IMMERSIVE_DISABLE_TITLE_AND_DOCK_HOVER)
      .catch((err: BusinessError) => {
        LogUtil.error(TAG, 'Failed to maximize the window. Cause:' + JSON.stringify(err));
      });
  }

  @LogMethod
  setBounds(id: number,
    bounds: WindowBound,
    callback: () => void) {
    let windowClass = this.getWindow(id);
    if (!windowClass) {
      LogUtil.error(TAG, 'Failed to obtain the main window.');
      return;
    }

    try {
      const windowProp = windowClass.getWindowProperties();
      let xComBorder: number = windowProp?.drawableRect.left || 0;
      let xComTop: number = windowProp?.drawableRect.top || 0;
      bounds.left -= xComBorder;
      bounds.top -= xComTop;
      bounds.width = bounds.width + xComBorder + xComBorder;
      bounds.height = bounds.height + xComBorder + xComTop;

      windowClass.moveWindowToAsync(bounds.left, bounds.top)
        .then(() => {
          windowClass?.resizeAsync(bounds.width, bounds.height)
            .catch((err: BusinessError) => {
              LogUtil.error(TAG, 'Failed to change the window size. Cause:' + JSON.stringify(err));
              callback && callback();
            });
        }).catch((err: BusinessError) => {
        LogUtil.error(TAG, 'Failed to move the window. Cause:' + JSON.stringify(err));
      });
    } catch (exception) {
      LogUtil.error(TAG, 'move the window exception. Cause:' + JSON.stringify(exception));
    }
  }

  @LogMethod
  setEnabled(enable: boolean, id: number) {
    let windowClass = this.getWindow(id);
    if (!windowClass) {
      LogUtil.error(TAG, 'Failed to obtain the main window.');
      return;
    }
    windowClass.setWindowTouchable(enable)
      .catch((err: BusinessError) => {
        LogUtil.error(TAG, 'Failed to set the window to be touchable. Cause:' + JSON.stringify(err));
      });
  }

  @LogMethod
  setWindowLimits(minWidth: number,
    minHeight: number,
    maxWidth: number,
    maxHeight: number,
    id: number) {
    let windowClass = this.getWindow(id);
    if (!windowClass) {
      LogUtil.error(TAG, 'Failed to obtain the main window.');
      return;
    }

    try {
      const windowProp = windowClass.getWindowProperties();
      let xComBorder: number = windowProp?.drawableRect.left || 0;
      let xComTop: number = windowProp?.drawableRect.top || 0;
      let windowLimits: window.WindowLimits = {
        minWidth: minWidth === 0 ? minWidth : minWidth + xComBorder + xComBorder,
        minHeight: minHeight === 0 ? minHeight : minHeight + xComTop + xComBorder,
        maxWidth: maxWidth === 0 ? maxWidth : maxWidth + xComBorder + xComBorder,
        maxHeight: maxHeight === 0 ? maxHeight : maxHeight + xComTop + xComBorder,
      };
      windowClass.setWindowLimits(windowLimits)
        .catch((err: BusinessError) => {
          LogUtil.error(TAG, 'Failed to change the window limits. Cause: ' + JSON.stringify(err));
        });
    } catch (exception) {
      LogUtil.error(TAG, 'change the window limits exception. Cause:' + JSON.stringify(exception));
    }
  }

  @LogMethod
  maximize(id: number) {
    let windowClass = this.getWindow(id);
    if (!windowClass) {
      LogUtil.error(TAG, 'Failed to obtain the main window.');
      return;
    }

    try {
      windowClass.maximize(window.MaximizePresentation.EXIT_IMMERSIVE).then(() => {
        LogUtil.info(TAG, 'Succeeded in maximizing the window.');
      }).catch((err: BusinessError) => {
        LogUtil.error(TAG, 'Failed to maximize the window. Cause:' + JSON.stringify(err));
      });

      if (!this.deviceInfoAdapter.isPcMode()) {
        this.abilityManager.getProxy(id)?.updateAvoidAreaStatus(false);
      }
    } catch (exception) {
      LogUtil.error(TAG, 'maximize the window exception. Cause:' + JSON.stringify(exception));
    }
  }

  @LogMethod
  unMaximize(id: number) {
    let windowClass = this.getWindow(id);
    if (!windowClass) {
      LogUtil.error(TAG, 'Failed to obtain the main window.');
      return;
    }

    try {
      windowClass.recover().then(() => {
        LogUtil.info(TAG, 'Succeeded in setting the system bar to be invisible.');
      }).catch((err: BusinessError) => {
        LogUtil.error(TAG, 'Failed to recover the window. Cause: ' + JSON.stringify(err));
      });
    } catch (exception) {
      LogUtil.error(TAG, 'recover the window exception. Cause:' + JSON.stringify(exception));
    }
  }

  @LogMethod
  minimize(id: number) {
    let windowClass = this.getWindow(id);
    if (!windowClass) {
      LogUtil.error(TAG, 'Failed to obtain the main window.');
      return;
    }
    windowClass.minimize()
      .catch((err: BusinessError) => {
        LogUtil.error(TAG, 'Failed to minimize the window. Cause: ' + JSON.stringify(err));
      });
  }

  @LogMethod
  setTitle(title:string, id: number) {
    let proxy = this.abilityManager.getProxy(id);
    proxy?.setWindowTitle(title);
  }

  @LogMethod
  relaunch(): boolean {
    let applicationContext = this.ctxAdapter.getContext().getApplicationContext();
    let want: Want = {
      bundleName: this.bundleName,
      abilityName: 'EntryAbility'
    };
    try {
      applicationContext.restartApp(want);
      return true;
    } catch (err) {
      LogUtil.error(TAG, 'restartApp fail, error:' + JSON.stringify(err))
      return false;
    }
  }

  @LogMethod
  setMovable(id: number, movable: boolean) {
    let window = this.getWindow(id);
    try {
      window?.setWindowTitleMoveEnabled(movable);
    } catch (err) {
      LogUtil.error(TAG, 'Failed to set the window Movable. Cause: ' + JSON.stringify(err));
    }
  }

  @LogMethod
  setResizable(id: number, resizable: boolean) {
    let window = this.getWindow(id);
    try {
      window?.setResizeByDragEnabled(resizable)
        .catch((err: BusinessError) => {
          LogUtil.error(TAG, `Failed to set the window resizable. Cause ${JSON.stringify(err)}`);
        });
    } catch (err) {
      LogUtil.error(TAG, 'Failed to set the window resizable. Cause: ' + JSON.stringify(err));
    }
  }

  @LogMethod
  startWindowMoving(id: number) {
    let window = this.getWindow(id);
    window?.startMoving()
      .catch((err: BusinessError) => {
        LogUtil.error(TAG, `startWindowMoving fail, error info: ${JSON.stringify(err)}`);
      });
  }

  @LogMethod
  startWindowMovingWithOffset(id: number, offsetX: number, offsetY: number) {
    let window = this.getWindow(id);
    window?.startMoving(offsetX, offsetY)
      .catch((err: BusinessError) => {
        LogUtil.error(TAG, `startWindowMovingWithOffset fail, error info: ${JSON.stringify(err)}`);
      });
  }

  @LogMethod
  getWindowsByCoordinate(coordinate: PointCoordinate,
    callback: (windowIds: Array<number>) => void) {
    let windowIds = new Array<number>();
    try {
      window.getWindowsByCoordinate(coordinate.displayId, this.queryWindowNum,
        coordinate.x, coordinate.y).then((windows) => {
        for (let win of windows) {
          let originWindowId: number = win.getWindowProperties().id
          let windowId = this.abilityManager.getWindowIdByOriginWindow(originWindowId);
          if (windowId) {
            windowIds.push(windowId);
          }
        }
        callback(windowIds);
      }).catch((err: BusinessError) => {
        LogUtil.error(TAG, 'window.getWindowsByCoordinate fail, code:' + JSON.stringify(err.code) +
          ',message:' + JSON.stringify(err.message));
        callback(windowIds);
      });
    } catch (err) {
      LogUtil.error(TAG, 'getWindowsByCoordinate fail, error:' + JSON.stringify(err));
      callback(windowIds);
    }
  }

  @LogMethod
  restore(id: number) {
    let windowClass = this.getWindow(id);
    if (!windowClass) {
      LogUtil.error(TAG, 'Failed to obtain the main window.');
      return;
    }
    windowClass.restore().then(() => {
      LogUtil.info(TAG, 'Succeeded in restoring the window.');
    }).catch((err: BusinessError) => {
      LogUtil.error(TAG, 'Failed to restore the window. Cause: ' + JSON.stringify(err));
    });
  }

  @LogMethod
  getOriginWindowIds(windowIds: Array<number>): Array<number> {
    let originWindowIds = new Array<number>();
    try {
      for (let i = 0; i < windowIds.length; i++) {
        let originWindowId = this.abilityManager.getProxy(windowIds[i])?.getOriginWindowId();
        originWindowIds.push(originWindowId ? originWindowId : -1);
      }
    } catch (err) {
      let message = (err as BusinessError).message;
      LogUtil.error(TAG, `getOriginWindowIds failed.: ${message}`);
    }
    return originWindowIds;
  }

  @LogMethod
  shiftWindowEvent(sourceId: number, targetId: number,
    callback: (result: boolean) => void): void {
    try {
      let sourceOriginWindowId = this.abilityManager.getProxy(sourceId)?.getOriginWindowId();
      let targetOriginWindowId = this.abilityManager.getProxy(targetId)?.getOriginWindowId();
      window.shiftAppWindowPointerEvent(sourceOriginWindowId, targetOriginWindowId).then(() => {
        LogUtil.info(TAG, 'shiftWindowEvent success.');
        callback(true);
      }).catch((err: BusinessError) => {
        LogUtil.error(TAG, 'Failed to shiftWindowEvent. Cause: ' + JSON.stringify(err));
        callback(false);
      })
    } catch (err) {
      let message = (err as BusinessError).message;
      LogUtil.error(TAG, `shiftWindowEvent failed.: ${message}`);
      callback(false);
    }
  }

  @LogMethod
  setWindowModal(modal: boolean, id: number) {
    let windowStage = this.abilityManager.getProxy(id)?.getWindowStage();
    windowStage?.setWindowModal(modal)
      .catch((err: BusinessError) => {
        LogUtil.error(TAG, 'Failed to set window model. Cause: ' + JSON.stringify(err));
      });
  }

  @LogMethod
  setFocusable(isFocusable: boolean, id: number) {
    let window = this.getWindow(id);
    try {
      window?.setWindowFocusable(isFocusable)
        .catch((err: BusinessError) => {
          LogUtil.error(TAG, `Failed to set the window Focusable. Cause ${JSON.stringify(err)}`);
        });
    } catch (err) {
      LogUtil.error(TAG, 'Failed to set the window Focusable. Cause: ' + JSON.stringify(err));
    }
  }

  @LogMethod
  setParent(id: number, parent_id: number) {
    let window = this.getWindow(id);
    let parentWindowId = this.abilityManager.getProxy(parent_id)?.getOriginWindowId();
    try {
      window?.setParentWindow(parentWindowId)
        .catch((err: BusinessError) => {
          LogUtil.error(TAG, `Failed to set parent window. Cause ${JSON.stringify(err)}`);
        });
    } catch (err) {
      LogUtil.error(TAG, 'Failed to set parent window. Cause: ' + JSON.stringify(err));
    }
  }

  private findWindow(id: number): IWindowInfo | undefined {
    for (let i = 0; i < this.windowList.length; i++) {
      const item = this.windowList[i];
      if (item && item.id === id) {
        return item;
      }
    }
    return undefined;
  }

  private removeWindow(id: number) {
    for (let i = 0; i < this.windowList.length; i++) {
      const item = this.windowList[i];
      if (item && item.id === id) {
        this.windowList.splice(i, 1);
        break;
      }
    }
  }
};

// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { inject, injectable } from 'inversify';
import window from '@ohos.window';
import { IParams,
  ISubWindowInfo,
  NewWindowParam,
  WindowBound,
  WindowStyles} from '../interface/CommonInterface';
import { AbilityManager } from '../common/AbilityManager';
import { BaseAdapter } from '../common/BaseAdapter';
import LogUtil from '../utils/LogUtil';
import LogMethod from '../common/LogDecorator';
import { ConfigurationConstant } from '@kit.AbilityKit';
import { deviceInfo, BusinessError } from '@kit.BasicServicesKit';
import { SystemFloatingWindowManager } from '../common/SystemFloatingWindowManager';
import { WineProxy } from '../interface/WineProxy';
import { ContextAdapter } from './ContextAdapter';
import { WindowStyle } from '../common/WindowStyle';

const TAG: string = 'SubWindow';
const MAX_TOTAL_SUB_WINDOW_NUM: number = 20;

@injectable()
export class SubWindowAdapter extends BaseAdapter {
  private abilityManager: AbilityManager;
  private systemFloatingWindowManager: SystemFloatingWindowManager;
  private ctxAdapter: ContextAdapter;
  private subWindowList: (ISubWindowInfo)[] = [];
  private reuseSubWindowMap: Map<number, (ISubWindowInfo)[]> =
    new Map<number, (ISubWindowInfo)[]>();
  private globalCache: (ISubWindowInfo)[] = [];
  private windowStyle: WindowStyle;

  constructor(
    @inject(AbilityManager) abilityManager: AbilityManager,
    @inject(SystemFloatingWindowManager) systemFloatingWindowManager: SystemFloatingWindowManager,
    @inject(ContextAdapter) ctxAdapter: ContextAdapter,
    @inject(WindowStyle) windowStyle: WindowStyle
  ) {
    super();
    this.abilityManager = abilityManager;
    this.systemFloatingWindowManager = systemFloatingWindowManager;
    this.ctxAdapter = ctxAdapter;
    this.windowStyle = windowStyle;
  }

  @LogMethod
  createWindow(param: NewWindowParam, callback: (id: number) => void) {
    let parentId = this.getParentWindowId(param.parent_id);
    let bounds = param.bounds;
    LogUtil.info(TAG, `createWindow widget_id: ${param.widget_id},
      parentId: ${param.parent_id} --> ${parentId},
      bounds: (${bounds.left}, ${bounds.top}, ${bounds.width}, ${bounds.height})`);

    const item = this.findReuseWindow(parentId, bounds);
    if (item) {
      this.reuseSubWindow(item, param, bounds, parentId, callback);
    } else {
      this.createSubWindow(param, bounds, parentId, callback);
    }
  }

  private reuseSubWindow(item: ISubWindowInfo,
    param: NewWindowParam,
    bounds: WindowBound,
    parentId: number,
    callback: (id: number) => void) {
    try {
      let windowClass: window.Window = item.subWindow;
      let paramsLink: SubscribedAbstractProperty<IParams> = item.localStorage.link('subWindowParams');
      let lastParams = paramsLink.get();
      lastParams.initColorRgb = param.init_color_argb;
      paramsLink.set(lastParams);
      this.resizeSubWindow(windowClass, bounds.width, bounds.height);
      this.moveSubWindow(windowClass, bounds.left, bounds.top, parentId);

      let styles = this.windowStyle.buildWindowStyles(param.feature_mask, param.window_state_mask);
      LogUtil.info(TAG, 'reuseSubWindow loadContent styles:' + JSON.stringify(styles));

      // compute Window Styles
      this.computeWindowStyles(windowClass, styles);

      styles.show_window && windowClass.showWindow()
        .catch((err: BusinessError) => {
          LogUtil.error(TAG, 'Failed to reuseSubWindow: ' + item.id + ',err:' + JSON.stringify(err));
          callback(-1);
          return;
        });
      callback(item.id);
    } catch (err) {
      LogUtil.error(TAG,'reuseSubWindow: ' + item.id + ',err: ' + JSON.stringify(err));
      callback(-1);
    }
  }

  private getParentProxy(parentId: number): WineProxy {
    let proxy = this.abilityManager.getProxy(parentId);
    if (!proxy) {
      let mainWindowId = this.systemFloatingWindowManager.getMainWindowId(parentId);
      proxy = this.abilityManager.getProxy((mainWindowId));
    }
    if (!proxy) {
      proxy = this.ctxAdapter.getActiveProxy();
    }
    return proxy;
  }

  private getParentWindow(parentId: number): window.Window | undefined {
    return this.getParentProxy(parentId).getWindow();
  }

  private getParentWindowId(parentId: number): number {
    return this.getParentProxy(parentId).getWindowId();
  }

  private findSubWindow(id: number): ISubWindowInfo | undefined {
    for (let i = 0; i < this.subWindowList.length; i++) {
      const item = this.subWindowList[i];
      if (item && item.id === id) {
        return item;
      }
    }
    return undefined;
  }

  private findReuseWindow(parentId: number, bounds: WindowBound)
    : ISubWindowInfo | undefined {
    let reuseWindowList = this.reuseSubWindowMap.get(parentId);
    if (!reuseWindowList) {
      return undefined;
    }
    for (let i = 0; i < reuseWindowList.length; i++) {
      const item = reuseWindowList[i];
      if (!item) {
        continue;
      }
      const rect = item.subWindow.getWindowProperties().windowRect;
      const maxDistance:number = 2;
      if (Math.abs(rect.width - bounds.width) <= maxDistance &&
        Math.abs(rect.height - bounds.height) <= maxDistance) {
        reuseWindowList.splice(i, 1);
        let found = false;
        for (let j = 0; j < this.globalCache.length; j++) {
          if (this.globalCache[j].parentId === item.parentId &&
            this.globalCache[j].id === item.id) {
            found = true;
            this.globalCache.splice(j, 1);
            break;
          }
        }
        if (!found) {
          LogUtil.error(TAG, 'Global cache is inconsistent with cache of ' + item.parentId);
        }
        LogUtil.info(TAG, `find reuse window: ${item.id}, parentId: ${parentId}, size: ${reuseWindowList.length}`);
        this.subWindowList.push(item);
        return item;
      }
    }
    return undefined;
  }

  private createSubWindow(param: NewWindowParam,
    bounds: WindowBound,
    parentId: number,
    callback: (id: number) => void) {
    const para: Record<string, IParams> = {
      'subWindowParams': {
        id: param.widget_id,
        size: [bounds.width, bounds.height],
        initColorRgb: param.init_color_argb,
        ownerPid: param.owner_pid,
      }
    };
    let instanceKey = `${param.process_key}:${param.widget_id}`;
    let subStorage: LocalStorage = new LocalStorage(para);
    let styles = this.windowStyle.buildWindowStyles(param.feature_mask, param.window_state_mask);
    let proxy = this.getParentProxy(parentId);
    LogUtil.info(TAG, 'createSubWindow instanceKey:' + instanceKey + ',styles:' + JSON.stringify(styles) +
      ',feature_mask:' + param.feature_mask + ',window_state_mask:' + param.window_state_mask);
    proxy?.createSubWindow(instanceKey)
      .then((data: window.Window) => {
        this.initSubWindow(data, parentId, param.widget_id, bounds, subStorage, callback, styles);
      }).catch((error: BusinessError) => {
      LogUtil.error(TAG, 'Failed to create the subWindow. Cause: ' + JSON.stringify(error));
      callback(-1);
    });
  }

  private initSubWindow(data: window.Window,
    parentId: number,
    widget_id: number,
    bounds: WindowBound,
    subStorage: LocalStorage,
    callback: (id: number) => void,
    styles: WindowStyles) {
    try {
      let windowClass = data;
      styles.hide_title_bar && windowClass.setWindowCornerRadius(0)
        .catch((err: BusinessError) => {
          LogUtil.error(TAG, 'Failed to set corner radius. Cause:' + JSON.stringify(err));
        });
      windowClass.keepKeyboardOnFocus(true);

      windowClass.on('windowStatusChange', (status) => {
        this.nativeContext.OnWindowStatusChange(widget_id, status);
      });

      windowClass.on("windowRectChange", (option) => {
        this.nativeContext.OnWindowRectChange(widget_id, option.rect, option.reason.valueOf());
      });

      windowClass.on('windowEvent', (type) => {
        this.nativeContext.OnWindowEvent(widget_id, type.valueOf());
      });

      windowClass.on("windowVisibilityChange", (visible) => {
        this.nativeContext.OnWindowVisibilityChange(widget_id, visible);
      });

      this.moveSubWindow(windowClass, bounds.left, bounds.top, parentId);
      if (bounds.width > 0 && bounds.height > 0) {
        this.resizeSubWindow(windowClass, bounds.width, bounds.height);
      }

      windowClass.loadContent('pages/SubWindow', subStorage)
        .then(() =>{
          // compute Window Styles
          this.computeWindowStyles(windowClass, styles);

          styles.show_window && windowClass.showWindow()
            .catch((err: BusinessError) => {
              LogUtil.error(TAG, 'Failed to show SubWindow. Cause: ' + JSON.stringify(err));
              callback(-1);
              return;
            });

          this.subWindowList.push({
            id: widget_id,
            parentId: parentId,
            subWindow: data,
            localStorage: subStorage
          });
          callback(widget_id);
        }).catch((err: BusinessError) => {
        LogUtil.error(TAG, 'Failed to load the content. Cause:' + JSON.stringify(err));
        callback(-1);
      });
    } catch (err) {
      LogUtil.error(TAG,'Failed to create sub window. Cause: ' + JSON.stringify(err));
      callback(-1);
    }
  }

  private computeWindowStyles(windowClass: window.Window, styles: WindowStyles) {
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
      if (styles.always_on_top) {
        windowClass.raiseToAppTop()
          .catch((err: BusinessError) => {
            LogUtil.error(TAG, 'Failed to setTopmost. Cause:' + JSON.stringify(err));
          });
      }
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
    } catch (err) {
      LogUtil.error(TAG, 'Failed to compute window styles. Cause:' + JSON.stringify(err));
    }
  }

  private async resizeSubWindow(windowClass: window.Window, width: number, height: number) {
    return await windowClass.resizeAsync(width, height)
      .then(() => {
        LogUtil.info(TAG, `Succeeded in resize window, ${width}x${height}`);
      }).catch((err: BusinessError) => {
        LogUtil.error(TAG, `Failed to resize window. Cause code: ${err.code}, message: ${err.message}`);
      });
  }

  private moveSubWindow(windowClass: window.Window, x: number, y: number, parentId: number) {
    try {
      let displayId : number | undefined;
      let mainWindow = this.getParentWindow(parentId);
      if (mainWindow) {
        const prop = mainWindow.getWindowProperties();
        displayId = prop.displayId;
      }
      let config: window.MoveConfiguration = {
        displayId: displayId
      };
      windowClass.moveWindowToAsync(x, y, config)
        .catch((err: BusinessError) => {
          LogUtil.error(TAG, `Failed to move the window. Cause code: ${err.code},
            message: ${err.message}, display id: ${displayId}`);
        });
    } catch (exception) {
      LogUtil.error(TAG, `Failed to move the window. Cause code: ${exception.code},
          message: ${exception.message}`);
    }
  }

  @LogMethod
  closeWindow(id: number) {
    for (let i = 0; i < this.subWindowList.length; i++) {
      const item = this.subWindowList[i];
      if (item && item.id === id) {
        item.subWindow.minimize().catch((err: BusinessError) => {
          LogUtil.error(TAG, 'Failed to close the subWindow. Cause:' + JSON.stringify(err));
        });
        this.addReuseWindow(item);
        this.subWindowList.splice(i, 1);
        break;
      }
    }
  }

  @LogMethod
  destroyWindow(id: number) {
    for (let i = 0; i < this.subWindowList.length; i++) {
      const item = this.subWindowList[i];
      if (item && item.id === id) {
        item.subWindow.destroyWindow()
          .catch((err: BusinessError) => {
            LogUtil.error(TAG, 'Failed to destroy the subWindow. Cause:' + JSON.stringify(err));
          });
        this.subWindowList.splice(i, 1);
        break;
      }
    }
  }

  private addReuseWindow(item: ISubWindowInfo) {
    if (!item) {
      LogUtil.error(TAG, 'addReuseWindow fail');
    }
    let total = this.getTotalSubWindowNum();
    this.globalCache.push(item);
    let reuseWindowList = this.reuseSubWindowMap.get(item.parentId);
    if (reuseWindowList) {
      reuseWindowList.push(item);
    } else {
      this.reuseSubWindowMap.set(item.parentId, [item]);
    }
    if (total > MAX_TOTAL_SUB_WINDOW_NUM) {
      let oldest = this.globalCache.shift();
      if (oldest) {
        let target = this.reuseSubWindowMap.get(oldest.parentId);
        if (!target) {
          LogUtil.error(TAG, 'Global cache is inconsistent with cache of ' + oldest.parentId);
        } else {
          let found = false;
          for (let i = 0; i < target.length; i++) {
            if (oldest.id === target[i].id) {
              found = true;
              target.splice(i, 1);
              break;
            }
          }
          if (!found) {
            LogUtil.error(TAG, 'Global cache is inconsistent with cache of ' + oldest.parentId);
          }
        }
        oldest.subWindow.destroyWindow().catch((err: BusinessError) => {
          LogUtil.error(TAG, 'Failed to destroy the subWindow. Cause:' + JSON.stringify(err));
        });
      } else {
        LogUtil.error(TAG, 'Global cache should not be empty at this point');
      }
    }
  }

  private getTotalSubWindowNum(): number {
    return this.subWindowList.length + this.globalCache.length;
  }

  @LogMethod
  hideWindow(id: number) {
    let windowClass = this.findSubWindow(id)?.subWindow;
    if (!windowClass) {
      LogUtil.error(TAG, 'Failed to obtain the SubWindow.');
      return;
    }

    windowClass.minimize().catch((err: BusinessError) => {
      LogUtil.error(TAG, 'Failed to hide SubWindow. Cause: ' + JSON.stringify(err));
    });
  }

  @LogMethod
  showWindow(id: number) {
    let windowClass = this.findSubWindow(id)?.subWindow;
    if (!windowClass) {
      LogUtil.error(TAG, 'Failed to obtain the SubWindow.');
      return;
    }

    windowClass.showWindow().catch((err: BusinessError) => {
      LogUtil.error(TAG, 'Failed to show SubWindow. Cause: ' + JSON.stringify(err));
    });
  }

  @LogMethod
  setBounds(id: number, bounds: WindowBound) {
    let windowClass = this.findSubWindow(id)?.subWindow;
    if (!windowClass) {
      LogUtil.error(TAG, 'Failed to obtain the SubWindow.');
      return;
    }

    LogUtil.info(TAG, `setBounds widget_id: ${id},
      bounds: (${bounds.left}, ${bounds.top}, ${bounds.width}, ${bounds.height})`);
    windowClass.resizeAsync(bounds.width, bounds.height)
      .then(() => {
        windowClass?.moveWindowToAsync(bounds.left, bounds.top)
          .catch((err: BusinessError) => {
            LogUtil.error(TAG, 'Failed to move the subwindow. Cause:' + JSON.stringify(err));
          });
      }).catch((err: BusinessError) => {
      LogUtil.error(TAG, 'Failed to change the subWindow size. Cause:' + JSON.stringify(err));
    });
  }

  @LogMethod
  setFullscreen(id: number) {
    let windowClass = this.findSubWindow(id)?.subWindow;
    if (!windowClass) {
      LogUtil.error(TAG, 'Failed to obtain the SubWindow.');
      return;
    }

    windowClass.maximize(window.MaximizePresentation.ENTER_IMMERSIVE_DISABLE_TITLE_AND_DOCK_HOVER)
      .catch((err: BusinessError) => {
        LogUtil.error(TAG, 'Failed to maximize the window. Cause:' + JSON.stringify(err));
      });
  }

  @LogMethod
  maximize(id: number) {
    let windowClass = this.findSubWindow(id)?.subWindow;
    if (!windowClass) {
      LogUtil.error(TAG, 'Failed to obtain the SubWindow.');
      return;
    }

    windowClass.maximize(window.MaximizePresentation.EXIT_IMMERSIVE).then(() => {
      LogUtil.info(TAG, 'Succeeded in maximizing the window.');
    }).catch((err: BusinessError) => {
      LogUtil.error(TAG, 'Failed to maximize the window. Cause:' + JSON.stringify(err));
    });
  }

  @LogMethod
  unMaximize(id: number) {
    let windowClass = this.findSubWindow(id)?.subWindow;
    if (!windowClass) {
      LogUtil.error(TAG, 'Failed to obtain the SubWindow.');
      return;
    }

    windowClass.recover().catch((err: BusinessError) => {
      LogUtil.error(TAG, 'Failed to recover the window. Cause: ' + JSON.stringify(err));
    });
  }

  @LogMethod
  minimize(id: number) {
    let windowClass = this.findSubWindow(id)?.subWindow;
    if (!windowClass) {
      LogUtil.error(TAG, 'Failed to obtain the SubWindow.');
      return;
    }

    windowClass.minimize()
      .catch((err: BusinessError) => {
        LogUtil.error(TAG, 'Failed to minimize the window. Cause: ' + JSON.stringify(err));
      });
  }

  @LogMethod
  restore(id: number) {
    let windowClass = this.findSubWindow(id)?.subWindow;
    if (!windowClass) {
      LogUtil.error(TAG, 'Failed to obtain the SubWindow.');
      return;
    }

    windowClass.restore().catch((err: BusinessError) => {
      LogUtil.error(TAG, 'Failed to restore the window. Cause: ' + JSON.stringify(err));
    });
  }

  @LogMethod
  activateWindow(id: number) {
    this.showWindow(id);
  }
}

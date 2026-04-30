// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { inject, injectable } from 'inversify';
import { BusinessError } from '@ohos.base';
import { window } from '@kit.ArkUI';
import { BaseAdapter } from '../common/BaseAdapter';
import LogUtil from '../utils/LogUtil';
import LogMethod from '../common/LogDecorator';
import { NewWindowParam, WindowBound } from '../interface/CommonInterface';
import lazy { SystemFloatingWindowManager } from '../common/SystemFloatingWindowManager';
import lazy { ContextAdapter } from './ContextAdapter';
import { common } from '@kit.AbilityKit';
import StringUtil from '../utils/StringUtil';

const TAG: string = 'SystemFloatingWindowWindow';

@injectable()
export class SystemFloatingWindowAdapter extends BaseAdapter {
  private ctxAdapter: ContextAdapter;
  private systemFloatingWindowManager: SystemFloatingWindowManager;

  constructor(
    @inject(ContextAdapter) ctxAdapter: ContextAdapter,
    @inject(SystemFloatingWindowManager) systemFloatingWindowManager: SystemFloatingWindowManager,
  ) {
  super();
    this.ctxAdapter = ctxAdapter;
    this.systemFloatingWindowManager = systemFloatingWindowManager;
  }

  @LogMethod
  createWindow(param: NewWindowParam, callback: (id: number) => void) {
    let activeContext = this.ctxAdapter.getActiveContext() as common.UIAbilityContext;
    if (!activeContext) {
      LogUtil.warn(TAG, 'Failed to get active context');
      return;
    }

    let bounds: WindowBound = {
      left: param.bounds.left,
      top: param.bounds.top,
      width: param.bounds.width,
      height: param.bounds.height,
    };
    const out_bounds = this.adjustBounds(bounds);
    let config: window.Configuration = {
      name: StringUtil.widgetConvert(param.widget_id),
      windowType: window.WindowType.TYPE_FLOAT,
      ctx: activeContext,
      decorEnabled: true
    };
    window.createWindow(config)
      .then((data: window.Window) => {
        try {
          data.moveWindowTo(out_bounds.left, out_bounds.top)
            .then(() => {
              data.resize(out_bounds.width, out_bounds.height)
                .catch((err: BusinessError) => {
                  LogUtil.error(TAG, 'Failed to change the window size. Cause:' + JSON.stringify(err));
                });
            }).catch((err: BusinessError) => {
            LogUtil.error(TAG, 'Failed to move the window. Cause:' + JSON.stringify(err));
          });
        } catch (exception) {
          LogUtil.error(TAG, 'move the window exception. Cause:' + JSON.stringify(exception));
        }

        try {
          data.on('windowSizeChange', (size) => {
            const windowProp = data.getWindowProperties();
            let windowBound: WindowBound = {
              left: windowProp.windowRect.left + windowProp.drawableRect.left,
              top: windowProp.windowRect.top + windowProp.drawableRect.top,
              width: size.width - windowProp.drawableRect.left - windowProp.drawableRect.left,
              height: size.height - windowProp.drawableRect.top - windowProp.drawableRect.top,
            };
            this.nativeContext.OnWindowSizeChange(param.widget_id, windowBound);
          });
        } catch (exception) {
          LogUtil.error(TAG,'Failed to enable the listener for window size changes. Cause: ' + JSON.stringify(exception));
        }

        try {
          data.on("windowRectChange", (option) => {
            this.nativeContext.OnWindowRectChange(param.widget_id, option.rect, option.reason.valueOf());
          });
        } catch (exception) {
          LogUtil.error(TAG,'Failed to enable the listener for window rect changes. Cause: ' + JSON.stringify(exception));
        }

        try {
          data.on('windowEvent', (type) => {
            this.nativeContext.OnWindowEvent(param.widget_id, type.valueOf());
          });
        } catch (exception) {
          LogUtil.error(TAG, 'Failed to register callback. Cause: ' + JSON.stringify(exception));
        }

        try {
          data.on("windowVisibilityChange", (visible) => {
            this.nativeContext.OnWindowVisibilityChange(param.widget_id, visible);
          });
        } catch (exception) {
          LogUtil.error(TAG, 'Failed to register callback. Cause: ' + JSON.stringify(exception));
        }

        let windowClass = data;
        this.cacheWindow(param.widget_id, windowClass);

        let startUri: string = ''
        const para: Record<string, Object> = {
          'widgetId': param.widget_id,
          'startUri': startUri,
          'ownerPid': param.owner_pid,
        };
        let storage: LocalStorage = new LocalStorage(para);
        windowClass.loadContent("pages/Index", storage, (err, data) => {
          let errCode: number = err.code;
          if (errCode) {
            LogUtil.error(TAG, 'Failed to load the content. Cause:' + JSON.stringify(err));
            return;
          }
          windowClass.showWindow()
            .catch((err: BusinessError) => {
              LogUtil.error(TAG, 'Failed to show the window. Cause:' + JSON.stringify(err));
            });
        })
        callback(param.widget_id);
      }).catch((error: BusinessError) => {
      LogUtil.error(TAG, 'Failed to create window. Cause: ' + JSON.stringify(error));
    });
  }

  @LogMethod
  closeWindow(id: number) {
    let mainWindowId = this.systemFloatingWindowManager.getMainWindowId(id);
    if (mainWindowId) {
      this.systemFloatingWindowManager.getFloatWindow(id)?.destroyWindow();
      this.systemFloatingWindowManager.removeSystemFloatingWindow(mainWindowId, id);
    }
  }

  @LogMethod
  setBounds(id: number,
            bounds: WindowBound,
            onCompleted: (ret: boolean) => void) {
    let windowClass = this.systemFloatingWindowManager.getFloatWindow(id);
    if (!windowClass) {
      onCompleted(false);
      LogUtil.error(TAG, 'Failed to obtain the main window.');
      return;
    }

    const windowProp = windowClass?.getWindowProperties();
    let xComBorder: number = windowProp?.drawableRect.left || 0;
    let xComTop: number = windowProp?.drawableRect.top || 0;
    bounds.left -= xComBorder;
    bounds.top -= xComTop;
    bounds.width = bounds.width + xComBorder + xComBorder;
    bounds.height = bounds.height + xComBorder + xComTop;

    try {
      windowClass.moveWindowTo(bounds.left, bounds.top)
        .then(() => {
          windowClass?.resize(bounds.width, bounds.height)
            .then(() => {
              onCompleted(true);
            }).catch((err: BusinessError) => {
            onCompleted(false);
            LogUtil.error(TAG, 'Failed to change the window size. Cause:' + JSON.stringify(err));
          });
        }).catch((err: BusinessError) => {
        onCompleted(false);
        LogUtil.error(TAG, 'Failed to move the window. Cause:' + JSON.stringify(err));
      });
    } catch (exception) {
      onCompleted(false);
      LogUtil.error(TAG, 'move the window exception. Cause:' + JSON.stringify(exception));
    }
  }

  @LogMethod
  setWindowLimits(minWidth: number,
                  minHeight: number,
                  maxWidth: number,
                  maxHeight: number,
                  id: number) {
    let windowClass = this.systemFloatingWindowManager.getFloatWindow(id);
    const windowProp = windowClass?.getWindowProperties();
    let xComBorder: number = windowProp?.drawableRect.left || 0;
    let xComTop: number = windowProp?.drawableRect.top || 0;

    try {
      let windowLimits: window.WindowLimits = {
        minWidth: minWidth === 0 ? minWidth : minWidth + xComBorder + xComBorder,
        minHeight: minHeight === 0 ? minHeight : minHeight + xComTop + xComBorder,
        maxWidth: maxWidth === 0 ? maxWidth : maxWidth + xComBorder + xComBorder,
        maxHeight: maxHeight === 0 ? maxHeight : maxHeight + xComTop + xComBorder,
      };
      windowClass?.setWindowLimits(windowLimits)
        .catch((err: BusinessError) => {
          LogUtil.error(TAG, 'Failed to change the window limits. Cause: ' + JSON.stringify(err));
        });
    } catch (exception) {
      LogUtil.error(TAG, 'change the window limits exception. Cause:' + JSON.stringify(exception));
    }
  }

  @LogMethod
  startWindowMoving(id: number) {
    let window = this.systemFloatingWindowManager.getFloatWindow(id);
    window?.startMoving().then(() => {
    }).catch((err: BusinessError) => {
      LogUtil.error(TAG, `startWindowMoving fail, error info: ${JSON.stringify(err)}`);
    });
  }

  private adjustBounds(bounds: WindowBound): WindowBound {
    let out_bounds = bounds;
    let mainWindow = this.ctxAdapter.getActiveProxy().getWindow();
    if (!mainWindow) {
      return out_bounds;
    }
    const windowProp = mainWindow?.getWindowProperties();
    let xComBorder: number = windowProp?.drawableRect.left || 0;
    let xComTop: number = windowProp?.drawableRect.top || 0;
    out_bounds.left -= xComBorder;
    out_bounds.top -= xComTop;
    out_bounds.width += xComBorder + xComBorder;
    out_bounds.height += xComBorder + xComTop;
    return out_bounds;
  }

  private cacheWindow(id: number, windowClass: window.Window) {
    let mainWindowId = this.ctxAdapter.getActiveProxy().getWindowId();
    if (mainWindowId) {
      this.systemFloatingWindowManager.addSystemFloatingWindow(mainWindowId, {
        "id": id,
        "window": windowClass
      });
    }
  }
};

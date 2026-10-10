// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import LogUtil from '../utils/LogUtil';
import window from '@ohos.window';
import Want from '@ohos.app.ability.Want';
import AbilityConstant from '@ohos.app.ability.AbilityConstant';
import { NativeThemeAdapter } from '../adapter/NativeThemeAdapter';
import { AbilityManager } from '../common/AbilityManager';
import { Configuration } from '@ohos.app.ability.Configuration';
import Inject from '../common/InjectModule';
import { CloseResponse, CaptionButtonRect, WindowBound, WindowLimits } from '../interface/CommonInterface';
import { DisplayAdapter } from  '../adapter/DisplayAdapter';
import { WineBaseAbility } from './WineBaseAbility';
import { ConfigData } from '../common/Constants';
import { PermissionManagerAdapter } from '../adapter/PermissionManagerAdapter';
import { deviceInfo, emitter, settings } from '@kit.BasicServicesKit';
import { common, ConfigurationConstant } from '@kit.AbilityKit';
import { DeviceInfoAdapter } from '../adapter/DeviceInfoAdapter';
import lazy { StatusBarCallback, TrayManager } from '../common/TrayManager';
import { WineLoaderAdapter } from '../adapter/WineLoaderAdapter';

const TAG: string = 'WineAbility';

export class WineAbility extends WineBaseAbility {
  private abilityManager: AbilityManager | undefined = undefined;
  private displayAdapter : DisplayAdapter | undefined = undefined;
  private nativeThemeAdapter: NativeThemeAdapter | undefined = undefined;
  private deviceInfoAdapter: DeviceInfoAdapter | undefined = undefined;
  private wineLoader: WineLoaderAdapter | undefined = undefined;
  private callback: StatusBarCallback = {
    iconClick: (data: emitter.EventData) => {
      LogUtil.info(TAG, 'iconClick data: ' + JSON.stringify(data));
    },
    rightMenuClick: (data: emitter.EventData) => {
      LogUtil.info(TAG, 'rightMenuClick data: ' + JSON.stringify(data));
    },
  }

  onConfigurationUpdate(config: Configuration) {
    this.nativeThemeAdapter?.setSystemNativeTheme(config.colorMode);
    this.displayAdapter?.onSystemFontSizeChange(config.fontSizeScale);
  }

  onCreate(want: Want, launchParam: AbilityConstant.LaunchParam) {
    super.onCreate(want, launchParam);

    this.abilityManager = Inject.get(AbilityManager);
    this.nativeThemeAdapter = Inject.get(NativeThemeAdapter);
    this.displayAdapter = Inject.get(DisplayAdapter);
    this.displayAdapter.getDefaultDisplay((display) => {
      this.nativeContext.SetDefaultDisplay(display);
    });
    Inject.get(PermissionManagerAdapter).initPermissions();
    this.deviceInfoAdapter = Inject.get(DeviceInfoAdapter);
    Inject.get(TrayManager).addToStatusBar(this.context, this.callback);
    this.wineLoader = Inject.get(WineLoaderAdapter);
  }

  onWindowStageCreate(windowStage: window.WindowStage) {
    super.onWindowStageCreate(windowStage);

    if (this.checkSingleInstance()) {
      LogUtil.warn(TAG, 'repeat launch from desktop, need terminate');
      return;
    }

    this.abilityManager?.addProxy(this);

    windowStage.getMainWindow((err, data) => {
      if (err.code) {
        LogUtil.error(TAG, 'Failed to obtain the main window: ' + JSON.stringify(err));
        return;
      }

      try {
        data.on('windowStatusChange', (status) => {
          this.nativeContext.OnWindowStatusChange(this.widgetId, status);
        })
      } catch (exception) {
        LogUtil.error(TAG,'Failed to listener for window status changes: ' + JSON.stringify(exception));
      }

      try {
        data.on("windowRectChange", (option) => {
          this.nativeContext.OnWindowRectChange(this.widgetId, option.rect, option.reason.valueOf());
        });
      } catch (exception) {
        LogUtil.error(TAG,'Failed to listener for window rect changes: ' + JSON.stringify(exception));
      }

      try {
        data.on('windowEvent', (type) => {
          this.nativeContext.OnWindowEvent(this.widgetId, type.valueOf());
        });
      } catch (exception) {
        LogUtil.error(TAG, 'Failed to register callback: ' + JSON.stringify(exception));
      }

      try {
        data.on("windowVisibilityChange", (visible) => {
          this.nativeContext.OnWindowVisibilityChange(this.widgetId, visible);
        });
      } catch (exception) {
        LogUtil.error(TAG, 'Failed to register callback: ' + JSON.stringify(exception));
      }

      try {
        data.on('keyboardHeightChange', (height) => {
          this.nativeContext.OnKeyboardHeightChange(this.widgetId, height);
        });
      } catch (exception) {
        LogUtil.error(TAG, 'Failed to listener for keyboard height changes: ' + JSON.stringify(exception));
      }
    });

    const para: Record<string, Object> = {
      'widgetId': this.widgetId,
      'window': windowStage.getMainWindowSync(),
      'useDarkMode': this.needDarkMode(),
      'ownerPid': this.ownerPid,
    };
    let storage: LocalStorage = new LocalStorage(para);
    windowStage.loadContent(this.getContentPath(), storage, (err, data) => {
      if (err.code) {
        LogUtil.error(TAG, 'Failed to load the content: ' + JSON.stringify(err));
        return;
      }

      let windowClass: window.Window = windowStage.getMainWindowSync();
      LogUtil.info(TAG, 'loadContent styles:' + JSON.stringify(this.styles));

      // compute Window Styles
      this.computeWindowStyles(windowClass, this.styles);

      // window title button rect change callback must registered after loadContent/setUIContent
      try {
        windowClass.on("windowTitleButtonRectChange", (titleButtonRect) => {
          let uiContext = windowClass.getUIContext();
          let rect: CaptionButtonRect = {
            right: uiContext.vp2px(titleButtonRect.right),
            top: uiContext.vp2px(titleButtonRect.top),
            width: uiContext.vp2px(titleButtonRect.width),
            height: uiContext.vp2px(titleButtonRect.height)
          };
          this.nativeContext.OnCaptionButtonRectChange(this.widgetId, rect);
        });
      } catch (exception) {
        LogUtil.error(TAG, 'Failed to register window title button change callback: ' + JSON.stringify(exception));
      }

      if (deviceInfo.sdkApiVersion >= 15) {
        this.nativeContext.RegisterWindowEventFilter(windowClass.getWindowProperties().id);
      }

      let isPCStatus = settings.getValueSync(this.context, 'isSystemModePc', '', settings.domainName.USER_PROPERTY);
      if (deviceInfo.deviceType === 'tablet' && isPCStatus !== 'true') {
        this.onPadWindowStageCreate(windowClass, storage);
      }

      try {
        if (this.widgetId === ConfigData.DEFAULT_WIDGET_ID) {
          let windowLimits = this.getWindow()?.getWindowLimits();
          let uiContext = windowClass.getUIContext();
          let limits: WindowLimits = {
            maxHeight: Math.ceil(uiContext.px2vp(windowLimits?.maxHeight ?? 0)),
            maxWidth: Math.ceil(uiContext.px2vp(windowLimits?.maxWidth ?? 0)),
            minHeight: Math.ceil(uiContext.px2vp(windowLimits?.minHeight ?? 0)),
            minWidth: Math.ceil(uiContext.px2vp(windowLimits?.minWidth ?? 0))
          }
          this.nativeContext.SetSystemWindowLimits(limits);
        }
      } catch (exception) {
        LogUtil.error(TAG, 'Failed to get window limits or get ui context:' + JSON.stringify(exception));
      }

      this.wineLoader?.loadWine(this.exePath, this.sockPath, this.startUri, this.buildWineArgv())
        .catch((err: Object) => {
          LogUtil.error(TAG, `loadWine failed: ${JSON.stringify(err)}`);
        });
    });
  }

  onWindowStageDestroy() {
    if (this.widgetId <= 0) {
      LogUtil.warn(TAG, 'onWindowStageDestroy component is not instantiated');
      return;
    }
    if (deviceInfo.sdkApiVersion >= 15) {
      let originWindowId = this.abilityManager?.getOriginWindowId(this.widgetId);
      this.nativeContext.ClearWindowEventFilter(originWindowId);
    }
    // Main window is destroyed, release UI related resources
    this.abilityManager?.removeProxy(this.widgetId);
  }

  onNewWant(want: Want, launchParam: AbilityConstant.LaunchParam): void {
    let uriNotNull = want["uri"] !== undefined && want["uri"] !== '';
    if(want!== undefined && want !== null) {
      LogUtil.info(TAG, 'onNewWant begin, want: ' +
        'bundleName: ' + JSON.stringify(want.bundleName) +
        ', abilityName: ' + JSON.stringify(want.abilityName) +
        ', deviceId: ' + JSON.stringify(want.deviceId) +
        ', uri not null: ' + JSON.stringify(uriNotNull) +
        ', type: ' + JSON.stringify(want.type) +
        ', flags: ' + JSON.stringify(want.flags) +
        ', action: ' + JSON.stringify(want.action) +
        ', parameters: ' + JSON.stringify(want.parameters) +
        ', entities: ' + JSON.stringify(want.entities) +
        ', moduleName: ' + JSON.stringify(want.moduleName) +
        ', LaunchParam: ' + JSON.stringify(launchParam));
    }
  }

  async onPrepareToTerminateAsync(): Promise<boolean> {
    LogUtil.info(TAG, 'onPrepareToTerminateAsync widgetId:' + this.widgetId +
      ',processKey:' + this.processKey + ',ownerPid:' + this.ownerPid);
    return false;
  }

  async onDestroy(): Promise<void> {
    LogUtil.info(TAG, 'onDestroy widgetId:' + this.widgetId +
      ',processKey:' + this.processKey + ',ownerPid:' + this.ownerPid);
  }

  private checkSingleInstance(): boolean {
    /* In-app window launches carry an explicit widgetId and are always
     * allowed.  Desktop launches do not: onCreate has already allocated a
     * fresh widget id for them (kGetWidget always increments), so testing
     * widgetId here can never detect a repeat -- check whether another
     * ability already owns a live window instead. */
    if (this.isWindowLaunch) {
      return false;
    }
    if (this.abilityManager && !this.abilityManager.isEmpty()) {
      this.closeWindow();
      return true;
    }
    return false;
  }

  private onPadWindowStageCreate(windowClass: window.Window, storage: LocalStorage) {
    // During window initialization, it is not in full-screen state, so the input is false.
    // Here set the status bar avoidance and hides the navigation bar.
    this.updateAvoidAreaStatus(false);

    let avoidArea = windowClass.getWindowAvoidArea(window.AvoidAreaType.TYPE_SYSTEM);
    let statusBarHeight = avoidArea.topRect.height;
    storage.setOrCreate('statusBarHeight', statusBarHeight);

    const context: Context = getContext(this);
    this.deviceInfoAdapter?.registerDeviceModeChange(context);

    windowClass.on('avoidAreaChange', (data) => {
      if (data.type !== window.AvoidAreaType.TYPE_SYSTEM) {
        return;
      }

      let statusBarHeight = data.area.topRect.height;
      storage.setOrCreate('statusBarHeight', statusBarHeight);
      LogUtil.info(TAG, 'avoidArea changes,statusBarHeight:' + statusBarHeight);

      if(!this.deviceInfoAdapter?.isNormalWindowMode()) {
        return;
      }

      let prop = windowClass.getWindowProperties();
      //The current system has a timing issue where the drawableRect lags in updating during mode switching.
      //Here, when switching to normal mode, the drawableRect is set to 0
      //to temporarily maintain normal performance until the issue is resolved.
      prop.drawableRect.height = prop.drawableRect.height + prop.drawableRect.top;
      prop.drawableRect.top = 0;

      let windowBound: window.Rect = {
        left: prop.windowRect.left + prop.drawableRect.left,
        top: prop.windowRect.top + prop.drawableRect.top + statusBarHeight,
        width: prop.drawableRect.width,
        height: prop.drawableRect.height - statusBarHeight,
      };
      this.nativeContext.OnWindowRectChange(this.widgetId, windowBound, 0);
    });
  }

  private needDarkMode(): boolean {
    let context = this.getWindowContext() as common.UIAbilityContext;
    let currentColorMode: ConfigurationConstant.ColorMode | undefined = context.config.colorMode;
    if (!currentColorMode) {
      LogUtil.warn(TAG, 'Can not get current color mode from window: ' + this.widgetId);
    }
    return this.styles?.use_dark_mode || currentColorMode === ConfigurationConstant.ColorMode.COLOR_MODE_DARK;
  }
};

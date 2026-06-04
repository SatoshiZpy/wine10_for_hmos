// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { NativeContext, WindowBound } from '../interface/CommonInterface';
import lazy { DragDropAdapter } from '../adapter/DragDropAdapter';
import LogUtil from '../utils/LogUtil';
import { ConfigData, ContextType, PanAction } from '../common/Constants';
import Inject from '../common/InjectModule';
import JsBindingUtils from '../utils/JsBindingUtils';
import { MultiInputAdapter } from '../adapter/MultiInputAdapter';
import { DragParamManager } from '../common/DragParamManager';
import { window } from '@kit.ArkUI';
import { NativeThemeAdapter } from '../adapter/NativeThemeAdapter'
import fs from '@ohos.file.fs';
import lazy { PresetDeployerAdapter, PresetMapping } from '../adapter/PresetDeployerAdapter';
import CheckEmptyUtils from '../utils/CheckEmptyUtils';

@Component
export struct WineWindow {
  private static TAG: string = 'WineWindow';
  @LocalStorageLink('widgetId') widgetId: number = -1;
  @LocalStorageLink('startUri') startUri: string = '';
  @LocalStorageLink('window') window: window.Window | undefined = undefined;
  @LocalStorageLink('statusBarHeight') statusBarHeight: number = 0;
  @LocalStorageLink('useDarkMode') useDarkMode: boolean = false;
  @LocalStorageLink('ownerPid') ownerPid: number = -1;
  @LocalStorageLink('wineExePath') exePath: string = '';
  @LocalStorageLink('wineArgs') args: string = '';
  private LIBRARY_NAME: string = 'adapter';
  private XCOMPONENT_NAME: string = 'xcomponent';
  private MODULE_NAME: string = this.widgetId + ':' + this.XCOMPONENT_NAME + ':' + this.ownerPid;
  private dragDropAdapter: DragDropAdapter = Inject.get(DragDropAdapter);
  private nativeContext: NativeContext = JsBindingUtils.getNativeContext(ContextType.kMainProcess);
  private multiInputAdapter: MultiInputAdapter = Inject.get(MultiInputAdapter);
  private dragParamManager = Inject.get(DragParamManager);
  private presetDeployer = Inject.get(PresetDeployerAdapter);
  @State private bgColor: Color | string =
    this.useDarkMode ? NativeThemeAdapter.DARK_BACKGROUND_COLOR : NativeThemeAdapter.LIGHT_BACKGROUND_COLOR;

  aboutToAppear(): void {
    this.dragParamManager.addDragContextData({
      id: this.widgetId,
      uiContext: this.getUIContext(),
    });
  }

  aboutToDisappear(): void {
    this.dragParamManager.removeDragContextData(this.widgetId);
  }

  build() {
    XComponent({
      id: this.MODULE_NAME,
      type: XComponentType.SURFACE,
      libraryname: this.LIBRARY_NAME
    })
      .focusable(true)
      .defaultFocus(true)
      .backgroundColor(this.bgColor)
      .gesture(
        PanGesture({ direction: PanDirection.Vertical | PanDirection.Horizontal, distance: 1 })
          .onActionStart((event: GestureEvent) => {
            this.multiInputAdapter.OnPanEvent(PanAction.kStart, this.widgetId, event);
          })
          .onActionUpdate((event: GestureEvent) => {
            this.multiInputAdapter.OnPanEvent(PanAction.kUpdate, this.widgetId, event);
          })
          .onActionEnd((event: GestureEvent) => {
            this.multiInputAdapter.OnPanEvent(PanAction.kEnd, this.widgetId, event);
          })
          .onActionCancel(() => {
            this.multiInputAdapter.OnPanEvent(PanAction.kCancel, this.widgetId, undefined);
          })
      )
      .gesture(
        PinchGesture()
          .onActionStart((event: GestureEvent) => {
            this.multiInputAdapter.OnPinchEvent("start", this.widgetId, event);
          })
          .onActionUpdate((event: GestureEvent) => {
            this.multiInputAdapter.OnPinchEvent("update", this.widgetId, event);
          })
          .onActionEnd((event: GestureEvent) => {
            this.multiInputAdapter.OnPinchEvent("end", this.widgetId, event);
          })
      )
      .gesture(
        GestureGroup(GestureMode.Exclusive,
          TapGesture({ count: 2})
            .onAction((event: GestureEvent) => {
              if (event) {
                let offsetX = vp2px(event.fingerList[0].globalX);
                let offsetY = vp2px(event.fingerList[0].globalY);
                this.nativeContext.OnDoubleTapCB(this.widgetId, offsetX, offsetY);
              }
            })
        )
      )
      .onDragEnter((event?: DragEvent) => {
        LogUtil.info(WineWindow.TAG, 'onDragEnter widgetId:' + this.widgetId);
        this.dragDropAdapter.dragEnterData(this.widgetId, (event as DragEvent).getSummary());
      })
      .onDragMove((event?: DragEvent) => {
        let windowX = event?.getWindowX();
        let windowY = event?.getWindowY();
        windowX = vp2px(windowX);
        windowY = vp2px(windowY);
        this.dragDropAdapter.dragMove(this.widgetId,
                                      windowX,
                                      windowY - this.statusBarHeight);
      })
      .onDragLeave(() => {
        LogUtil.info(WineWindow.TAG, 'onDragLeave widgetId:' + this.widgetId);
        this.dragDropAdapter.dragLeave(this.widgetId);
      })
      .onDrop((event?: DragEvent) => {
        if (!event) {
          LogUtil.error(WineWindow.TAG, 'onDrop event is null');
          return;
        }
        LogUtil.info(WineWindow.TAG, 'onDrop widgetId:' + this.widgetId);
        let dragData: UnifiedData = (event as DragEvent).getData() as UnifiedData;
        this.dragDropAdapter.dropData(this.widgetId, dragData);
      })
      .onLoad(() => {
        LogUtil.info(WineWindow.TAG, 'onLoad xComponent:' + this.MODULE_NAME);
        if (this.widgetId === ConfigData.DEFAULT_WIDGET_ID) {
          this.setDefaultBounds();
        }

        this.loadWine().catch((err: Object) => {
          LogUtil.error(WineWindow.TAG, `loadWine failed: ${JSON.stringify(err)}`);
        });

        setTimeout(() => {
          this.bgColor = Color.White;
        }, 5000);
      })
      .id(this.XCOMPONENT_NAME)
      .renderFit(RenderFit.TOP_LEFT)
  }

  private setDefaultBounds() {
    let prop = this.window?.getWindowProperties();
    if (prop) {
      let windowRect: WindowBound = {
        left: prop.windowRect.left,
        top: prop.windowRect.top,
        width: prop.windowRect.width,
        height: prop.windowRect.height
      }

      let drawableRect: WindowBound = {
        left: prop.drawableRect.left,
        top: prop.drawableRect.top + this.statusBarHeight,
        width: prop.drawableRect.width,
        height: prop.drawableRect.height - this.statusBarHeight
      }

      this.nativeContext.OnWindowInitSize(windowRect, drawableRect);
    }

    let state = this.window?.getWindowStatus();
    if (state) {
      this.nativeContext.OnWindowInitState(state);
    }
  }

  private buildArgs(): string[] {
    let default_uri = '';
    if (this.startUri.length !== 0) {
      default_uri = this.startUri;
    }
    let resDir = '--bundle-installation-dir=' + this.getUIContext().getHostContext()?.resourceDir;
    let vec_args = [resDir, default_uri];
    return vec_args;
  }

  private async loadWine(): Promise<void> {
    const hostContext = this.getUIContext().getHostContext();
    if (!hostContext) {
      LogUtil.error(WineWindow.TAG, 'hostContext is undefined');
      return;
    }

    const bundleDir = hostContext.bundleCodeDir ?? '';
    const resDir = hostContext.resourceDir ?? '';
    const dataDir = hostContext.filesDir ?? '';
    const cacheDir = hostContext.cacheDir ?? '';
    const preferenceDir = hostContext.preferencesDir ?? '';

    if (!bundleDir || !resDir || !dataDir || !cacheDir || !preferenceDir) {
      LogUtil.error(WineWindow.TAG,
        `invalid path, bundleDir=${bundleDir}, resDir=${resDir}, dataDir=${dataDir},
        cacheDir=${cacheDir}, preferenceDir=${preferenceDir}`);
      return;
    }

    const binDir = `${resDir}/bin`;
    const libDir = `${resDir}/lib`;
    const dllDir = `${libDir}/wine`;
    const loader = `${bundleDir}/libs/arm64/libwine.so`;

    const prefixDir = `${dataDir}/prefix`;
    const logPath = `${dataDir}/log.txt`;

    this.ensureDir(prefixDir);
    this.ensureFile(logPath);

    if (this.presetDeployer.hasSapPreset()) {
      await this.deploySapPreset(prefixDir);
    } else {
      LogUtil.info(WineWindow.TAG, 'rawfile/preset not found, skip deploySapPreset');
    }

    const environ = new Map<string, string>();
    this.setEnv(environ, 'WINERESPATH', resDir);

    this.setEnv(environ, 'XDG_DATA_HOME', dataDir);
    this.setEnv(environ, 'XDG_CACHE_HOME', cacheDir);
    this.setEnv(environ, 'XDG_CONFIG_HOME', preferenceDir);

    this.setEnv(environ, 'WINELOADER', loader);
    this.setEnv(environ, 'WINEDLLPATH', dllDir);
    this.setEnv(environ, 'WINEPREFIX', prefixDir);
    let wineDebug = '';
    this.setEnv(environ, 'WINEDEBUG', wineDebug);
    this.setEnv(environ, 'WINEDEBUGLOG', logPath);

    const config = hostContext.resourceManager.getConfigurationSync();
    const locale = config?.locale;
    this.setEnv(environ, 'LC_ALL', locale);
    this.setEnv(environ, 'LANG', locale);

    const env: string[] = [];
    environ.forEach((value, key) => {
      env.push(key, value);
    });

    let exePath = CheckEmptyUtils.checkStrIsEmpty(this.exePath) ?
      'c:\\windows\\system32\\notepad.exe' : this.exePath;
    const cmd: string[] = [
      loader,
      exePath
    ];

    LogUtil.debug(WineWindow.TAG, `
      exePath=${exePath}, env=${env.toString()}, loader=${loader},
      prefixDir=${prefixDir}, logPath=${logPath}`);

    this.nativeContext.runWine(cmd, env);
  }

  private async deploySapPreset(prefixDir: string): Promise<void> {
    const cDriveDir = `${prefixDir}/dosdevices/c:`;
    const system32Dir = `${cDriveDir}/windows/system32`;
    const sapDir = `${cDriveDir}/sap`;
    const userDir = `${cDriveDir}/users/100`;

    const mappings: PresetMapping[] = [
      { rawDir: 'preset/regs', destAbsDir: prefixDir },
      { rawDir: 'preset/sap', destAbsDir: sapDir },
      { rawDir: 'preset/add', destAbsDir: system32Dir },
      { rawDir: 'preset/modify', destAbsDir: system32Dir },
      { rawDir: 'preset/root', destAbsDir: userDir }
    ];

    try {
      await this.presetDeployer.deployIfNeeded(prefixDir, '.preset_sap_deployed_v1', mappings);
    } catch (err) {
      LogUtil.error(WineWindow.TAG, `deploySapPreset failed: ${JSON.stringify(err)}`);
    }
  }

  private setEnv(environ: Map<string, string>, key: string, value?: string): void {
    if (value && value.trim().length > 0) {
      environ.set(key, value);
    }
  }

  private ensureDir(path: string): void {
    try {
      const exists = fs.accessSync(path);
      if (!exists) {
        fs.mkdirSync(path);
      }
    } catch (err) {
      LogUtil.error(WineWindow.TAG, `ensureDir failed: ${path}, err=${JSON.stringify(err)}`);
    }
  }

  private ensureFile(path: string): void {
    try {
      const exists = fs.accessSync(path);
      if (!exists) {
        const file = fs.openSync(path, fs.OpenMode.CREATE | fs.OpenMode.READ_WRITE);
        const randomAccessFile = fs.createRandomAccessFileSync(file);
        randomAccessFile.close();
      }
    } catch (err) {
      LogUtil.error(WineWindow.TAG, `ensureFile failed: ${path}, err=${JSON.stringify(err)}`);
    }
  }
}

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
import { typeNode as TypedNode, NodeController, FrameNode } from '@ohos.arkui.node';
import { DragParamManager } from '../common/DragParamManager';
import { window } from '@kit.ArkUI';
import fs from '@ohos.file.fs';
// resmgr：HarmonyOS 资源管理器，PresetDeployer 通过它访问 rawfile/preset/* 下的预置资源
import resmgr from '@ohos.resourceManager';
import { NativeThemeAdapter } from '../adapter/NativeThemeAdapter';
// PresetDeployer：把 rawfile/preset 下的 SAP GUI 部署包一次性展开到 Wine prefix 的工具
// PresetMapping：单条 rawDir → destAbsDir 的映射定义，详见 PresetDeployer.ets
import { PresetDeployer, PresetMapping } from '../utils/PresetDeployer';

interface ComponentConfig {
  moduleName: string,
  widgetId: number,
  uri: string,
  nativeContext: NativeContext,
  window?: window.Window,
  bgColor?: Color | string
}

const TAG: string = 'WineWindowNode';

class WindowXComponentController extends XComponentController {
  onSurfaceCreated(surfaceId: string): void {
    LogUtil.info(TAG, `onSurfaceCreated surfaceId: ${surfaceId}`);
  }
  onSurfaceChanged(surfaceId: string, rect: SurfaceRect): void {
    LogUtil.info(TAG, `onSurfaceChanged surfaceId: ${surfaceId}, rect: ${JSON.stringify(rect)}`);
  }
  onSurfaceDestroyed(surfaceId: string): void {
    LogUtil.info(TAG, `onSurfaceDestroyed surfaceId: ${surfaceId}`);
  }
}

class WindowNodeController extends NodeController {
  private xcController: XComponentController = new WindowXComponentController();
  private xComponent: TypedNode.XComponent | undefined = undefined;
  private config: ComponentConfig;

  constructor(config: ComponentConfig) {
    super();
    this.config = config;
  }

  makeNode(uiContext: UIContext): FrameNode | null {
    let node = new FrameNode(uiContext);
    let column = TypedNode.createNode(uiContext, 'Column')
    column.initialize()
      .width('100%')
      .height('100%')
    try {
      node.appendChild(column);
    } catch (err) {
      LogUtil.error(TAG, `Fail to append child: err=${JSON.stringify(err)}`);
    }

    this.xComponent = TypedNode.createNode(uiContext, "XComponent", {
      type: XComponentType.SURFACE,
      controller: this.xcController
    })
    this.xComponent.initialize(
      {
        id: this.config.moduleName,
        type: XComponentType.SURFACE,
        controller: this.xcController,
        libraryname: "adapter"
      });
    this.xComponent.attribute
      .defaultFocus(true)
      .focusable(true)
      .focusOnTouch(true)
      .backgroundColor(this.config.bgColor)
      .renderFit(RenderFit.TOP_LEFT)
      .onLoad(() => {
        LogUtil.info(TAG, 'onLoad xComponent:' + this.config.moduleName);
        if (this.config.widgetId === ConfigData.DEFAULT_WIDGET_ID) {
          this.setDefaultBounds();
        }

        // loadWine 已改为 async（内部要先做 SAP 预置部署再拉起 Wine）。
        // onLoad 本身不是 async，不能直接 await，所以用 .catch 兜住任何被
        // bubble 出来的异常，避免 unhandled promise rejection；正常路径上的
        // 子步骤错误已经在 loadWine 内部各自的 try/catch 里就地降级处理了。
        this.loadWine().catch((err: Object) => {
          LogUtil.error(TAG, `loadWine failed: ${JSON.stringify(err)}`);
        });

        setTimeout(() => {
          this.config.bgColor = Color.White;
        }, 5000);
      })

    try {
      column.appendChild(this.xComponent);
    } catch (err) {
      LogUtil.error(TAG, `Fail to append child: err=${JSON.stringify(err)}`);
    }
    return node;
  }

  aboutToResize(size: Size) {
    LogUtil.info(TAG, 'aboutToResize xComponent:' + this.config.moduleName + ',size:' + JSON.stringify(size));
  }

  onAttach(): void {
    LogUtil.info(TAG, 'onAttach xComponent:' + this.config.moduleName);
    this.rebuild();
  }

  onDetach(): void {
    LogUtil.info(TAG, 'onDetach xComponent:' + this.config.moduleName);
  }

  private setDefaultBounds() {
    let prop = this.config.window?.getWindowProperties();
    if (prop) {
      let windowRect: WindowBound = {
        left: prop.windowRect.left,
        top: prop.windowRect.top,
        width: prop.windowRect.width,
        height: prop.windowRect.height
      }

      let drawableRect: WindowBound = {
        left: prop.drawableRect.left,
        top: prop.drawableRect.top,
        width: prop.drawableRect.width,
        height: prop.drawableRect.height
      }

      this.config.nativeContext.OnWindowInitSize(windowRect, drawableRect);
    }
  }

  private buildArgs(): string[] {
    let default_uri = '';
    if (this.config.uri.length !== 0) {
      default_uri = this.config.uri;
    }
    let resDir = '--bundle-installation-dir=' + getContext().resourceDir;
    let vec_args = [resDir, default_uri];
    return vec_args;
  }

  private async loadWine(): Promise<void> {
    const hostContext = this.config.window?.getUIContext().getHostContext();
    if (!hostContext) {
      LogUtil.error(TAG, 'hostContext is undefined');
      return;
    }

    const bundleDir = hostContext.bundleCodeDir ?? '';
    const resDir = hostContext.resourceDir ?? '';
    const dataDir = hostContext.filesDir ?? '';
    const cacheDir = hostContext.cacheDir ?? '';
    const preferencedir = hostContext.preferencesDir ?? '';

    if (!bundleDir || !resDir || !dataDir || !cacheDir || !preferencedir) {
      LogUtil.error(TAG,
        `invalid path, bundleDir=${bundleDir}, resDir=${resDir}, dataDir=${dataDir}, cacheDir=${cacheDir}, preferencedir=${preferencedir}`
      );
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

    // 只有打包了 rawfile/preset 的 SAP 工程才进入 SAP 预置部署；复用 Wine 底座的普通工程完全跳过这条链路。
    if (PresetDeployer.hasSapPreset(hostContext.resourceManager)) {
      await this.deploySapPreset(prefixDir, hostContext.resourceManager);
    } else {
      LogUtil.info(TAG, 'rawfile/preset not found, skip deploySapPreset');
    }

    const environ = new Map<string, string>();
    this.setEnv(environ, 'WINERESPATH', resDir);

    this.setEnv(environ, 'XDG_DATA_HOME', dataDir);
    this.setEnv(environ, 'XDG_CACHE_HOME', cacheDir);
    this.setEnv(environ, 'XDG_CONFIG_HOME', preferencedir);

    this.setEnv(environ, 'WINELOADER', loader);
    this.setEnv(environ, 'WINEDLLPATH', dllDir);
    this.setEnv(environ, 'WINEPREFIX', prefixDir);

    let wineDebug = '+err,+warn';
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

    const cmd: string[] = [
      loader,
      'c:\\sap\\SAPGUI\\saplogon.exe'
    ];

    LogUtil.info(TAG, `loader=${loader}`);
    LogUtil.info(TAG, `prefixDir=${prefixDir}`);
    LogUtil.info(TAG, `logPath=${logPath}`);

    this.config.nativeContext.runWine(cmd, env);
  }

  /**
   * 把 rawfile/preset 下的 SAP GUI 部署包展开到 Wine prefix。
   *
   * 我们的 Wine prefix 没有 drive_c 实体目录，C 盘指向 ${prefixDir}/dosdevices/c:，
   * 所有 Windows 路径（C:\sap\...、C:\windows\system32\...、C:\users\100\...）都
   * 会落到这条 dosdevices 软链下。注册表（system.reg、user.reg）则直接放 prefix 根。
   *
   * 五条映射对应 sap 部署包工程说明里的 §5：
   *   1) regs   → ${prefix}/                                       覆盖 prefix 的注册表
   *   2) sap    → ${prefix}/dosdevices/c:/sap                      SAP GUI 主目录
   *   3) add    → ${prefix}/dosdevices/c:/windows/system32         新增的兼容运行时 DLL
   *   4) modify → ${prefix}/dosdevices/c:/windows/system32         覆盖性的运行时 DLL
   *   5) root   → ${prefix}/dosdevices/c:/users/100                用户 AppData，目录名用 100
   *      与 user.reg 中 USERNAME=100、HOMEPATH=\\users\\100 保持一致
   *
   * 每条 mapping 的展开语义是"把 rawDir 里的孩子合并进 destAbsDir"，而不是在
   * destAbsDir 下再嵌套一层 rawDir 名。例如 preset/sap 里只有一个 SAPGUI/，
   * 合并后就形成 c:/sap/SAPGUI/saplogon.exe，正好对得上启动命令。
   *
   * 用 .preset_sap_deployed_v1 作为部署完成标记。后续如需重新预置（比如换了
   * 部署包），把这个标记文件删掉即可触发再次展开；标记名带版本号是为了将来
   * 改部署内容时可以靠改标记名强制重新部署，而不需要让用户手动清 prefix。
   */
  private async deploySapPreset(prefixDir: string, resourceManager: resmgr.ResourceManager): Promise<void> {
    // C 盘根：HarmonyOS 文件系统下的实体目录，Wine 通过 dosdevices 软链当作 C:\
    const cDriveDir = `${prefixDir}/dosdevices/c:`;
    const system32Dir = `${cDriveDir}/windows/system32`;
    const sapDir = `${cDriveDir}/sap`;
    // 用户家目录命名为 100（数字），与注册表里 USERNAME=100 对齐
    const userDir = `${cDriveDir}/users/100`;

    const mappings: PresetMapping[] = [
      { rawDir: 'preset/regs', destAbsDir: prefixDir },
      { rawDir: 'preset/sap', destAbsDir: sapDir },
      { rawDir: 'preset/add', destAbsDir: system32Dir },
      // modify 放在 add 之后，让覆盖性 DLL 覆盖掉 add 中的同名文件（实际两边没重名，按工程规约保序更安全）
      { rawDir: 'preset/modify', destAbsDir: system32Dir },
      { rawDir: 'preset/root', destAbsDir: userDir }
    ];

    try {
      // 标记文件的检查/写入由 PresetDeployer 内部完成，此处不需要重复判断
      await PresetDeployer.deployIfNeeded(resourceManager, prefixDir, '.preset_sap_deployed_v1', mappings);
    } catch (err) {
      // 单条 mapping 拷贝失败时 PresetDeployer 已经在 copyRawFile 里就地记日志降级，
      // 这里捕获的是 deployIfNeeded 整个流程异常（如 prefixDir 不可写等极端情况）。
      // 不向上抛：让 Wine 仍尝试启动，让 Wine 日志报具体哪个文件缺失，便于排障。
      LogUtil.error(TAG, `deploySapPreset failed: ${JSON.stringify(err)}`);
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
      LogUtil.error(TAG, `ensureDir failed: ${path}, err=${JSON.stringify(err)}`);
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
      LogUtil.error(TAG, `ensureFile failed: ${path}, err=${JSON.stringify(err)}`);
    }
  }
}

@Component
export struct WineWindowNode {
  @LocalStorageLink('widgetId') widgetId: number = -1;
  @LocalStorageLink('startUri') startUri: string = '';
  @LocalStorageLink('window') window: window.Window | undefined = undefined;
  @LocalStorageLink('useDarkMode') useDarkMode: boolean = false;
  @LocalStorageLink('ownerPid') ownerPid: number = -1;
  private XCOMPONENT_NAME: string = 'xcomponent';
  private MODULE_NAME: string = this.widgetId + ':' + this.XCOMPONENT_NAME + ':' + this.ownerPid;
  private dragDropAdapter: DragDropAdapter = Inject.get(DragDropAdapter);
  private nativeContext: NativeContext = JsBindingUtils.getNativeContext(ContextType.kMainProcess);
  private multiInputAdapter: MultiInputAdapter = Inject.get(MultiInputAdapter);
  private dragParamManager = Inject.get(DragParamManager);
  @State private bgColor: Color | string =
    this.useDarkMode ? NativeThemeAdapter.DARK_BACKGROUND_COLOR : NativeThemeAdapter.LIGHT_BACKGROUND_COLOR;

  private windowNodeController: WindowNodeController =
    new WindowNodeController({
      moduleName: this.MODULE_NAME,
      widgetId: this.widgetId,
      uri: this.startUri,
      nativeContext: this.nativeContext,
      window: this.window,
      bgColor: this.bgColor
    });

  aboutToAppear() {
    this.dragParamManager.addDragContextData({
      id: this.widgetId,
      uiContext: this.getUIContext(),
    });
    LogUtil.info(TAG, 'aboutToAppear widgetId' + this.widgetId);
  }

  aboutToDisappear(): void {
    this.dragParamManager.removeDragContextData(this.widgetId);
    LogUtil.info(TAG, 'aboutToDisappear widgetId' + this.widgetId);
  }

  build() {
    NodeContainer(this.windowNodeController)
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
      .onDragEnter((event?: DragEvent) => {
        this.dragDropAdapter.dragEnterData(this.widgetId, (event as DragEvent).getSummary());
      })
      .onDragMove((event?: DragEvent) => {
        let windowX = event?.getWindowX();
        let windowY = event?.getWindowY();
        windowX = vp2px(windowX);
        windowY = vp2px(windowY);
        this.dragDropAdapter.dragMove(this.widgetId, windowX, windowY);
      })
      .onDragLeave(() => {
        this.dragDropAdapter.dragLeave(this.widgetId);
      })
      .onDrop((event?: DragEvent) => {
        if (!event) {
          LogUtil.error(TAG, 'OhosDrag WineWindow DragDrop onDrop event is null');
          return;
        }
        let dragData: UnifiedData = (event as DragEvent).getData() as UnifiedData;
        this.dragDropAdapter.dropData(this.widgetId, dragData);
      })
      .id(this.MODULE_NAME)
  }
}

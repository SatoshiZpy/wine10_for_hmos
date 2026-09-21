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
import { NativeThemeAdapter } from '../adapter/NativeThemeAdapter';

interface ComponentConfig {
  moduleName: string,
  widgetId: number,
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
      let windowRect: window.Rect = {
        left: prop.windowRect.left,
        top: prop.windowRect.top,
        width: prop.windowRect.width,
        height: prop.windowRect.height
      }

      let drawableRect: window.Rect = {
        left: prop.drawableRect.left,
        top: prop.drawableRect.top,
        width: prop.drawableRect.width,
        height: prop.drawableRect.height
      }
      LogUtil.info(TAG, 'windowRect:' + JSON.stringify(windowRect) +
        ',drawableRect:' + JSON.stringify(drawableRect));
      this.config.nativeContext.OnWindowInitSize(windowRect, drawableRect);
    }
  }
}

@Component
export struct WineWindowNode {
  @LocalStorageLink('widgetId') widgetId: number = -1;
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

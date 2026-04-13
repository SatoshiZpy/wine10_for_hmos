// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { NativeContext, WindowBound } from '../interface/CommonInterface';
import lazy { DragDropAdapter } from '../adapter/DragDropAdapter';
import LogUtil from '../utils/LogUtil';
import { CommandType, ConfigData, ContextType, PanAction } from '../common/Constants';
import Inject from '../common/InjectModule';
import JsBindingUtils from '../utils/JsBindingUtils';
import { MultiInputAdapter } from '../adapter/MultiInputAdapter';
import { DragParamManager } from '../common/DragParamManager';
import { UIExtensionContentSession } from '@kit.AbilityKit';

@Component
export struct WineEmbeddedWindow {
  private static TAG: string = 'WineEmbeddedWindow';
  @LocalStorageLink('widgetId') widgetId: number = -1;
  @LocalStorageLink('startUri') startUri: string = '';
  @LocalStorageLink('session') session: UIExtensionContentSession | undefined = undefined;
  @LocalStorageLink('userData') userData: string = '';
  @LocalStorageLink('ownerPid') ownerPid: number = -1;
  private LIBRARY_NAME: string = 'adapter';
  private XCOMPONENT_NAME: string = 'xcomponent';
  private MODULE_NAME: string = this.widgetId + ':' + this.XCOMPONENT_NAME + ':' + this.ownerPid;
  private dragDropAdapter: DragDropAdapter = Inject.get(DragDropAdapter);
  private nativeContext: NativeContext = JsBindingUtils.getNativeContext(ContextType.kMainProcess);
  private multiInputAdapter: MultiInputAdapter = Inject.get(MultiInputAdapter);
  private dragParamManager = Inject.get(DragParamManager);

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
      .backgroundColor(Color.White)
      .gesture(
        PanGesture({ direction: PanDirection.Vertical | PanDirection.Horizontal })
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
          LogUtil.error(WineEmbeddedWindow.TAG, 'OhosDrag WineWindow DragDrop onDrop event is null');
          return;
        }
        let dragData: UnifiedData = (event as DragEvent).getData() as UnifiedData;
        this.dragDropAdapter.dropData(this.widgetId, dragData);
      })
      .onLoad(() => {
        LogUtil.info(WineEmbeddedWindow.TAG, 'onLoad xComponent:' + this.MODULE_NAME);
        if (this.widgetId === ConfigData.DEFAULT_WIDGET_ID) {
          this.setDefaultBounds();
          let vec_args = this.buildArgs();
          this.nativeContext.runWine(vec_args, undefined);
        } else {
          this.nativeContext.ExecuteCommand(
            CommandType.kNewWidget,
            {
              url: this.startUri,
              user_data: this.userData,
            });
        }
      })
      .id(this.XCOMPONENT_NAME)
      .renderFit(RenderFit.TOP_LEFT)
  }

  private setDefaultBounds() {
    let windowProxy = this.session?.getUIExtensionWindowProxy();
    let prop = windowProxy?.properties;
    if (prop) {
      let windowRect: WindowBound = {
        left: prop.uiExtensionHostWindowProxyRect.left,
        top: prop.uiExtensionHostWindowProxyRect.top,
        width: prop.uiExtensionHostWindowProxyRect.width,
        height: prop.uiExtensionHostWindowProxyRect.height
      }

      let drawableRect: WindowBound = {
        left: 0,
        top: 0,
        width: prop.uiExtensionHostWindowProxyRect.width,
        height: prop.uiExtensionHostWindowProxyRect.height
      }

      this.nativeContext.OnWindowInitSize(windowRect, drawableRect);
    }
  }

  private buildArgs(): string[] {
    let default_uri = '';
    if (this.startUri.length !== 0) {
      default_uri = this.startUri;
    }
    let resDir = '--bundle-installation-dir=' + getContext().resourceDir;
    let vec_args = [resDir, default_uri];
    return vec_args;
  }
}

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
import { JSON } from '@kit.ArkTS';

@Component
export struct WineWindow {
  private static TAG: string = 'WineWindow';
  @LocalStorageLink('widgetId') widgetId: number = -1;
  @LocalStorageLink('window') window: window.Window | undefined = undefined;
  @LocalStorageLink('statusBarHeight') statusBarHeight: number = 0;
  @LocalStorageLink('useDarkMode') useDarkMode: boolean = false;
  @LocalStorageLink('ownerPid') ownerPid: number = -1;
  private LIBRARY_NAME: string = 'adapter';
  private XCOMPONENT_NAME: string = 'xcomponent';
  private MODULE_NAME: string = this.widgetId + ':' + this.XCOMPONENT_NAME + ':' + this.ownerPid;
  private dragDropAdapter: DragDropAdapter = Inject.get(DragDropAdapter);
  private nativeContext: NativeContext = JsBindingUtils.getNativeContext(ContextType.kMainProcess);
  private multiInputAdapter: MultiInputAdapter = Inject.get(MultiInputAdapter);
  private dragParamManager = Inject.get(DragParamManager);
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
      let windowRect: window.Rect = {
        left: prop.windowRect.left,
        top: prop.windowRect.top,
        width: prop.windowRect.width,
        height: prop.windowRect.height
      }

      let drawableRect: window.Rect = {
        left: prop.drawableRect.left,
        top: prop.drawableRect.top + this.statusBarHeight,
        width: prop.drawableRect.width,
        height: prop.drawableRect.height - this.statusBarHeight
      }
      LogUtil.info(WineWindow.TAG, 'windowRect:' + JSON.stringify(windowRect) +
        ',drawableRect:' + JSON.stringify(drawableRect));
      this.nativeContext.OnWindowInitSize(windowRect, drawableRect);
    }

    let state = this.window?.getWindowStatus();
    if (state) {
      this.nativeContext.OnWindowInitState(state);
    }
  }
}

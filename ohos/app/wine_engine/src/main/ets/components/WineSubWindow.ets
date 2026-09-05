// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { IParams } from '../interface/CommonInterface';
import { PanAction } from '../common/Constants';
import { DragDropAdapter } from '../adapter/DragDropAdapter';
import Inject from '../common/InjectModule';
import LogUtil from '../utils/LogUtil';
import { MultiInputAdapter } from '../adapter/MultiInputAdapter';
import { DragParamManager } from '../common/DragParamManager';
import { NativeThemeAdapter } from '../adapter/NativeThemeAdapter';

@Component
export struct WineSubWindow {
  private static TAG: string = 'WineSubWindow';
  @LocalStorageLink('subWindowParams') params: IParams = {
    id: -1,
    size: [0, 0], // [width, height]
    initColorRgb: 0xffffff,
    ownerPid: -1,
  };
  private widgetId: number = this.params.id;
  private ownerPid: number = this.params.ownerPid;
  private LIBRARY_NAME: string = 'adapter';
  private XCOMPONENT_NAME: string = 'subXcomponent';
  private MODULE_NAME: string = this.widgetId + ':' + this.XCOMPONENT_NAME + ':' + this.ownerPid;
  private dragDropAdapter: DragDropAdapter = Inject.get(DragDropAdapter);
  private multiInputAdapter: MultiInputAdapter = Inject.get(MultiInputAdapter);
  private dragParamManager = Inject.get(DragParamManager);
  @State private bgColor: Color | string = NativeThemeAdapter.LIGHT_BACKGROUND_COLOR;

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
      .backgroundColor(this.bgColor)
      .focusable(true)
      .defaultFocus(true)
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
      .onDragEnter((event?: DragEvent) => {
        LogUtil.info(WineSubWindow.TAG, 'onDragEnter widgetId:' + this.widgetId);
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
        LogUtil.info(WineSubWindow.TAG, 'onDragLeave xComponentId:' + this.widgetId);
        this.dragDropAdapter.dragLeave(this.widgetId);
      })
      .onDrop((event?: DragEvent) => {
        if (!event) {
          LogUtil.error(WineSubWindow.TAG, 'OhosDrag WineSubWindow DragDrop onDrop event is null');
          return;
        }
        LogUtil.info(WineSubWindow.TAG, 'onDrop widgetId:' + this.widgetId);
        let dragData: UnifiedData = (event as DragEvent).getData() as UnifiedData;
        this.dragDropAdapter.dropData(this.widgetId, dragData);
      })
      .onLoad(() => {
        LogUtil.info(WineSubWindow.TAG, 'onLoad xComponent:' + this.MODULE_NAME);
      })
      .id(this.XCOMPONENT_NAME)
  }
}

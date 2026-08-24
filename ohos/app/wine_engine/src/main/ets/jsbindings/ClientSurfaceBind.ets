// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import JsBindingUtils from '../utils/JsBindingUtils';
import Inject from '../common/InjectModule';
import lazy { ClientSurfaceManager } from '../adapter/ClientSurfaceManager';

function implClientSurfaceManager(): ClientSurfaceManager {
  return Inject.getOrCreate(ClientSurfaceManager);
}

function createClientSurface(parentWidgetId: number, clientWidgetId: number,
  offX: number, offY: number, width: number, height: number,
  callback: (success: boolean) => void) {
  implClientSurfaceManager().create(parentWidgetId, clientWidgetId,
    offX, offY, width, height, callback);
}

function setClientSurfaceBounds(clientWidgetId: number,
  offX: number, offY: number, width: number, height: number) {
  implClientSurfaceManager().setBounds(clientWidgetId, offX, offY, width, height);
}

function destroyClientSurface(clientWidgetId: number) {
  implClientSurfaceManager().destroy(clientWidgetId);
}

export class ClientSurfaceBind {
  static bind() {
    JsBindingUtils.bindFunction("ClientSurface.Create", createClientSurface);
    JsBindingUtils.bindFunction("ClientSurface.SetBounds", setClientSurfaceBounds);
    JsBindingUtils.bindFunction("ClientSurface.Destroy", destroyClientSurface);
  }
}

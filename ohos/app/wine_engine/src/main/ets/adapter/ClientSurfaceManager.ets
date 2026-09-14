// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { injectable } from 'inversify';
import LogUtil from '../utils/LogUtil';

// Geometry of the vulkan client surface XComponent, in pixels of the hosting
// window: the GDI buffer maps window pixels 1:1 onto the page (top-left
// anchored), so the surface stays aligned with the GDI content whatever
// geometry the window manager assigns to the window.
export interface ClientSurfaceConfig {
  clientWidgetId: number;
  offX: number;
  offY: number;
  width: number;
  height: number;
}

const TAG: string = 'ClientSurface';

@injectable()
export class ClientSurfaceManager {
  private renderers: Map<number, (config: ClientSurfaceConfig | undefined) => void> =
    new Map<number, (config: ClientSurfaceConfig | undefined) => void>();
  private ownerOf: Map<number, number> = new Map<number, number>();

  public register(parentWidgetId: number,
    setter: (config: ClientSurfaceConfig | undefined) => void) {
    this.renderers.set(parentWidgetId, setter);
  }

  public unregister(parentWidgetId: number) {
    this.renderers.delete(parentWidgetId);
    const owned: number[] = [];
    this.ownerOf.forEach((owner: number, clientWidgetId: number) => {
      if (owner === parentWidgetId) {
        owned.push(clientWidgetId);
      }
    });
    owned.forEach((clientWidgetId: number) => {
      this.ownerOf.delete(clientWidgetId);
    });
  }

  public create(parentWidgetId: number, clientWidgetId: number,
    offX: number, offY: number, width: number, height: number,
    callback: (success: boolean) => void) {
    const setter = this.renderers.get(parentWidgetId);
    if (!setter) {
      LogUtil.error(TAG, `create failed: renderer of window ${parentWidgetId} not found`);
      callback(false);
      return;
    }
    this.ownerOf.set(clientWidgetId, parentWidgetId);
    setter(this.buildConfig(clientWidgetId, offX, offY, width, height));
    LogUtil.info(TAG, `create client surface ${clientWidgetId} on window ${parentWidgetId}, ` +
      `bounds: (${offX}, ${offY}, ${width}, ${height})`);
    callback(true);
  }

  public setBounds(clientWidgetId: number,
    offX: number, offY: number, width: number, height: number) {
    const parentWidgetId = this.ownerOf.get(clientWidgetId);
    if (parentWidgetId === undefined) {
      LogUtil.warn(TAG, `setBounds: client surface ${clientWidgetId} not found`);
      return;
    }
    this.renderers.get(parentWidgetId)?.(this.buildConfig(clientWidgetId, offX, offY, width, height));
    LogUtil.info(TAG, `setBounds client surface ${clientWidgetId}, ` +
      `bounds: (${offX}, ${offY}, ${width}, ${height})`);
  }

  private buildConfig(clientWidgetId: number,
    offX: number, offY: number, width: number, height: number): ClientSurfaceConfig {
    const config: ClientSurfaceConfig = {
      clientWidgetId: clientWidgetId,
      offX: offX,
      offY: offY,
      width: width,
      height: height
    };
    return config;
  }

  public destroy(clientWidgetId: number) {
    const parentWidgetId = this.ownerOf.get(clientWidgetId);
    if (parentWidgetId === undefined) {
      LogUtil.warn(TAG, `destroy: client surface ${clientWidgetId} not found`);
      return;
    }
    this.ownerOf.delete(clientWidgetId);
    this.renderers.get(parentWidgetId)?.(undefined);
    LogUtil.info(TAG, `destroy client surface ${clientWidgetId} of window ${parentWidgetId}`);
  }
}

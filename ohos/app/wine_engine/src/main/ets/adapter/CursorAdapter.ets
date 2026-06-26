// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { inject, injectable } from 'inversify';
import { AbilityManager } from '../common/AbilityManager';
import { BaseAdapter } from '../common/BaseAdapter';
import LogMethod from '../common/LogDecorator';
import pointer from '@ohos.multimodalInput.pointer';
import display from '@ohos.display';
import { image } from '@kit.ImageKit';
import LogUtil from '../utils/LogUtil';
import { BusinessError } from '@kit.BasicServicesKit';
import { deviceInfo } from '@kit.BasicServicesKit';
import { SystemFloatingWindowManager } from '../common/SystemFloatingWindowManager';

const TAG: string = 'Cursor';

@injectable()
export class CursorAdapter extends BaseAdapter {
  private abilityManager: AbilityManager;
  private systemFloatingWindowManager : SystemFloatingWindowManager;
  // Cached system DPI scale, lazily read on first custom-cursor call.
  // -1 sentinel means "not initialized yet"; fall back to 1.0 on read failure.
  private cachedCursorScale: number = -1;
  // Cap the projected cursor side to keep the system cursor reasonable
  // on extreme HiDPI configs (avoids "huge cursor" when SAP supplies a
  // ~64px source on a density-3 screen).
  private static readonly CURSOR_MAX_SIDE: number = 96;

  constructor(
    @inject(AbilityManager) abilityManager: AbilityManager,
    @inject(SystemFloatingWindowManager) systemFloatingWindowManager: SystemFloatingWindowManager
  ) {
    super();
    this.abilityManager = abilityManager;
    this.systemFloatingWindowManager = systemFloatingWindowManager;
  }

  private getWindow(id: number) {
    let window = this.abilityManager.getProxy(id)?.getWindow();
    if (!window) {
      window = this.systemFloatingWindowManager.getFloatWindow(id);
    }
    return window;
  }

  /**
   * Compute the per-axis scale to apply to the source ARGB cursor bits
   * before forwarding to the multimodal pointer service.
   *
   * SAP/Wine cursors arrive at the original 11x17 / 16x16 hotspot size
   * authored for 96 DPI displays. On a HiDPI panel (densityPixels > 1)
   * the system cursor service renders them as a few physical pixels,
   * which is what users see as the "tiny down-arrow" issue. We scale
   * the pixelMap up by densityPixels so the cursor matches the visual
   * size of HarmonyOS system pointers.
   */
  private getCursorScale(srcMaxSide: number): number {
    if (this.cachedCursorScale < 0) {
      try {
        let displayClass: display.Display = display.getDefaultDisplaySync();
        let density: number = displayClass.densityPixels;
        // densityPixels==1 on 96 DPI baseline; <1.05 → no perceptible gain.
        this.cachedCursorScale = density > 1.05 ? density : 1.0;
      } catch (error) {
        LogUtil.error(TAG, "Failed to read densityPixels, fallback scale=1.0. Cause: " + JSON.stringify(error));
        this.cachedCursorScale = 1.0;
      }
    }
    let scale: number = this.cachedCursorScale;
    if (srcMaxSide > 0 && srcMaxSide * scale > CursorAdapter.CURSOR_MAX_SIDE) {
      scale = CursorAdapter.CURSOR_MAX_SIDE / srcMaxSide;
    }
    return scale < 1.0 ? 1.0 : scale;
  }

  private applyCustomCursor(id: number, cursorImage: image.PixelMap,
                            hotspotX: number, hotspotY: number) {
    let window = this.getWindow(id);
    if (!window) {
      return;
    }
    let windowId = window.getWindowProperties().id;
    if (deviceInfo.sdkApiVersion >= 15) {
      let cuisor: pointer.CustomCursor = {
        pixelMap: cursorImage,
        focusX: hotspotX,
        focusY: hotspotY
      }
      pointer.setCustomCursor(windowId, cuisor, { followSystem: false })
        .catch((error: BusinessError) => {
          LogUtil.error(TAG, "Failed to set setCustomCursor. Cause: " + JSON.stringify(error));
        });
    } else {
      pointer.setCustomCursorSync(windowId, cursorImage, hotspotX, hotspotY);
    }
  }

  @LogMethod
  setCursor(id: number, type: number) {
    let window = this.getWindow(id);
    if (window) {
      let windowId = window.getWindowProperties().id;
      pointer.setPointerStyle(windowId, type)
        .catch((error: BusinessError) => {
          LogUtil.error(TAG, "Failed to set cursor. Cause: " + JSON.stringify(error));
        });
    }
  }

  @LogMethod
  setCursorVisible(visible: boolean) {
    pointer.setPointerVisible(visible)
      .catch((error: BusinessError) => {
        LogUtil.error(TAG, "Failed to set cursor visible. Cause: " + JSON.stringify(error));
      });
  }

  @LogMethod
  setCustomCursor(id: number,
    width: number,
    height: number,
    hotspotX: number,
    hotspotY: number,
    buff: ArrayBuffer) {
    // editable=true is required so we can call pixelMap.scale() below.
    let opts: image.InitializationOptions = {
      editable: true,
      pixelFormat: image.PixelMapFormat.BGRA_8888,
      size: {
        height: height,
        width: width
      }
    };
    let srcMaxSide: number = width > height ? width : height;
    let scale: number = this.getCursorScale(srcMaxSide);
    image.createPixelMap(buff, opts)
      .then((cursorImage: image.PixelMap) => {
        if (scale > 1.0) {
          cursorImage.scale(scale, scale)
            .then(() => {
              this.applyCustomCursor(id, cursorImage,
                Math.round(hotspotX * scale), Math.round(hotspotY * scale));
            })
            .catch((error: BusinessError) => {
              LogUtil.error(TAG, "Failed to scale cursor pixelMap, using source size. Cause: " + JSON.stringify(error));
              this.applyCustomCursor(id, cursorImage, hotspotX, hotspotY);
            });
        } else {
          this.applyCustomCursor(id, cursorImage, hotspotX, hotspotY);
        }
      });
  }
}

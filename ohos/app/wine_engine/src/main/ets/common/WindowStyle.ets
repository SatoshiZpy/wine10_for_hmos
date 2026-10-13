// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { injectable } from 'inversify';
import { WindowFeatures, WindowState, WindowStyles } from '../interface/CommonInterface';
import LogMethod from './LogDecorator';

@injectable()
export class WindowStyle {
  private use_system_frame: boolean;

  constructor() {
    // Temporarily remove the system-provided window decorations
    // (such as title bar, borders, and caption buttons).
    this.use_system_frame = false;
  }

  public decimalColorToHex(decimalColor: number): string {
    let hexColor = decimalColor.toString(16).padStart(8, '0');
    return hexColor;
  }

  public hexColorToDecimal(hexColor: string): number {
    let decimalColor = parseInt(hexColor, 16);
    return decimalColor;
  }

  public getColorAlpha(decimalColor: number): number {
    return (decimalColor >>> 24) / 255;
  }

  public decimalOpacityToHex(decimalOpacity: number): string {
    let hexOpacity = Math.round(decimalOpacity * 255).toString(16).padStart(2, '0');
    return hexOpacity;
  }

  public hasValue(val: number, mask: number): boolean {
    return ((1 << val) & mask) !== 0;
  }

  @LogMethod
  public buildWindowStyles(feature_mask: number, window_state_mask: number): WindowStyles {
    let styles: WindowStyles = new WindowStyles();
    let has_title_bar = this.hasValue(WindowFeatures.WINDOW_TITLE_BAR, feature_mask);
    styles.hide_title_bar = !has_title_bar;
    styles.closable = has_title_bar && this.hasValue(WindowFeatures.WINDOW_CLOSE_BUTTON, feature_mask);
    styles.minimizable = has_title_bar && this.hasValue(WindowFeatures.WINDOW_MINIMIZE_BUTTON, feature_mask);
    styles.maximizable = has_title_bar && this.hasValue(WindowFeatures.WINDOW_MAXIMIZE_BUTTON, feature_mask);
    styles.use_floating_window = this.hasValue(WindowFeatures.USE_FLOATING_WINDOW, feature_mask);
    styles.use_dark_mode = this.hasValue(WindowFeatures.USE_DARK_MODE, feature_mask);
    styles.always_on_top = this.hasValue(WindowState.WINDOW_TOP_MOST, window_state_mask);

    let is_enable = !this.hasValue(WindowState.WINDOW_DISABLED, window_state_mask);
    styles.resizable = is_enable && this.hasValue(WindowFeatures.WINDOW_RESIZABLE, feature_mask);
    styles.focusable = is_enable && !this.hasValue(WindowFeatures.WINDOW_NO_ACTIVATION, feature_mask);
    styles.touchable = is_enable && this.hasValue(WindowFeatures.WINDOW_TOUCHABLE, feature_mask);
    styles.has_shadow = this.hasValue(WindowFeatures.WINDOW_HAS_SHADOW, feature_mask);
    styles.is_modal = this.hasValue(WindowFeatures.WINDOW_MODAL, feature_mask);
    styles.show_window = this.hasValue(WindowState.WINDOW_VISIBLE, window_state_mask);
    return styles;
  }
};

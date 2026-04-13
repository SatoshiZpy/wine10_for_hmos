// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { injectable } from 'inversify';
import { WindowFeatures, WindowState, WindowStyles } from '../interface/CommonInterface';
import LogMethod from './LogDecorator';

@injectable()
export class WindowStyle {
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
    return ((1 << val) & mask) === val;
  }

  @LogMethod
  public buildWindowStyles(features: number, window_state: number): WindowStyles {
    let styles: WindowStyles = new WindowStyles();
    let has_title_bar = this.hasValue(WindowFeatures.WINDOW_TITLE_BAR, features);
    styles.hide_title_bar = !has_title_bar;
    styles.closable = has_title_bar && this.hasValue(WindowFeatures.WINDOW_CLOSE_BUTTON, features);
    styles.minimizable = has_title_bar && this.hasValue(WindowFeatures.WINDOW_MINIMIZE_BUTTON, features);
    styles.maximizable = has_title_bar && this.hasValue(WindowFeatures.WINDOW_MAXIMIZE_BUTTON, features);
    styles.use_floating_window = this.hasValue(WindowFeatures.USE_FLOATING_WINDOW, features);
    styles.use_dark_mode = this.hasValue(WindowFeatures.USE_DARK_MODE, features);
    styles.always_on_top = this.hasValue(WindowState.WINDOW_TOP_MOST, window_state);

    let is_enable = !this.hasValue(WindowState.WINDOW_DISABLED, window_state);
    styles.resizable = is_enable && this.hasValue(WindowFeatures.WINDOW_RESIZABLE, features);
    styles.focusable = is_enable && !this.hasValue(WindowFeatures.WINDOW_NO_ACTIVATION, features);
    styles.touchable = is_enable && this.hasValue(WindowFeatures.WINDOW_TOUCHABLE, features);
    styles.show_widow = this.hasValue(WindowState.WINDOW_VISIABLE, window_state);
    return styles;
  }
};

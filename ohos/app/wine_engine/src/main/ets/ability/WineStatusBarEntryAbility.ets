// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import LogUtil from '../utils/LogUtil';
import Want from '@ohos.app.ability.Want';
import { AbilityConstant, UIExtensionContentSession } from '@kit.AbilityKit';
import { StatusBarViewExtensionAbility } from '@kit.StatusBarExtensionKit';

const TAG: string = 'WineStatusBarEntryAbility';

export class WineStatusBarEntryAbility extends StatusBarViewExtensionAbility {
  protected session: UIExtensionContentSession | undefined = undefined;

  onCreate(launchParam: AbilityConstant.LaunchParam) {
    LogUtil.info(TAG, 'Ability onCreate');
  }

  onSessionCreate(want: Want, session: UIExtensionContentSession) {
    LogUtil.info(TAG, 'Ability onSessionCreate');
    this.session = session;
    session.loadContent('pages/StatusBarPage');
  }

  onForeground() {
    LogUtil.info(TAG, 'Ability onForeground');
  }

  onBackground() {
    LogUtil.info(TAG, 'Ability onBackground');
  }

  onSessionDestroy(session: UIExtensionContentSession) {
    LogUtil.info(TAG, 'Ability onSessionDestroy');
  }

  onDestroy() {
    LogUtil.info(TAG, 'Ability onDestroy');
  }
};

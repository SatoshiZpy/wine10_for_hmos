// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { inject, injectable } from 'inversify';
import { AbilityManager } from '../common/AbilityManager';
import { BaseAdapter } from '../common/BaseAdapter';
import { WineStatus } from '../common/Constants';
import LogMethod from '../common/LogDecorator';

@injectable()
export class AppLifecycleAdapter extends BaseAdapter {
  private abilityManager: AbilityManager;
  private webStatus: WineStatus;

  constructor(
    @inject(AbilityManager) abilityManager: AbilityManager,
  ) {
    super();
    this.abilityManager = abilityManager;
    this.webStatus = WineStatus.kInit;
  }

  @LogMethod
  onWineCreate() {
    this.webStatus = WineStatus.kStart;
    this.start();
  }

  @LogMethod
  onWineDestroy() {
    this.webStatus = WineStatus.kDestroy;
    this.destroy();
  }

  getWineStatus(): number {
    return this.webStatus;
  }

  start() {}

  destroy() {
    let proxy_list = this.abilityManager.getProxyList();
    for (let i = 0; i < proxy_list.length; i++) {
      proxy_list[i].closeWindow();
    }
  }
}

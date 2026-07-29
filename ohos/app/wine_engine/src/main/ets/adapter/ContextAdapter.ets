// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { injectable, inject } from 'inversify';
import { MODULE_TYPE } from "../common/ModuleType";
import { BaseAdapter } from '../common/BaseAdapter';
import { AbilityManager } from '../common/AbilityManager';
import LogMethod from '../common/LogDecorator';
import LogUtil from '../utils/LogUtil';
import { WineProxy, FakeWineProxy } from '../interface/WineProxy';

const TAG: string = 'ContextAdapter';

@injectable()
export class ContextAdapter extends BaseAdapter {
    private abilityManager: AbilityManager;
    private context: Context;
    private activeWindow: number = -1;
    private lastActiveUIAbility: number = -1;

    constructor(
        @inject(AbilityManager) abilityManager: AbilityManager,
        @inject(MODULE_TYPE.Context) ctx: Context
    ) {
        super();
        this.abilityManager = abilityManager;
        this.context = ctx;
    }

    public getContext(): Context {
        return this.context;
    }

    @LogMethod
    public setActiveWindow(activeWindow: number) {
        LogUtil.info(TAG, 'set current active window:' + activeWindow);
        this.activeWindow = activeWindow;
        let webProxy = this.abilityManager.getProxy(activeWindow);
        if (webProxy && webProxy.isUIAbility()) {
            LogUtil.info(TAG, 'set last active UI ability: ' + activeWindow);
            this.lastActiveUIAbility = activeWindow;
        }
    }

    @LogMethod
    public getActiveWindow(): number {
        if (this.activeWindow > 0) {
            return this.activeWindow;
        }
        let activeProxy = this.getActiveProxy();
        if (!activeProxy.isUIAbility()) {
            LogUtil.error(TAG, 'no active window context');
            return -1;
        }
        return activeProxy.getWindowId();
    }

    public getActiveContext(): Context | undefined {
        let activeProxy = this.getActiveProxy();
        if (!activeProxy) {
            LogUtil.error(TAG, 'no active window context');
        }
        return activeProxy?.getWindowContext();
    }

    public getActiveProxy(): WineProxy {
        let activeProxy = this.abilityManager.getProxy(this.activeWindow);
        if (activeProxy) {
            return activeProxy;
        }
 
        activeProxy = this.abilityManager.getProxy(this.lastActiveUIAbility);
        if (activeProxy) {
            return activeProxy;
        }
        LogUtil.warn(TAG, 'No active proxy. use default proxy');
 
        if (this.abilityManager.getProxyCount() > 0) {
            return this.abilityManager.getProxyList()[0];
        } else {
            LogUtil.error(TAG, 'No proxy can be used');
            return new FakeWineProxy();
        }
    }
}
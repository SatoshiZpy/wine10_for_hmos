// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { injectable } from 'inversify';
import { ConfigData } from './Constants';
import { WineProxy } from '../interface/WineProxy';

AppStorage.setOrCreate('abilityProxyList', []);

@injectable()
export class AbilityManager {
  private appStorage: WineProxy[];

  constructor() {
    let appStorageLink: SubscribedAbstractProperty<WineProxy[]>
      = AppStorage.link('abilityProxyList');
    this.appStorage = appStorageLink.get();
  }

  public addProxy(proxy: WineProxy) {
    for (let i = 0; i < this.appStorage.length; i++) {
      if (proxy.getWindowId() === this.appStorage[i].getWindowId()) {
        return;
      }
    }
    this.appStorage.push(proxy);
  }

  public removeProxy(proxyId: number) {
    for (let i = 0; i < this.appStorage.length; i++) {
      if (proxyId === this.appStorage[i].getWindowId()) {
        this.appStorage.splice(i, 1);
        break;
      }
    }
  }

  public getProxy(proxyId: number): WineProxy | undefined {
    for (let i = 0; i < this.appStorage.length; i++) {
      if (proxyId === this.appStorage[i].getWindowId()) {
        return this.appStorage[i];
      }
    }
    return undefined;
  }

  public isEmpty(): boolean {
    return this.appStorage.length === 0;
  }

  public getProxyCount(): number {
    return this.appStorage.length;
  }

  public getProxyList(): WineProxy[] {
    return this.appStorage;
  }
  
  public getWindowIdByOriginWindow(originWindowId: number): number {
    for (let i = 0; i < this.appStorage.length; i++) {
      if (originWindowId === this.appStorage[i].getOriginWindowId()) {
        return this.appStorage[i].getWindowId();
      }
    }
    return -1;
  }

  public getOriginWindowId(windowId: number): number {
    for (let i = 0; i < this.appStorage.length; i++) {
      if (windowId === this.appStorage[i].getWindowId()) {
        return this.appStorage[i].getOriginWindowId();
      }
    }
    return -1;
  }

};

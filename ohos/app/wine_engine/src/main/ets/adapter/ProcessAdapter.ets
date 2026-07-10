// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { inject, injectable } from 'inversify';
import childProcessManager from '@ohos.app.ability.childProcessManager';
import { BusinessError } from '@ohos.base';

import { BaseAdapter } from '../common/BaseAdapter';
import LogUtil from '../utils/LogUtil';
import LogMethod from '../common/LogDecorator';
import lazy { ContextAdapter } from './ContextAdapter';
import { bundleManager, contextConstant, StartOptions, Want } from '@kit.AbilityKit';
import LaunchHelper from '../utils/LaunchHelper';
import CheckEmptyUtils from '../utils/CheckEmptyUtils';
import lazy { AppLifecycleAdapter } from './AppLifecycleAdapter';

const TAG: string = 'ProcessAdapter';

function fireProcessBind(callback: Function) {
  const invalidPid: number = -1;
  try {
    childProcessManager.startChildProcess('./ets/process/CustomChildProcess.ets',
      childProcessManager.StartMode.APP_SPAWN_FORK)
      .then((result) => {
        callback(result);
      }, (err: BusinessError) => {
        callback(invalidPid);
        LogUtil.error('ProcessAdapter', `startChildProcess failed: ${err}`);
      });
  } catch (err) {
    callback(invalidPid);
    LogUtil.error('ProcessAdapter', `startChildProcess failed: ${err}`);
  }
}

@injectable()
export class ProcessAdapter extends BaseAdapter {
  private bundleName: string = 'com.huawei.ohos_wine';
  private abilityName: string = 'IsolateProcessAbility';
  private bundleFlags = bundleManager.BundleFlag.GET_BUNDLE_INFO_WITH_APPLICATION |
    bundleManager.BundleFlag.GET_BUNDLE_INFO_WITH_METADATA;
  private ctxAdapter: ContextAdapter;
  private lifecycleAdapter: AppLifecycleAdapter;

  constructor(
    @inject(ContextAdapter) ctxAdapter: ContextAdapter,
    @inject(AppLifecycleAdapter) lifecycleAdapter: AppLifecycleAdapter
  ) {
    super();
    this.ctxAdapter = ctxAdapter;
    this.lifecycleAdapter = lifecycleAdapter;

    try {
      bundleManager.getBundleInfoForSelf(this.bundleFlags).then((data) => {
        this.bundleName = data?.name;
      }).catch((err: BusinessError) => {
        LogUtil.error(TAG, `getBundleInfoForSelf failed. Cause: ${err.message}`);
      });
    } catch (err) {
      let message = (err as BusinessError).message;
      LogUtil.error(TAG, `getBundleInfoForSelf failed.: ${message}`);
    }
  }

  @LogMethod
  startChildProcess(callback: Function): void {
    fireProcessBind(callback);
  }

  @LogMethod
  createProcess(processKey: string, sockPath: string, callback: (ret: boolean) => void): void {
    if (CheckEmptyUtils.checkStrIsEmpty(processKey)) {
      LogUtil.error(TAG, 'createProcess failed. Cause: invalid process key');
    }

    const want: Want = {
      bundleName: this.bundleName,
      abilityName: this.abilityName,
      parameters: {
        'instanceKey': processKey + ':1',
        'processKey': processKey,
        'sockPath': sockPath,
      },
    };

    let options: StartOptions = {
      processMode: contextConstant.ProcessMode.ATTACH_TO_STATUS_BAR_ITEM,
      startupVisibility: contextConstant.StartupVisibility.STARTUP_HIDE,
    }
    LogUtil.info(TAG, 'createProcess want:' + JSON.stringify(want) + 'options:' + JSON.stringify(options));
    LaunchHelper.LaunchWithOptions(this.ctxAdapter.getActiveContext(), want, options)
      .then((result) => {
        LogUtil.info(TAG, 'createProcess result:' + result);
        callback(result);
      }).catch((err: BusinessError) => {
      LogUtil.error(TAG, 'Failed to create process. Cause:' + JSON.stringify(err));
      callback(false);
    });
  }

  @LogMethod
  killProcess(): void {
    this.lifecycleAdapter.onDestroy();
  }
}
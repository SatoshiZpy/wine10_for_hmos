// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import AbilityStage from '@ohos.app.ability.AbilityStage';
import { ContextType } from '../common/Constants';
import JsBindingUtils from '../utils/JsBindingUtils';
import Want from '@ohos.app.ability.Want';
import lazy { GlobalThisHelper } from '../utils/GlobalThisHelper';
import lazy { NativeContext } from '../interface/CommonInterface';
import lazy { CommonDependencyProvider } from '../common/CommonDependencyProvider';
import { AbilityConstant, ConfigurationConstant } from '@kit.AbilityKit';
import LogUtil from '../utils/LogUtil';

const DELAYED_TIME = 0;
const TAG: string = 'WineAbilityStage';

export class WineAbilityStage extends AbilityStage {
  private nativeContext: NativeContext | undefined = undefined;
  private processName: string = '';

  onCreate(): void {
    this.runTaskAsync();
  }

  onAcceptWant(want: Want): string {
    LogUtil.info(TAG, 'onAcceptWant want: ' + JSON.stringify(want));
    let instanceKey = want.parameters?.instanceKey as string | undefined;
    if (instanceKey) {
      return instanceKey.toString();
    }
    instanceKey = `${this.processName}:1`;
    LogUtil.info(TAG, 'onAcceptWant return default_instance:' + instanceKey);
    return instanceKey;
  }

  onNewProcessRequest(want: Want): string {
    LogUtil.info(TAG, 'onNewProcessRequest want: ' + JSON.stringify(want));
    let processKey = want.parameters?.processKey as string | undefined;
    if (processKey) {
      return processKey.toString();
    }
    processKey = this.processName;
    LogUtil.info(TAG, 'onNewProcessRequest return default_process:' + processKey);
    return processKey;
  }

  async onPrepareTerminationAsync(): Promise<AbilityConstant.PrepareTermination> {
    LogUtil.info(TAG, 'onPrepareTerminationAsync');
    return AbilityConstant.PrepareTermination.TERMINATE_IMMEDIATELY;
  }

  private runTaskAsync() {
    setTimeout(() => {
      let appContext = this.context.getApplicationContext();
      appContext.setColorMode(ConfigurationConstant.ColorMode.COLOR_MODE_NOT_SET);
      JsBindingUtils.initNativeContext(ContextType.kMainProcess);
      this.nativeContext = JsBindingUtils.getNativeContext(ContextType.kMainProcess);
      if (!GlobalThisHelper.isLaunched()) {
        GlobalThisHelper.appInit(new CommonDependencyProvider(appContext));
      }
      import('../jsbindings/JsBindingMethod').then((ns:ESObject) => {
        ns.JsBindingMethod.bind();
      }).catch(()=>{
        LogUtil.error(TAG, 'import failed');
      });

      appContext.getRunningProcessInformation().then((data) => {
        for (let info of data) {
          LogUtil.info(TAG, 'ProcessInfo:' + JSON.stringify(info));
          this.processName = info.processName;
        }
      }).catch((err: BusinessError) => {
        LogUtil.error(TAG, 'Failed to getRunningProcessInformation. Cause:' + JSON.stringify(err));
      });
    }, DELAYED_TIME)
  }
}

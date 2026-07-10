// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { StartOptions, Want } from '@kit.AbilityKit';
import LogUtil from './LogUtil';
import common from '@ohos.app.ability.common';
import { BusinessError } from '@kit.BasicServicesKit';

const TAG: string = 'LaunchHelper';

export default class LaunchHelper {
  static LaunchWithOptions(uiContext: Context | undefined,
                           want: Want,
                           options: StartOptions): Promise<boolean> {
    return new Promise<boolean>((resolve, reject) => {
      try {
        let context = uiContext as common.UIAbilityContext ||
          uiContext as common.UIExtensionContext;
        if (!context) {
          LogUtil.error(TAG, `excute failed, want: ${JSON.stringify(want)}, options: ${JSON.stringify(options)}`);
          resolve(false);
          return;
        }
        context.startAbility(want, options)
          .then(() => {
            resolve(true);
          })
          .catch((error: BusinessError) => {
            LogUtil.error(TAG, `excute failed. Cause: ${JSON.stringify(error)}`);
            resolve(false);
          });
      } catch (exception) {
        LogUtil.error(TAG, `excute failed exception. Cause: ${JSON.stringify(exception)}`);
        resolve(false);
      }
    });
  }

  static Launch(uiContext: Context | undefined, want: Want): Promise<boolean> {
    return LaunchHelper.LaunchWithOptions(uiContext, want, {});
  }

  static LaunchInProcess(uiContext: Context | undefined,
                         want: Want,
                         specifiedFlag: string,
                         options: StartOptions): Promise<boolean> {
    return new Promise<boolean>((resolve, reject) => {
      try {
        let context = uiContext as common.UIAbilityContext;
        if (!context) {
          LogUtil.error(TAG, `excute failed, want: ${JSON.stringify(want)}, options: ${JSON.stringify(options)}`);
          resolve(false);
          return;
        }
        context.startSelfUIAbilityInCurrentProcess(want, specifiedFlag, options)
          .then(() => {
            resolve(true);
          })
          .catch((error: BusinessError) => {
            LogUtil.error(TAG, `excute failed. Cause: ${JSON.stringify(error)}`);
            resolve(false);
          });
      } catch (exception) {
        LogUtil.error(TAG, `excute failed exception. Cause: ${JSON.stringify(exception)}`);
        resolve(false);
      }
    });
  }
}
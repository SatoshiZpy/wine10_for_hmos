// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { inject, injectable } from 'inversify';
import { common, OpenLinkOptions, UIAbility, Want, wantConstant } from '@kit.AbilityKit';

import { BaseAdapter } from '../common/BaseAdapter';
import LogMethod from '../common/LogDecorator';
import LaunchHelper from '../utils/LaunchHelper';
import { ContextAdapter } from './ContextAdapter';
import { fileUri } from '@kit.CoreFileKit';
import LogUtil from '../utils/LogUtil';
import { fileIo as fs } from '@kit.CoreFileKit';
import { BusinessError } from '@kit.BasicServicesKit';

const TAG: string = 'FileManagerAdapter';
const OPEN_IN_FOLDER_LINK: string = 'filemanager://openDirectory';

@injectable()
export class FileManagerAdapter extends BaseAdapter {
  private ctxAdapter: ContextAdapter;

  constructor(@inject(ContextAdapter) ctxAdapter: ContextAdapter) {
    super();
    this.ctxAdapter = ctxAdapter;
  }

  @LogMethod
  openItemInFolder(filePath: string) {
    let uri = fileUri.getUriFromPath(filePath);
    let openLinkOptions: OpenLinkOptions = {
      parameters: {
        'fileUri': uri
      }
    };
    let context = this.ctxAdapter.getActiveContext() as common.UIAbilityContext;
    if (!context) {
      LogUtil.warn(TAG, 'Failed to get context');
      return;
    }
    try {
      context.openLink(OPEN_IN_FOLDER_LINK, openLinkOptions, (err, result) => {
        if(err){
          LogUtil.error(TAG,
            `Failed to open in folder ,err is: ${JSON.stringify(err)}, result is ${JSON.stringify(result)}`);
        }
      })
    } catch (err) {
      LogUtil.error(TAG, `Failed to execute openLink , err is : ${JSON.stringify(err)}`);
    }
  }

  @LogMethod
  openVerifiedItem(filePath: string) {
    let uri = fileUri.getUriFromPath(filePath);
    const want: Want = {
      // Configure read and write permissions for shared files,
      // such as granting read and write authorization to shared applications
      flags: wantConstant.Flags.FLAG_AUTH_WRITE_URI_PERMISSION |
             wantConstant.Flags.FLAG_AUTH_READ_URI_PERMISSION,
      action: "ohos.want.action.viewData",
      uri: uri
    }
    LaunchHelper.Launch(this.ctxAdapter.getActiveContext(), want);
  }

  @LogMethod
  createSymlink(srcPath: string, dstPath: string) {
    fs.symlink(srcPath, dstPath)
      .catch((err: BusinessError) => {
        LogUtil.error(TAG, `symlink failed with error: ${JSON.stringify(err)}`);
      });
  }
}
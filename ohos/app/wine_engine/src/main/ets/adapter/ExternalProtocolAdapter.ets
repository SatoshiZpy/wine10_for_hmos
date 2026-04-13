// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { inject, injectable } from 'inversify';
import { Want, wantConstant } from '@kit.AbilityKit';

import { BaseAdapter } from '../common/BaseAdapter';
import LogMethod from '../common/LogDecorator';
import LaunchHelper from '../utils/LaunchHelper';
import { ContextAdapter } from './ContextAdapter';

@injectable()
export class ExternalProtocolAdapter extends BaseAdapter {
  private ctxAdapter: ContextAdapter;

  constructor(@inject(ContextAdapter) ctxAdapter: ContextAdapter) {
    super();
    this.ctxAdapter = ctxAdapter;
  }

  @LogMethod
  openExternal(urlStr: string) {
    const want: Want = {
      flags: wantConstant.Flags.FLAG_START_WITHOUT_TIPS,
      uri: urlStr,
      action: "ohos.want.action.viewData"
    }
    LaunchHelper.Launch(this.ctxAdapter.getActiveContext(), want);
  }
}

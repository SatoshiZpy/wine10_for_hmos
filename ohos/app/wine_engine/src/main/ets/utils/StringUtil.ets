// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import fileUri from '@ohos.file.fileuri';
import { ConfigData } from '../common/Constants';
import { util } from '@kit.ArkTS';

export default class StringUtil {
  private static TAG: string = 'StringUtil';

  static uriConvert(docs: string[]): string[] {
    let copy: string[] = [];
    docs.forEach((path) => {
      // Resolve Chinese garbled characters
      path = decodeURI(path);
      // Convert to uri get systeam path
      let uri = new fileUri.FileUri(path);
      copy.push(uri.path);
    });
    return copy;
  }

  static widgetConvert(widgetId: string | number): string {
    if (typeof widgetId === 'number') {
      return ConfigData.WINDOW_PREFIX + widgetId;
    }
    return widgetId;
  }

  static uint8ArrayToString(array: Uint8Array): string {
    let textDecoderOptions: util.TextDecoderOptions = {
      fatal: false,
      ignoreBOM: true
    };
    let decodeToStringOptions: util.DecodeToStringOptions = {
      stream: false
    };
    let textDecoder = util.TextDecoder.create('utf-8', textDecoderOptions);
    let retStr = textDecoder.decodeToString(array, decodeToStringOptions);
    return retStr;
  }
}

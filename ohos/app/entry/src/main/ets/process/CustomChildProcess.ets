// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { WineChildProcess } from 'wine_engine/childProcess';

export default class CustomChildProcess extends WineChildProcess {
  onStart(): void {
    super.onStart();
  }

  static toString(): string {
    return "CustomChildProcess";
  }
}
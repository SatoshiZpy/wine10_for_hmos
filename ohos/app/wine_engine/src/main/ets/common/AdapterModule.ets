// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { ContainerModule } from 'inversify';


import lazy { ContextAdapter } from '../adapter/ContextAdapter';
import lazy { ContextPathAdapter } from '../adapter/ContextPathAdapter';
import lazy { DisplayAdapter } from '../adapter/DisplayAdapter';
import lazy { DragDropAdapter } from '../adapter/DragDropAdapter';
import lazy { MultiInputAdapter } from '../adapter/MultiInputAdapter';
import lazy { NativeThemeAdapter } from '../adapter/NativeThemeAdapter';
import lazy { PermissionManagerAdapter } from '../adapter/PermissionManagerAdapter';
import lazy { DeviceInfoAdapter } from '../adapter/DeviceInfoAdapter';
import lazy { PresetDeployerAdapter } from '../adapter/PresetDeployerAdapter';

// Only the adapters which used in WineAbility or WineWindow should be added here.
export const adapterModule = new ContainerModule((bind) => {
  bind(ContextAdapter).toSelf();
  bind(DisplayAdapter).toSelf();
  bind(DragDropAdapter).toSelf();
  bind(MultiInputAdapter).toSelf();
  bind(NativeThemeAdapter).toSelf();
  bind(ContextPathAdapter).toSelf();
  bind(PermissionManagerAdapter).toSelf();
  bind(DeviceInfoAdapter).toSelf();
  bind(PresetDeployerAdapter).toSelf();
});

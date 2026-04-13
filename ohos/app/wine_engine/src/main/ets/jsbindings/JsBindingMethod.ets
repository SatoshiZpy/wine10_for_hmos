// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { AccessibilityAdapterBind } from '../jsbindings/AccessibilityAdapterBind';
import { AppLifecycleAdapterBind } from './AppLifecycleAdapterBind';
import { AppWindowAdapterBind } from '../jsbindings/AppWindowAdapterBind';
import { BatteryAdapterBind } from '../jsbindings/BatteryAdapterBind';
import { BluetoothAdapterBind } from '../jsbindings/BluetoothAdapterBind';
import { BluetoothLowEnergyAdapterBind } from '../jsbindings/BluetoothLowEnergyAdapterBind';
import { CertManagerAdapterBind } from '../jsbindings/CertManagerAdapterBind';
import { ContextAdapterBind } from './ContextAdapterBind';
import { ContextPathAdapterBind } from '../jsbindings/ContextPathAdapterBind';
import { CursorAdapterBind } from '../jsbindings/CursorAdapterBind';
import { DeviceAdapterBind } from '../jsbindings/DeviceAdapterBind';
import { DeviceUserAuthAdapterBind } from '../jsbindings/DeviceUserAuthAdapterBind';
import { DisplayAdapterBind } from '../jsbindings/DisplayAdapterBind';
import { DragDropAdapterBind } from '../jsbindings/DragDropAdapterBind';
import { ExternalProtocolAdapterBind } from '../jsbindings/ExternalProtocolAdapterBind';
import { FileManagerAdapterBind } from '../jsbindings/FileManagerAdapterBind';
import { FilePickerAdapterBind } from '../jsbindings/FilePickerAdapterBind';
import { FontAdapterBind } from '../jsbindings/FontAdapterBind';

import { I18nAdapterBind } from '../jsbindings/I18nAdapterBind';
import { IMFAdapterBind } from '../jsbindings/IMFAdapterBind';
import { MediaAdapterBind } from '../jsbindings/MediaAdapterBind';
import { MimeTypeAdapterBind } from '../jsbindings/MimeTypeAdapterBind';
import { NativeThemeAdapterBind } from '../jsbindings/NativeThemeAdapterBind';
import { NetConnectionAdapterBind } from '../jsbindings/NetConnectionAdapterBind';
import { NotificationAdapterBind } from '../jsbindings/NotificationAdapterBind';
import { OcrAdapterBind } from '../jsbindings/OcrAdapterBind';
import { PermissionManagerAdapterBind } from '../jsbindings/PermissionManagerAdapterBind';
import { PowerMonitorAdapterBind } from '../jsbindings/PowerMonitorAdapterBind';
import { PrintAdapterBind } from '../jsbindings/PrintAdapterBind';
import { ProcessAdapterBind } from '../jsbindings/ProcessAdapterBind';
import { RunningLockAdapterBind } from '../jsbindings/RunningLockAdapterBind';
import { ScreenlockMonitorAdapterBind } from '../jsbindings/ScreenlockMonitorAdapterBind';
import { ScreenshotAdapterBind } from '../jsbindings/ScreenshotAdapterBind';
import { ShapeDetectionAdapterBind } from '../jsbindings/ShapeDetectionAdapterBind';
import { SpeechAdapterBind } from '../jsbindings/SpeechAdapterBind';
import { SubWindowAdapterBind } from '../jsbindings/SubWindowAdapterBind';
import { SystemFloatingWindowAdapterBind } from './SystemFloatingWindowAdapterBind';

export class JsBindingMethod {
  static bind() {
    AccessibilityAdapterBind.bind();
    AppLifecycleAdapterBind.bind();
    AppWindowAdapterBind.bind();
    BatteryAdapterBind.bind();
    BluetoothAdapterBind.bind();
    BluetoothLowEnergyAdapterBind.bind();
    CertManagerAdapterBind.bind();
    ContextAdapterBind.bind();
    ContextPathAdapterBind.bind();
    CursorAdapterBind.bind();
    DeviceAdapterBind.bind();
    DeviceUserAuthAdapterBind.bind();
    DisplayAdapterBind.bind();
    DragDropAdapterBind.bind();
    ExternalProtocolAdapterBind.bind();
    FileManagerAdapterBind.bind();
    FilePickerAdapterBind.bind();
    FontAdapterBind.bind();
    I18nAdapterBind.bind();
    IMFAdapterBind.bind();
    MediaAdapterBind.bind();
    MimeTypeAdapterBind.bind();
    NativeThemeAdapterBind.bind();
    NetConnectionAdapterBind.bind();
    NotificationAdapterBind.bind();
    OcrAdapterBind.bind();
    PermissionManagerAdapterBind.bind();
    PowerMonitorAdapterBind.bind();
    PrintAdapterBind.bind();
    ProcessAdapterBind.bind();
    RunningLockAdapterBind.bind();
    ScreenlockMonitorAdapterBind.bind();
    ScreenshotAdapterBind.bind();
    ShapeDetectionAdapterBind.bind();
    SpeechAdapterBind.bind();
    SubWindowAdapterBind.bind();
    SystemFloatingWindowAdapterBind.bind();
  }
}

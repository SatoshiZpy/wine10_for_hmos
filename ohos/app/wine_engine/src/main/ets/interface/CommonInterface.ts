// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type image from '@ohos.multimedia.image';
import type inputMethod from '@ohos.inputMethod';
import type GestureEvent from '@ohos.multimodalInput.gestureEvent';
import type ConfigurationConstant from '@ohos.app.ability.ConfigurationConstant';
import type window from '@ohos.window';

export class PowerMonitor {
  OnSuspend: () => void;
  OnResume: () => void;
  OnPowerStateChanged: () => void;
}

export interface WindowBound {
  left: number;
  top: number;
  width: number;
  height: number;
}

export interface CaptionButtonRect {
  right: number;
  top: number;
  width: number;
  height: number;
}

export class JSBind {
  bindFunction: (name: string, func: Function) => number;
}

export interface CommandParameter {
  url?: string;
  user_data?: string;
  is_sync?: boolean;
}

export interface CommandResult {
  ret_code: number;
  widget_Id: number;
  last_widget_Id: number;
  pid: number;
}

export interface WindowLimits {
  maxHeight: number;
  maxWidth: number;
  minHeight: number;
  minWidth: number;
}

export interface NativeContext {
  runWine: (vec_args: string[], env: string[]) => void;
  Destroyed: () => boolean;
  runOtherProcessType: (processType: number) => void;
  registerLifecycle: () => void;
  readImageFromReceiver: (receiver: image.ImageReceiver) => image.Image;
  JSBind: JSBind;

  OnPanEventCB: (action: number, id: number, event: GestureEvent) => void;
  OnPinchEventCB: (pinch_step: string, id: number, event: GestureEvent) => void;
  OnDoubleTapCB: (id: number, offsetX: number, offsetY: number) => void;

  InsertTextCallback: (text: string) => void;
  DeleteBackCallback: (length: number) => void;
  DeleteForwardCallback: (length: number) => void;
  SendEnterKeyEventCallback: (enterKeyType: number) => void;
  MoveCursorCallback: (direction: inputMethod.Direction) => void;

  SetThemeSource: (themeSource: ConfigurationConstant.ColorMode) => void;

  OnDragEnterCB: (id: number, dragInfo: OhosDropData, fileUris: Array<string>) => void;
  OnDragLeaveCB: (id: number) => void;
  OnDragEndCB: (id: number) => void;
  OnDragMoveCB: (id: number, windowX: number, windowY: number) => void;
  OnDropCB: (id: number, dragInfo: OhosDropData, fileUris: Array<string>) => void;

  OnWindowInitSize: (windowRect: WindowBound, drawableRect: WindowBound) => void;
  OnWindowStatusChange: (id: number, status: window.WindowStatusType) => void;
  OnWindowInitState: (state: window.WindowStatusType) => void;
  OnWindowRectChange: (id: number, event: WindowBound, reason: number) => void;
  OnWindowSizeChange: (id: number, event: WindowBound) => void;
  OnWindowEvent: (id: number, event: number) => void;
  OnWindowVisibilityChange: (id: number, visible: boolean) => void;
  OnKeyboardHeightChange: (id: number, height: number) => void;
  OnCaptionButtonRectChange: (id: number, event: CaptionButtonRect) => void;
  SetSystemWindowLimits: (windowLimits: WindowLimits) => void;
  OnDeviceModeChange: (id: number, event: ChangeEventType) => void;

  OnNotificationClickCallback: (id: number) => void;
  OnNotificationCloseCallback: (id: number) => void;
  OnNotificationButtonClickCallback: (id: number, buttonIndex) => void;

  OnDisplayChangeCallback: (even: string, id: number) => void;
  OnFontSizeChangeCallback:(fontSizeZoom :number) => void;

  PowerMonitor: PowerMonitor;
  ExecuteCommand: (id: number, param: CommandParameter) => CommandResult;
  GetCloseResponse: (id: number) => CloseResponse;

  RegisterWindowEventFilter: (origin_window_id: number) => void;
  ClearWindowEventFilter: (origin_window_id: number) => void;

  UpdateWindowDeviceModeSwitchCB: (mode: DeviceMode) => void;
}

export interface IParams {
  callback: (id: number) => void,
  id: number,
  size: number[], // [width, height]
  initColorRgb: number,
  ownerPid: number,
}

export interface OhosBasicDragData {
  text: string;
  url: string;
  urlTitle: string;
  html: string;
}

export interface OhosDragParamToJs {
  basicData: OhosBasicDragData;
  imageTempUri: string;
  width: number;
  height: number;
  bookmarkBuffer: ArrayBuffer;
  webCustomBuffer: ArrayBuffer;
  pixelBuffer: ArrayBuffer;
  widgetId: number;
  touchX: number;
  touchY: number;
}

export interface OhosDropData {
  basicData: OhosBasicDragData;
  fileUris: Array<string>;
  bookmarkBuffer: ArrayBuffer | undefined;
  webCustomBuffer: ArrayBuffer | undefined;
}

export interface IMFAdapterInputAttribute {
  inputPattern: inputMethod.TextInputType;
  enterKeyType: inputMethod.EnterKeyType;
}

export interface IMFAdapterCursorInfo {
  left: number;
  top: number;
  width: number;
  height: number;
}

export interface IMFAdapterTextConfig {
  inputAttribute: IMFAdapterInputAttribute;
  cursorInfo: IMFAdapterCursorInfo;
}

export interface NotificationAdapterImage {
  width: number;
  height: number;
  buff: ArrayBuffer;
}

export interface NotificationAdapterButton {
  title: string;
  buttonIndex: number;
}

export interface OcrAdapterImage {
  width: number;
  height: number;
  buff: ArrayBuffer;
}

export interface PixelPoint {
  x: number;
  y: number;
}

export interface TextWord {
  value: string;
  cornerPoints: Array<PixelPoint>;
}

export interface NotificationAdapterRequest {
  notificationId: number;
  title: string;
  message: string;
  requireInteraction: boolean;
  silent: boolean;
  timestamp: number;
  icon: NotificationAdapterImage;
  buttons: NotificationAdapterButton[];
}

export interface OhosPasteDataRecord {
  html_text: string;
  mime_type: string;
  plain_text: string;
}

export interface SpeakingParamsExtraParams {
  speed?: number;
  volume?: number;
  pitch?: number;
  languageContext?: string;
  audioType?: string;
  playType?: number;
  soundChannel?: number;
  queueMode?: number;
}

export interface SpeakingParams {
  requestId: string;
  extraParams?: SpeakingParamsExtraParams;
}

export interface EngineCreationParamsExtraParams {
  style?: string;
  locate?: string;
  name?: string;
}

export interface EngineCreationParams {
  language: string;
  online: number;
  person: number;
  extraParams?: EngineCreationParamsExtraParams;
}

export interface VoiceQueryExtraParams {
  language?: string;
  person?: number;
}

export interface VoiceQuery {
  requestId: string;
  online: number;
  extraParams?: VoiceQueryExtraParams
}

export interface VoiceInfo {
  language: string;
  person: number;
  style: string;
  status: string;
  gender: string;
  description: string;
}

export interface AdvertisingParam {
  connectable: boolean;
  service_uuids: string[];
  manufacturer_data: Map<number, Uint8Array>;
  service_data: Map<string, Uint8Array>;
  scan_response_data: Map<number, Uint8Array>;
}

export interface BatteryInfo {
  batterySOC: number,
  chargingStatus: number,
  isBatteryPresent: boolean,
  estimatedRemainingChargeTime: number,
  nowCurrent: number,
  remainingEnergy: number
}

export enum WindowFeatures {
  WINDOW_TITLE_BAR = 0,
  WINDOW_CLOSE_BUTTON,
  WINDOW_MINIMIZE_BUTTON,
  WINDOW_MAXIMIZE_BUTTON,
  WINDOW_RESIZABLE,
  WINDOW_TOUCHABLE,
  WINDOW_HAS_SHADOW,
  WINDOW_NO_ACTIVATION,
  USE_FLOATING_WINDOW,
  USE_DARK_MODE,
};

export enum WindowState {
  WINDOW_DISABLED = 0,
  WINDOW_NO_FOREGROUND,
  WINDOW_TOP_MOST,
  WINDOW_EXCLUDED_BY_CONTROL,
  WINDOW_MINIMIZED,
  WINDOW_MAXIMIZED,
  WINDOW_FULL_SCREEN,
  WINDOW_SPLIT_MODE,
  WINDOW_VISIABLE,
};

export class WindowStyles {
  hide_title_bar: boolean;
  closable: boolean;
  minimizable: boolean;
  maximizable: boolean;
  use_floating_window: boolean;
  use_dark_mode: boolean;
  always_on_top: boolean;
  resizable: boolean;
  focusable: boolean;
  touchable: boolean;
  show_widow: boolean;
}

export interface NewWindowParam {
  ability_type: AbilityType,
  parent_id: number
  widget_id: number,
  bounds: WindowBound,
  feature_mask: number,
  window_state_mask: number,
  init_color_argb: number,
  hwnd: number,
  owner_pid: number,
}

export interface ISubWindowInfo {
  id: number,
  parentId: number,
  subWindow: window.Window,
  localStorage: LocalStorage,
}

export interface SelectFileDialogParams {
  multi_files: boolean,
  extensions: Array<Array<string>>,
  descriptions: Array<string>,
  include_all_files: boolean
}

export interface SaveAsDialogParams {
  file_name: string,
  dir_name: string,
  extensions: Array<Array<string>>,
  descriptions: Array<string>,
  include_all_files: boolean
}

export interface PointCoordinate {
  x: number
  y: number,
  displayId: number,
}

export enum CloseResponse {
  kUndetermined,
  kClosingContinue,
  kClosingInterrupt,
  kClosed,
  kCloseCancelled,
  kClosedAnyway,
}

export enum DeviceMode {
  kPcMode = 0,
  kNormalWindowMode,
  kFreeWindowsMode,
};

export enum ChangeEventType {
  CHANGE_TO_NORMAL_MODE = 0,
  CHANGE_TO_FREE_MODE
};

export enum AbilityType {
  kEntryAbility = 0,
  kEmbeddedAbility,
};

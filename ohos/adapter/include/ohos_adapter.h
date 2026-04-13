// Copyright (c) 2025 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_API_H_
#define OHOS_ADAPTER_API_H_

#include <stdarg.h>

#include <native_buffer/native_buffer.h>
#include <native_window/external_window.h>
#include <native_window/graphic_error_code.h>

#include "ime_event_callback.h"
#include "input_event_callback.h"
#include "ohos_adapter_common.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#ifdef _WIN32
# define ADAPTER_IMPORT
# define ADAPTER_EXPORT
#elif defined(__GNUC__)
# define ADAPTER_IMPORT
# define ADAPTER_EXPORT __attribute__((visibility("default")))
#else
# define ADAPTER_IMPORT
# define ADAPTER_EXPORT
#endif

#ifdef LIBADAPTER_SOURCE
# define ADAPTER_API ADAPTER_EXPORT
#else
# define ADAPTER_API ADAPTER_IMPORT
#endif

typedef OHNativeWindow* ohos_window;
typedef OHNativeWindowBuffer* ohos_window_buffer;
typedef BufferHandle* ohos_buffer_handle;

/* window */
ADAPTER_API ohos_window CreateNativeWindow(const struct window_param* param, int* widget_id);
ADAPTER_API void CloseNativeWindow(int widget_id);
ADAPTER_API void SetWindowBounds(int widget_id, const struct window_bounds* bounds);
ADAPTER_API void SetParentWindow(int widget_id, int parent_id);
ADAPTER_API void SetWindowFeatures(int widget_id, const struct window_features* wf);
ADAPTER_API void SetWindowState(int widget_id, const struct window_state* state);
ADAPTER_API void SetWindowAlpha(int widget_id, float opacity);
ADAPTER_API void SetWindowTitle(int widget_id, const char* title);
ADAPTER_API void SetWindowVisible(int widget_id, int visible);
ADAPTER_API struct window_bounds GetInitialBounds(void);

/* process */
ADAPTER_API int StartNativeProcess(int argc, char **argv, const struct launch_options *options);
ADAPTER_API void KillNativeProcess(int exitcode, int force);
ADAPTER_API int IsMainProcess(void);
ADAPTER_API int GetMainProcessId(void);

/* cursor */
ADAPTER_API void SetCustomCursor(int widget_id,
                                 int width,
                                 int height,
                                 int hotspot_x,
                                 int hotspot_y,
                                 const unsigned int *bits);

/* ipc */
ADAPTER_API int GetGlobalDescriptor(int key);

/* display */
ADAPTER_API struct window_bounds GetDisplayAvailArea(void);
ADAPTER_API int GetSystemDpi(void);

/* event */
ADAPTER_API void RegisterInputEventCallBack(input_event_callback callback);
ADAPTER_API void RegisterWindowEventCallBack(input_event_callback callback);

/* ime */
ADAPTER_API void RegisterIMEEventCallBack(const ime_event_callback callback);
ADAPTER_API void IMEAttachTextInput(bool show_keyboard,
                                    const struct ime_text_config* text_config,
                                    int32_t request_keyboard_reason);
ADAPTER_API void IMEDetachTextInput();
ADAPTER_API void IMEUpdateAttribute(const struct ime_input_attribute input_attribute);
ADAPTER_API void IMENotifyCursorUpdate(const struct ime_cursor_info* cursor_info);

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif  // OHOS_ADAPTER_API_H_

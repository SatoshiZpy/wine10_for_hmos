// Copyright (c) 2025 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_API_H_
#define OHOS_ADAPTER_API_H_

#include <stdarg.h>
#include <stdint.h>

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

typedef OHNativeWindow* ohos_native_window;
typedef OHNativeWindowBuffer* ohos_window_buffer;
typedef BufferHandle* ohos_buffer_handle;

/* window */
ADAPTER_API int CreateNativeWindow(const struct window_param* param);
ADAPTER_API void CloseNativeWindow(int widget_id);
ADAPTER_API void SetWindowBounds(int widget_id, const struct window_bounds* bounds);
ADAPTER_API void MaximizeWindow(int widget_id);
ADAPTER_API void MinimizeWindow(int widget_id);
ADAPTER_API void RestoreWindow(int widget_id);
ADAPTER_API void SetParentWindow(int widget_id, int parent_id);
ADAPTER_API void SetWindowFeatures(int widget_id, const struct window_features* wf);
ADAPTER_API void SetWindowState(int widget_id, const struct window_state* state);
ADAPTER_API void SetWindowAlpha(int widget_id, float opacity);
ADAPTER_API void SetWindowTitle(int widget_id, const char* title);
ADAPTER_API void SetWindowVisible(int widget_id, int visible);
ADAPTER_API struct window_bounds GetInitialBounds(void);
ADAPTER_API ohos_native_window GetNativeWindow(int widget_id);
ADAPTER_API int GetFocusWindow(void);

/* client surface: an XComponent nested into the parent window page */
ADAPTER_API int CreateClientSurface(struct client_surface_param* param);
ADAPTER_API void SetClientSurfaceBounds(int client_widget_id, int off_x, int off_y,
                                        int width, int height);
ADAPTER_API void DestroyClientSurface(int client_widget_id);

/* process */
ADAPTER_API int StartNativeProcess(int argc, char **argv, const struct launch_options *options);
ADAPTER_API void KillNativeProcess(int exitcode);
ADAPTER_API int IsMainProcess(void);
ADAPTER_API int GetMainProcessId(void);

/* cursor */
ADAPTER_API void SetCustomCursor(int widget_id,
                                 int width,
                                 int height,
                                 int hotspot_x,
                                 int hotspot_y,
                                 const unsigned int *bits);
/*
 * Apply a HarmonyOS built-in pointer style on a given widget.
 * cursor_type values map to ohos::adapter::OhosCursorType
 * (kDefault = 0, kHandPointing, kTextCursor, ...). We pass int here so
 * the C-only wineohos.drv side does not pull in the C++ enum class.
 * Pass cursor_type < 0 to mean "default arrow" (used when the app
 * issues SetCursor(NULL) and we still need to clear a previously
 * applied custom cursor).
 *
 * NB: do NOT rename this to SetSystemCursor -- that symbol is taken by
 * the Win32 USER API declared in winuser.h, which several TUs that
 * also #include "ohos_adapter.h" (win32u/font.c, sysparams.c,
 * window.c) end up pulling in.
 */
ADAPTER_API void SetCursorStyle(int widget_id, int cursor_type);

/* ipc */
ADAPTER_API int GetGlobalDescriptor(int key);

/* display */
ADAPTER_API struct window_bounds GetDisplayAvailArea(void);
ADAPTER_API int GetSystemDpi(void);
ADAPTER_API float GetDensityPixels(void);

/* event */
ADAPTER_API void RegisterInputEventCallBack(input_event_callback callback);
ADAPTER_API void RegisterWindowEventCallBack(input_event_callback callback);

/* ime */
ADAPTER_API void RegisterIMEEventCallBack(const ime_event_callback callback);
ADAPTER_API void IMEAttachTextInput(bool show_keyboard,
                                    const struct ime_text_config* text_config,
                                    int32_t request_keyboard_reason);
ADAPTER_API void IMEDetachTextInput(void);
ADAPTER_API void IMEUpdateAttribute(const struct ime_input_attribute input_attribute);
ADAPTER_API void IMENotifyCursorUpdate(const struct ime_cursor_info* cursor_info);

/* screenlock */
ADAPTER_API void StartListeningForScreenlock(screenlock_event_callback callback);

/* url */
ADAPTER_API int OpenNativeUrl(const char *url);

/* file */
ADAPTER_API int OpenNativeFileWithChooser(const char *path);

/* print */
#define OHOS_PRINTER_ID_MAX 256
#define OHOS_PRINTER_NAME_MAX 256
#define OHOS_PRINTER_TEXT_MAX 512
#define OHOS_PRINTER_CAPABILITY_MAX 2048

struct ohos_printer_info {
    char id[OHOS_PRINTER_ID_MAX];
    char name[OHOS_PRINTER_NAME_MAX];
    char description[OHOS_PRINTER_TEXT_MAX];
    char make[OHOS_PRINTER_NAME_MAX];
    char uri[OHOS_PRINTER_TEXT_MAX];
    char page_sizes[OHOS_PRINTER_CAPABILITY_MAX];
    char options[OHOS_PRINTER_CAPABILITY_MAX];
    uint32_t status;
    uint32_t color_modes;
    uint32_t duplex_modes;
    uint32_t is_default;
};

ADAPTER_API int GetPrinters(struct ohos_printer_info *printers, uint32_t capacity);
ADAPTER_API int PrintPostScript(const char *printer_id,
                                const char *job_name,
                                const void *data,
                                uint32_t length);
#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif  // OHOS_ADAPTER_API_H_

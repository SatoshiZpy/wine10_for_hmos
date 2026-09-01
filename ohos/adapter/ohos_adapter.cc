// Copyright (c) 2025 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos_adapter.h"

#include <stdint.h>
#include <cstring>
#include <new>
#include <unistd.h>
#include <napi/native_api.h>

#include "ohos/adapter/common/constants.h"
#include "ohos/adapter/common/logging.h"
#include "ohos/adapter/common/mutex.h"
#include "ohos/adapter/cursor/cursor.h"
#include "ohos/adapter/event/event_manager.h"
#include "ohos/adapter/external_protocol/external_protocol_adapter.h"
#include "ohos/adapter/file_manager/file_manager_adapter.h"
#include "ohos/adapter/ime_adapter/input_method_adapter.h"
#include "ohos/adapter/ime_adapter/wine_ime_delegate.h"
#include "ohos/adapter/life_cycle/lifecycle_init.h"
#include "ohos/adapter/multiprocess/app_spawn_communication.h"
#include "ohos/adapter/multiprocess/command_line/command_line_helper.h"
#include "ohos/adapter/ipc_lite/ipc_lite_adapter.h"
#include "ohos/adapter/print/ohos_print_adapter.h"
#include "ohos/adapter/screen/screen_adapter.h"
#include "ohos/adapter/screenlock_monitor/screenlock_monitor_adapter.h"
#include "ohos/adapter/window/window_common.h"
#include "ohos/adapter/window/wine_window_manager.h"
#include "ohos/adapter/xcomponent/adapter/window_adapter.h"
#include "ohos/adapter/xcomponent/xcomponent_manager.h"

EXTERN_C_START

using namespace ohos::adapter;
using namespace ohos::adapter::life_cycle;
using namespace ohos::adapter::xcomponent;
using namespace ohos::adapter::window;
using namespace ohos::adapter::event;
using namespace ohos::adapter::multiprocess;

static Mutex mutex;
static bool has_loaded_ = false;

static window_features_mask get_features_mask(const struct window_features* features)
{
    window_features_mask mask = 0;

    if (features && features->title_bar) {
        mask |= features_mask_for_type(WindowFeatures::WINDOW_TITLE_BAR);
    }

    if (features && features->close_button) {
        mask |= features_mask_for_type(WindowFeatures::WINDOW_CLOSE_BUTTON);
    }

    if (features && features->minimize_button) {
        mask |= features_mask_for_type(WindowFeatures::WINDOW_MINIMIZE_BUTTON);
    }

    if (features && features->maximize_button) {
        mask |= features_mask_for_type(WindowFeatures::WINDOW_MAXIMIZE_BUTTON);
    }

    if (features && features->resizable) {
        mask |= features_mask_for_type(WindowFeatures::WINDOW_RESIZABLE);
    }

    if (features && features->touchable) {
        mask |= features_mask_for_type(WindowFeatures::WINDOW_TOUCHABLE);
    }

    if (features && features->shadow) {
        mask |= features_mask_for_type(WindowFeatures::WINDOW_HAS_SHADOW);
    }

    if (features && features->prevents_app_activation) {
        mask |= features_mask_for_type(WindowFeatures::WINDOW_NO_ACTIVATION);
    }

    if (features && features->use_floating_window) {
        mask |= features_mask_for_type(WindowFeatures::USE_FLOATING_WINDOW);
    }

    if (features && features->use_dark_mode) {
        mask |= features_mask_for_type(WindowFeatures::USE_DARK_MODE);
    }

    LOGD("get_features_mask: %{public}#lx", mask);
    return mask;
}

static window_state_mask get_window_state_mask(const struct window_state* state)
{
    window_state_mask mask = 0;

    if (state && state->disabled) {
        mask |= state_mask_for_type(WindowState::WINDOW_DISABLED);
    }

    if (state && state->no_foreground) {
        mask |= state_mask_for_type(WindowState::WINDOW_NO_FOREGROUND);
    }

    if (state && state->top_most) {
        mask |= state_mask_for_type(WindowState::WINDOW_TOP_MOST);
    }

    if (state && state->excluded_by_control) {
        mask |= state_mask_for_type(WindowState::WINDOW_EXCLUDED_BY_CONTROL);
    }

    if (state && state->minimized) {
        mask |= state_mask_for_type(WindowState::WINDOW_MINIMIZED);
    }

    if (state && state->maximized) {
        mask |= state_mask_for_type(WindowState::WINDOW_MAXIMIZED);
    }

    if (state && state->full_screen) {
        mask |= state_mask_for_type(WindowState::WINDOW_FULL_SCREEN);
    }

    if (state && state->split_mode) {
        mask |= state_mask_for_type(WindowState::WINDOW_SPLIT_MODE);
    }

    if (state && state->visible) {
        mask |= state_mask_for_type(WindowState::WINDOW_VISIBLE);
    }

    LOGD("get_window_state_mask: %{public}#lx", mask);
    return mask;
}

static NewWindowParam Builder(const struct window_param* param)
{
    NewWindowParam win_param;
    win_param.process_key = CommandLineHelper::GetProcessName();
    win_param.ability_type = (AbilityType)param->container;
    win_param.bounds = {param->bounds.left, param->bounds.top, param->bounds.width, param->bounds.height};
    win_param.parent_id = param->parent_id;
    win_param.hwnd = param->hwnd;
    win_param.owner_pid = param->pid;
    win_param.feature_mask = get_features_mask(&param->features);
    win_param.window_state_mask = get_window_state_mask(&param->state);

    // The first window created must be the main window of the main process.
    if (WindowAdapter::GetInstance().GetWindowWidgetId() == USER_DEFAULT_WIDGET_ID &&
            IsMainProcess() &&
            !has_loaded_) {
        if (param->type != WindowInitType::kWindow ) {
            LOGW("first window must be kWindow, overriding type=%{public}d", param->type);
            win_param.window_type = WindowInitType::kWindow;
        }
        win_param.widget_id = USER_DEFAULT_WIDGET_ID;
        has_loaded_ = true;
    } else {
        win_param.window_type = param->type;
        win_param.widget_id = WindowAdapter::GetInstance().NextWindowWidgetId();
    }

    return win_param;
}

static Display GetDisplay()
{
    Display display;
    ScreenAdapter::GetInstance().GetDefaultDisplay(display);
    return display;
}

static void UpdateWindow(int widget_id, const NewWindowParam& param)
{
    MutexLock lock_protect(mutex);
    auto window = WineWindowManager::GetInstance().GetWindow(widget_id);
    if (!window) {
        LOGE("UpdateWindow: window not exists, widgetId %{public}d", widget_id);
        return;
    }
    LOGI("UpdateWindow widgetId %{public}d", widget_id);
}

ohos_window CreateNativeWindow(const struct window_param* param, int* widget_id)
{
    MutexLock lock_protect(mutex);
    if (!param) {
        LOGE("invalid param");
        return nullptr;
    }

    NewWindowParam win_param = Builder(param);
    int create_id = -1;
    auto window = WineWindow::Create(win_param, &create_id);
    if (!window) {
        LOGE("CreateWindow error");
        return nullptr;
    }
    *widget_id = create_id;
    LOGI("CreateNativeWindow widgetId %{public}d", create_id);
    return (ohos_window)window.get();
}

void CloseNativeWindow(int widget_id)
{
    MutexLock lock_protect(mutex);
    auto window = WineWindowManager::GetInstance().GetWindow(widget_id);
    if (!window) {
        LOGE("CloseNativeWindow: window not exists, widgetId %{public}d", widget_id);
        return;
    }
    LOGI("CloseNativeWindow widgetId %{public}d", widget_id);
    window->Close();
    WineWindowManager::GetInstance().RemoveWindow(widget_id);
}

int StartNativeProcess(int argc, char** argv, const struct launch_options* options)
{
    MutexLock lock_protect(mutex);
    if (!options) {
        LOGE("invalid launch_options");
        return -1;
    }
    LOGI("launch process type %{public}d", options->type);
    std::vector<std::string> command;
    for (int i = 0; i < argc && argv[i] != nullptr; ++i) {
        command.emplace_back(argv[i]);
    }
    std::vector<std::pair<int, int>> fds = {};
    for (size_t i = 0; i < MAX_SEND_FDS; i++) {
        int fd = options->fds[i];
        if (fd <= 0) {
            continue;
        }
        int id = options->id[i];
        fds.push_back(std::make_pair(fd, id));
    }
    int pid = ipc_lite::IpcLiteHelper::CreateProcess(command, fds);
    LOGI("launch process type %{public}d, %{public}d", options->type, pid);
    return pid;
}

void SetCustomCursor(int widget_id, int width, int height, int hotspot_x, int hotspot_y, const unsigned int* bits)
{
    if (!bits) {
        LOGE("invalid bits");
        return;
    }

    size_t buff_size = width * height * sizeof(unsigned int);
    std::shared_ptr<char[]> buff = std::make_shared<char[]>(buff_size);
    std::memcpy(buff.get(), bits, buff_size);
    CustomCusorInfo cursor_info{widget_id, width, height, hotspot_x, hotspot_y, buff};
    SetCustomCursor(cursor_info);
}

void SetCursorStyle(int widget_id, int cursor_type)
{
    // cursor_type < 0 means "fall back to the default arrow"; the JS side
    // treats this the same as kDefault. Clamp to the enum range so the
    // arkts side never sees an out-of-range value.
    OhosCursorType type = OhosCursorType::kDefault;
    if (cursor_type >= static_cast<int>(OhosCursorType::kMinValue) &&
        cursor_type <= static_cast<int>(OhosCursorType::kMaxValue)) {
        type = static_cast<OhosCursorType>(cursor_type);
    }
    SetCursor(widget_id, type);
}

void SetWindowBounds(int widget_id, const struct window_bounds* bounds)
{
    MutexLock lock_protect(mutex);
    if (!bounds) {
        LOGE("invalid bounds");
        return;
    }

    auto window = WineWindowManager::GetInstance().GetWindow(widget_id);
    if (!window) {
        LOGE("SetWindowBounds: window not exists, widgetId %{public}d", widget_id);
        return;
    }
    LOGI("SetWindowBounds widgetId %{public}d", widget_id);
    ::WindowRect rect = {bounds->left, bounds->top, bounds->width, bounds->height};
    window->SetBounds(rect);
}

void SetParentWindow(int widget_id, int parent_id)
{
    // todo: impl function
}

void SetWindowFeatures(int widget_id, const struct window_features* wf)
{
    // todo: impl function
}

void SetWindowState(int widget_id, const struct window_state* state)
{
    // todo: impl function
}

void SetWindowAlpha(int widget_id, float opacity)
{
    // todo: impl function
}

void SetWindowTitle(int widget_id, const char* title)
{
    auto window = WineWindowManager::GetInstance().GetWindow(widget_id);
    if (!window) {
        LOGE("SetWindowTitle: window not exists, widgetId %{public}d", widget_id);
        return;
    }
    LOGI("SetWindowTitle widgetId %{public}d", widget_id);
    window->SetTitle(std::string(title));
}

void SetWindowVisible(int widget_id, int visible)
{
    auto window = WineWindowManager::GetInstance().GetWindow(widget_id);
    if (!window) {
        LOGE("SetWindowVisible: window not exists, widgetId %{public}d", widget_id);
        return;
    }
    LOGI("SetWindowVisible widgetId %{public}d, visible %{public}d", widget_id, visible);
    visible ? window->Show() : window->Hide();
}

struct window_bounds GetInitialBounds()
{
    window_bounds bounds = {0};

    if (WindowAdapter::GetInstance().GetWindowWidgetId() > 1) {
        return bounds;
    }

    WindowRect window_rect = WindowAdapter::GetInstance().GetInitialBounds();
    bounds.left = window_rect.left;
    bounds.top = window_rect.top;
    bounds.width = window_rect.width;
    bounds.height = window_rect.height;
    return bounds;
}

ohos_native_window GetNativeWindow(ohos_window w)
{
    WineWindow* window = (WineWindow*)w;
    if (!window) {
        LOGE("invalid wine window");
        return nullptr;
    }
    return window->GetNativeWindow();
}

int GetFocusWindow()
{
    return XComponentManager::GetInstance()->GetActivateWindow();
}

struct window_bounds GetDisplayAvailArea()
{
    auto display = GetDisplay();
    LOGI("GetDisplayAvailArea rect: %{public}d, %{public}d, %{public}d, %{public}d",
         display.avail_area.left, display.avail_area.top,
         display.avail_area.width, display.avail_area.height);
    return window_bounds{.left = display.avail_area.left, .top = display.avail_area.top,
                         .width = display.avail_area.width, .height = display.avail_area.height};
}

int GetSystemDpi()
{
    auto display = GetDisplay();
    if (display.avail_area.width <= 0 || display.avail_area.height <= 0) {
        LOGE("get display physical size error");
        return 96;
    }

    LOGI("get display info: x_dpi=%{public}f, y_dpi=%{public}f, (%{public}d,%{public}d)",
        display.x_dpi, display.y_dpi, display.avail_area.width, display.avail_area.height);

    // Calculate physical size (in inches)
    double width_inch = display.avail_area.width / display.x_dpi;
    double height_inch = display.avail_area.height / display.y_dpi;
    double diag_inch = std::sqrt(width_inch * width_inch + height_inch * height_inch);
    double diag_px = std::sqrt(display.avail_area.width * display.avail_area.width
                               + display.avail_area.height * display.avail_area.height);

    // Calculate physical PPI
    double physical_ppi = diag_px / diag_inch;

    // Determine usage scenario based on screen size and apply correction factor
    double correction_factor = 1.0;
    if (diag_inch <= 15.6) {
        // Laptop screens (12-15.6 inches): scale at 60-70% of physical PPI
        correction_factor = 0.65;
    } else if (diag_inch <= 27) {
        // Desktop monitors (21-27 inches): scale at 80-90% of physical PPI
        correction_factor = 0.85;
    } else {
        // Large-screen TVs (32 inches+): scale at 100% of physical PPI
        correction_factor = 1.0;
    }

    // Calculate effective target DPI (considering viewing distance)
    double effective_target_dpi = physical_ppi * correction_factor;
    int dpi = static_cast<int>(std::round(effective_target_dpi));
    dpi = std::max(96, std::min(240, dpi));

    LOGI("physical PPI: %{public}.2f, size: %{public}.1f inch, factor: %{public}.2f, effective DPI: %{public}d",
        physical_ppi, diag_inch, correction_factor, dpi);

    // Map to Windows standard DPI scaling levels
    if (dpi > 96 && dpi <= 108) dpi = 96;           // 100%
    else if (dpi > 108 && dpi <= 120) dpi = 120;    // 125%
    else if (dpi > 120 && dpi <= 144) dpi = 144;    // 150%
    else if (dpi > 144 && dpi <= 168) dpi = 168;    // 175%
    else if (dpi > 168 && dpi <= 192) dpi = 192;    // 200%
    LOGI("recommended DPI: %{public}d (scale: %{public}.0f%%)", dpi, (dpi * 100.0 / 96));

    return dpi;
}

int GetGlobalDescriptor(int key)
{
    std::vector<std::pair<int, int>> ids_fds_map = AppSpawnCommunication::GetFdIdsRemap();
    for (const auto& [rawfd, remap_fd] : ids_fds_map) {
        LOGI("GetGlobalDescriptor fd %{public}d => %{public}d", rawfd, remap_fd);
        if (remap_fd == key) {
            return rawfd;
        }
    }
    return -1;
}

void KillNativeProcess(int exitcode)
{
    int pid = getpid();
    LOGI("KillNativeProcess pid %{public}d, exitcode %{public}#x", pid, exitcode);
    ipc_lite::IpcLiteHelper::KillProcess();
}

int IsMainProcess()
{
    return CommandLineHelper::IsMainProcess() ? 1 : 0;
}

int GetMainProcessId()
{
    return CommandLineHelper::GetMainProcessId();
}

void RegisterInputEventCallBack(input_event_callback callback)
{
    EventManager::GetInstance().RegisterInputEventCallBack(callback);
}

void RegisterWindowEventCallBack(input_event_callback callback)
{
    EventManager::GetInstance().RegisterWindowEventCallBack(callback);
}

void RegisterIMEEventCallBack(ime_event_callback callback)
{
    if (callback == nullptr) {
        LOGE("ime_event_callback is nullptr!");
        return;
    }
    WineIMEDelegate::GetInstance().RegisterIMEEventCallBack(callback);
}

void IMEAttachTextInput(bool show_keyboard, const struct ime_text_config* text_config, int32_t request_keyboard_reason)
{
    if (text_config == nullptr) {
        LOGE("text_config is nullptr!");
        return;
    }
    ohos::adapter::IMFAdapterTextConfig config;
    config.inputAttribute.inputPattern = text_config->input_attribute.input_pattern;
    config.inputAttribute.enterKeyType = text_config->input_attribute.enter_key_type;
    config.cursorInfo.left = text_config->cursor_info.left;
    config.cursorInfo.top = text_config->cursor_info.top;
    config.cursorInfo.width = text_config->cursor_info.width;
    config.cursorInfo.height = text_config->cursor_info.height;
    config.range.start = text_config->range.start;
    config.range.end = text_config->range.end;
    InputMethodAdapter::GetInstance().AttachTextInput(show_keyboard, config, request_keyboard_reason);
}

void IMEDetachTextInput()
{
    InputMethodAdapter::GetInstance().DetachTextInput();
}

void IMEUpdateAttribute(const struct ime_input_attribute input_attribute)
{
    IMFAdapterInputAttribute attribute;
    attribute.inputPattern = input_attribute.input_pattern;
    attribute.enterKeyType = input_attribute.enter_key_type;
    InputMethodAdapter::GetInstance().UpdateAttribute(attribute);
}

void IMENotifyCursorUpdate(const struct ime_cursor_info* cursor_info)
{
    if (cursor_info == nullptr) {
        LOGE("cursor_info is nullptr!");
        return;
    }
    ohos::adapter::IMFAdapterCursorInfo info;
    info.left = cursor_info->left;
    info.top = cursor_info->top;
    info.width = cursor_info->width;
    info.height = cursor_info->height;
    InputMethodAdapter::GetInstance().NotifyCursorUpdate(info);
}

void StartListeningForScreenlock(screenlock_event_callback callback)
{
    if (callback == nullptr) {
        LOGE("screenlock_event_callback is nullptr!");
        return;
    }
    ScreenlockMonitorAdapter::GetInstance().StartListeningForScreenlock(callback);
}

int OpenNativeUrl(const char *url)
{
    LOGI("OpenNativeUrl: url=%{public}s", url ? url : "(null)");
    if (!url) {
        LOGE("OpenNativeUrl: url is nullptr");
        return -1;
    }
    ohos::adapter::ExternalProtocolAdapter::GetInstance().OpenExternal(std::string(url));
    return 0;
}

int OpenNativeFileWithChooser(const char *path)
{
    LOGI("OpenNativeFileWithChooser: path=%{public}s", path ? path : "(null)");
    if (!path) {
        LOGE("OpenNativeFileWithChooser: path is nullptr");
        return -1;
    }
    ohos::adapter::FileManagerAdapter::GetInstance().OpenVerifiedItemWithChooser(std::string(path));
    return 0;
}

int GetPrinters(struct ohos_printer_info *printers, uint32_t capacity)
{
    return ohos::adapter::print::PrintAdapter::GetInstance().GetPrinters(printers, capacity);
}

int PrintPostScript(const char *printer_id, const char *job_name,
                        const void *data, uint32_t length)
{
    if (!data || !length) {
        LOGE("PrintPostScript: invalid input, data=%{public}p length=%{public}u",
            data, length);
        return -1;
    }
    std::shared_ptr<char[]> buffer(new (std::nothrow) char[length]);
    if (!buffer) {
        LOGE("PrintPostScript: buffer allocation failed, length=%{public}u", length);
        return -1;
    }
    std::memcpy(buffer.get(), data, length);
    return ohos::adapter::print::PrintAdapter::GetInstance().PrintPostScript(
        printer_id ? printer_id : "", job_name ? job_name : "", buffer, length) ? 0 : -1;
}
EXTERN_C_END

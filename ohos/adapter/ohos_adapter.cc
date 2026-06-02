// Copyright (c) 2025 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos_adapter.h"

#include <stdint.h>
#include <unistd.h>
#include <napi/native_api.h>

#include "ohos/adapter/common/constants.h"
#include "ohos/adapter/common/logging.h"
#include "ohos/adapter/common/mutex.h"
#include "ohos/adapter/cursor/cursor.h"
#include "ohos/adapter/event/event_manager.h"
#include "ohos/adapter/external_protocol/external_protocol_adapter.h"
#include "ohos/adapter/ime_adapter/input_method_adapter.h"
#include "ohos/adapter/ime_adapter/wine_ime_delegate.h"
#include "ohos/adapter/life_cycle/lifecycle_init.h"
#include "ohos/adapter/multiprocess/app_spawn_communication.h"
#include "ohos/adapter/multiprocess/child_process/child_process.h"
#include "ohos/adapter/multiprocess/child_process/child_process_helper.h"
#include "ohos/adapter/multiprocess/command_line/command_line_helper.h"
#include "ohos/adapter/screenlock_monitor/screenlock_monitor_adapter.h"
#include "ohos/adapter/window/wine_window_manager.h"
#include "ohos/adapter/xcomponent/xcomponent_manager.h"

EXTERN_C_START

using namespace ohos::adapter;
using namespace ohos::adapter::life_cycle;
using namespace ohos::adapter::xcomponent;
using namespace ohos::adapter::multiprocess;
using namespace ohos::adapter::window;
using namespace ohos::adapter::event;

static Mutex mutex;

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

static ipc::WindowParam Builder(const struct window_param* param)
{
    ipc::WindowParam parameter;
    parameter.ability_type = param->container;
    parameter.window_type = param->type;
    parameter.left = param->bounds.left;
    parameter.top = param->bounds.top;
    parameter.width = param->bounds.width;
    parameter.height = param->bounds.height;
    parameter.parent_id = param->parent_id;
    parameter.hwnd = param->hwnd;
    parameter.owner_pid = param->pid;
    parameter.features = get_features_mask(&param->features);
    parameter.window_state = get_window_state_mask(&param->state);
    return parameter;
}

static ipc::DisplayParam GetDisplay()
{
    ipc::DisplayParam display;
    if (!IsMainProcess()) {
        ChildProcess::Instance().GetDisplay(display);
    } else {
        ChildProcessHelper::GetDisplay(display);
    }
    return display;
}

static void UpdateWindow(int widget_id, const ipc::WindowParam& param)
{
    MutexLock lock_protect(mutex);
    if (!IsMainProcess()) {
        // todo: Implement function in a child process
        LOGW("performed in a child process is not supported");
        return;
    }
    ChildProcessHelper::UpdateWindow(widget_id, param);
}

ohos_window CreateNativeWindow(const struct window_param* param, int* widget_id)
{
    MutexLock lock_protect(mutex);
    if (!param) {
        LOGE("invalid param");
        return nullptr;
    }

    if (!IsMainProcess()) {
        // todo: Implement function in a child process
        LOGW("performed in a child process is not supported");
        return nullptr;
    }

    ipc::WindowParam init_param = Builder(param);
    int create_id = ChildProcessHelper::CreateWindow(std::move(init_param));
    if (create_id <= 0) {
        return nullptr;
    }
    auto window = WineWindowManager::GetInstance().GetWindow(create_id);
    *widget_id = create_id;
    return (ohos_window)window.get();
}

void CloseNativeWindow(int widget_id)
{
    MutexLock lock_protect(mutex);
    if (!IsMainProcess()) {
        // todo: Implement function in a child process
        LOGW("performed in a child process is not supported");
        return;
    }
    ChildProcessHelper::CloseWindow(widget_id);
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
    if (IsMainProcess()) {
        int pid = ChildProcessHelper::CreateProcess(command, fds);
        LOGI("launch process type %{public}d, %{public}d", options->type, pid);
        return pid;
    }
    return ChildProcess::Instance().CreateProcess(command, fds);
}

void SetCustomCursor(int widget_id, int width, int height, int hotspot_x, int hotspot_y, const unsigned int* bits)
{
    if (!bits) {
        LOGE("invalid bits");
        return;
    }

    if (!IsMainProcess()) {
        // todo: Implement function in a child process
        LOGW("performed in a child process is not supported");
        return;
    }

    size_t buff_size = width * height * sizeof(unsigned int);
    std::shared_ptr<char[]> buff = std::make_shared<char[]>(buff_size);
    std::memcpy(buff.get(), bits, buff_size);
    CustomCusorInfo cursor_info{widget_id, width, height, hotspot_x, hotspot_y, buff};
    SetCustomCursor(cursor_info);
}

void SetWindowBounds(int widget_id, const struct window_bounds* bounds)
{
    MutexLock lock_protect(mutex);
    if (!bounds) {
        LOGE("invalid bounds");
        return;
    }
    if (!IsMainProcess()) {
        // todo: Implement function in a child process
        LOGW("performed in a child process is not supported");
        return;
    }

    WindowRect rect = {bounds->left, bounds->top, bounds->width, bounds->height};
    ChildProcessHelper::SetWindowBounds(widget_id, std::move(rect));
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
    if (!IsMainProcess()) {
        // todo: Implement function in a child process
        LOGW("performed in a child process is not supported");
        return;
    }
    ChildProcessHelper::SetWindowTitle(widget_id, title);
}

void SetWindowVisible(int widget_id, int visible)
{
    if (!IsMainProcess()) {
        // todo: Implement function in a child process
        LOGW("performed in a child process is not supported");
        return;
    }
    ChildProcessHelper::SetWindowVisible(widget_id, visible);
}

struct window_bounds GetInitialBounds()
{
    window_bounds bounds = {0};
    if (!IsMainProcess()) {
        LOGW("performed in a child process is not supported");
        return bounds;
    }

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
    if (!IsMainProcess()) {
        // todo: Implement function in a child process
        LOGW("performed in a child process is not supported");
        return nullptr;
    }
    WineWindow* window = (WineWindow*)w;
    if (!window) {
        LOGE("invalid wine window");
        return nullptr;
    }
    return window->GetNativeWindow();
}

int GetFocusWindow()
{
    if (!IsMainProcess()) {
        // todo: Implement function in a child process
        LOGW("performed in a child process is not supported");
        return -1;
    }
    return XComponentManager::GetInstance()->GetActivateWindow();
}

struct window_bounds GetDisplayAvailArea()
{
    auto display = GetDisplay();
    LOGI("GetDisplayAvailArea rect: %{public}d, %{public}d, %{public}d, %{public}d", display.left, display.top,
         display.width, display.height);
    return window_bounds{.left = display.left, .top = display.top, .width = display.width, .height = display.height};
}

int GetSystemDpi()
{
    auto display = GetDisplay();
    if (display.width <= 0 || display.height <= 0) {
        LOGE("get display physical size error");
        return 96;
    }

    LOGI("get display info: x_dpi=%{public}f, y_dpi=%{public}f, (%{public}d,%{public}d)",
        display.x_dpi, display.y_dpi, display.width, display.height);

    // Calculate physical size (in inches)
    double width_inch = display.width / display.x_dpi;
    double height_inch = display.height / display.y_dpi;
    double diag_inch = std::sqrt(width_inch * width_inch + height_inch * height_inch);
    double diag_px = std::sqrt(display.width * display.width + display.height * display.height);

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
    auto ids_fds_map = AppSpawnCommunication::GetFdIdsRemap();
    for (const auto& [rawfd, remap_fd] : ids_fds_map) {
        LOGI("GetGlobalDescriptor fd %{public}d => %{public}d", rawfd, remap_fd);
        if (remap_fd == key) {
            return rawfd;
        }
    }
    return -1;
}

void KillNativeProcess(int exitcode, int force)
{
    int pid = getpid();
    if (!IsMainProcess()) {
        if (ChildProcess::Instance().KillProcess(pid) != 0) {
            LOGW("kill child pid %{public}d error, fallback to call exit()", pid);
            force ? _exit(exitcode) : exit(exitcode);
        }
        return;
    }

    int main_pid = GetMainProcessId();
    if (pid != main_pid) {
        LOGW("pid not match main pid:(%{public}d,%{public}d)", pid, main_pid);
    }
    LOGI("pid %{public}d, exitcode %{public}#x", pid, exitcode);
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
    LOGI("OpenNativeUrl: url=%{public}s, isMainProcess=%{public}d", url ? url : "(null)", IsMainProcess());
    if (!url) {
        LOGE("OpenNativeUrl: url is nullptr");
        return -1;
    }

    if (!IsMainProcess()) {
        LOGI("OpenNativeUrl: forwarding to main process via IPC");
        int ret = ChildProcess::Instance().OpenUrl(std::string(url));
        LOGI("OpenNativeUrl: IPC OpenUrl ret=%{public}d", ret);
        return ret;
    }

    ohos::adapter::ExternalProtocolAdapter::GetInstance().OpenExternal(std::string(url));
    return 0;
}

EXTERN_C_END

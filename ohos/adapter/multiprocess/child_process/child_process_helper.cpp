/*
 * Copyright (c) 2023-2026 Huawei Device Co., Ltd.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "ohos/adapter/multiprocess/child_process/child_process_helper.h"

#include <IPCKit/ipc_cskeleton.h>
#include <chrono>
#include <future>
#include <string>
#include <string_view>

#include "ohos/adapter/common/logging.h"
#include "ohos/adapter/common/mutex.h"
#include "ohos/adapter/common/trace.h"
#include "ohos/adapter/external_protocol/external_protocol_adapter.h"
#include "ohos/adapter/multiprocess/child_process/child_process_host.h"
#include "ohos/adapter/multiprocess/command_line/command_line_helper.h"
#include "ohos/adapter/multiprocess/isolate_process/native_child_process_configs_wrapper.h"
#include "ohos/adapter/multiprocess/isolate_process/native_child_process_library.h"
#include "ohos/adapter/multiprocess/isolate_process/native_childprocess_args_wrapper.h"
#include "ohos/adapter/screen/screen_adapter.h"
#include "ohos/adapter/window/wine_window_manager.h"

using namespace std::literals;
using namespace ohos::adapter;
using namespace ohos::adapter::window;

namespace ohos::adapter::multiprocess {
static NativeChildProcessLibrary nativeChildProcessLibrary;
const std::string_view LIB_ADAPTOR_ENRTY = "libadapter.so";
static Mutex mutex;
static std::promise<int> createProcessPid GUARDED_BY(mutex);

constexpr int32_t DEFAULT_SCREEN_WIDTH = 1024;
constexpr int32_t DEFAULT_SCREEN_HEIGHT = 768;

std::string GetProcessName(const std::string& image_file)
{
    std::string process_name = image_file;
    size_t lastSlash = process_name.rfind('\\');
    if (lastSlash != std::string::npos) {
        process_name = process_name.substr(lastSlash + 1);
    }
    size_t lastDot = process_name.rfind('.');
    if (lastDot != std::string::npos) {
        return process_name.substr(0, lastDot);
    }
    return process_name;
}

void OnNativeChildProcessStarted(int errCode, OHIPCRemoteProxy* remoteProxy)
{
    int pid = OH_IPCSkeleton_GetCallingPid();
    LOGI("OnNativeChildProcessStarted %{public}d", pid);

    ChildProcessHost::Instance().OnProcessCreated(pid, remoteProxy);
    createProcessPid.set_value(pid);
}

void OnNativeChildProcessExited(int32_t pid, int32_t signal)
{
    LOGI("OnNativeChildProcessExited %{public}d, %{public}d", pid, signal);

    ChildProcessHost::Instance().OnProcessDestroyed(pid);
}

int ChildProcessHelper::CreateProcess(const std::vector<std::string>& command,
                                      const std::vector<std::pair<int, int>>& fds)
{
    int32_t errCode = 0;
    if (nativeChildProcessLibrary.SupportsCreateChildProcessWithConfigs() && !command.empty()) {
        NativeChildProcessConfigsWrapper configs(nativeChildProcessLibrary);
        std::string process_loader = command[0];
        std::string image_file = process_loader == "wineserver" ? process_loader : command[1];
        std::string process_name = GetProcessName(image_file);
        errCode = nativeChildProcessLibrary.SetProcessName(configs.Get(), process_name.c_str());
        if (errCode != 0) {
            LOGE("SetProcessName:%{public}s, error: %{public}d", process_name.c_str(), errCode);
        }
        errCode = nativeChildProcessLibrary.CreateChildProcessWithConfigs(LIB_ADAPTOR_ENRTY.data(), configs.Get(),
                                                                          &OnNativeChildProcessStarted);
        if (errCode != 0) {
            LOGE("CreateChildProcessWithConfigs error: %{public}d", errCode);
        }
        std::future<int> future;
        std::future_status status;
        future = createProcessPid.get_future();
        status = future.wait_for(5s);
        std::promise<int> newPromise;
        createProcessPid.swap(newPromise);

        if (status == std::future_status::ready) {
            int pid = future.get();
            ChildProcessHost::ChildProcessInfo info{command, fds};
            ChildProcessHost::Instance().SetProcessInfo(pid, info);
            return pid;
        }

        LOGE("CreateChildProcessWithConfigs error: time out");
        return -1;
    }
    LOGE("CreateChildProcessWithConfigs error: not support");
    return -1;
}

int ChildProcessHelper::KillProcess(int32_t pid)
{
    int32_t errCode = nativeChildProcessLibrary.KillChildProcess(pid);
    if (errCode != 0) {
        LOGE("KillChildProcess error: %{public}d", errCode);
        return -1;
    }
    return 0;
}

int ChildProcessHelper::CreateWindow(const NewWindowParam& param)
{
    LOGI("CreateWindow type: %{public}d, rect: (%{public}d,%{public}d,%{public}d,%{public}d)",
        param.window_type, param.bounds.left, param.bounds.top, param.bounds.width, param.bounds.height);

    int widget_id = -1;
    auto window = WineWindow::Create(param, &widget_id);
    if (!window) {
        LOGE("create window error");
        return -1;
    }
    LOGI("CreateWindow windowId %{public}d", widget_id);
    return widget_id;
}

int ChildProcessHelper::UpdateWindow(int32_t widget_id, const NewWindowParam& param)
{
    auto window = WineWindowManager::GetInstance().GetWindow(widget_id);
    if (!window) {
        LOGE("window is not exists, windowId: %{public}d", widget_id);
        return -1;
    }
    LOGI("UpdateWindow windowId %{public}d", widget_id);
    return 0;
}

int ChildProcessHelper::SetWindowBounds(int32_t widget_id, const WindowRect& rect)
{
    auto window = WineWindowManager::GetInstance().GetWindow(widget_id);
    if (!window) {
        LOGE("window is not exists, windowId: %{public}d", widget_id);
        return -1;
    }

    LOGI("SetWindowBounds windowId %{public}d", widget_id);
    window->SetBounds(rect);
    return 0;
}

int ChildProcessHelper::SetWindowVisible(int widget_id, int visible)
{
    auto window = WineWindowManager::GetInstance().GetWindow(widget_id);
    if (!window) {
        LOGE("window is not exists, windowId: %{public}d", widget_id);
        return -1;
    }
    LOGI("SetWindowVisible windowId %{public}d, visible %{public}d", widget_id, visible);
    visible ? window->Show() : window->Hide();
    return 0;
}

int ChildProcessHelper::CloseWindow(int32_t widget_id)
{
    auto window = WineWindowManager::GetInstance().GetWindow(widget_id);
    if (!window) {
        LOGE("window is not exists, windowId: %{public}d", widget_id);
        return -1;
    }
    LOGI("CloseWindow windowId: %{public}d", widget_id);
    window->Close();
    return 0;
}

int ChildProcessHelper::SetWindowTitle(int widget_id, const std::string& title)
{
    auto window = WineWindowManager::GetInstance().GetWindow(widget_id);
    if (!window) {
        LOGE("window is not exists, windowId: %{public}d", widget_id);
        return -1;
    }
    LOGI("SetWindowTitle windowId: %{public}d", widget_id);
    window->SetTitle(title);
    return 0;
}

int ChildProcessHelper::GetDisplay(ipc::DisplayParam& param)
{
    Display default_display;
    ScreenAdapter::GetInstance().GetDefaultDisplay(default_display);
    if (default_display.id == -1) {
      LOGE("get default display failed");
      return -1;
    }
    param.id = default_display.id;
    param.name = default_display.name;
    param.left = default_display.avail_area.left;
    param.top = default_display.avail_area.top;
    param.width = default_display.avail_area.width;
    param.height = default_display.avail_area.height;
    param.density_dpi = default_display.density_dpi;
    param.x_dpi = default_display.x_dpi;
    param.y_dpi = default_display.y_dpi;
    return 0;
}

int ChildProcessHelper::OpenUrl(const std::string& url)
{
    ohos::adapter::ExternalProtocolAdapter::GetInstance().OpenExternal(url);
    return 0;
}

} // namespace ohos::adapter::multiprocess

// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_NEW_IPC_IPC_TYPES_H_
#define OHOS_ADAPTER_NEW_IPC_IPC_TYPES_H_

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "ohos/adapter/new_ipc/ipc_parcel.h"

namespace ohos::adapter::new_ipc {

struct FileDescriptor {
    int fd = 0;
    int32_t remap = 0;

    static FileDescriptor FromParcel(IpcParcel& parcel) {
        FileDescriptor desc;
        parcel.ReadInt32(desc.fd);
        parcel.ReadInt32(desc.remap);
        return desc;
    }

    bool ToParcel(IpcParcel& parcel) const {
        parcel.WriteInt32(fd);
        parcel.WriteInt32(remap);
        return true;
    }
};

struct WindowParam {
    int32_t ability_type = 0;
    int32_t window_type = 0;
    int32_t left = 0;
    int32_t top = 0;
    int32_t width = 0;
    int32_t height = 0;
    int32_t parent_id = -1;
    int32_t hwnd = -1;
    int32_t owner_pid = -1;
    int64_t features = 0;
    int64_t window_state = 0;

    static WindowParam FromParcel(IpcParcel& parcel) {
        WindowParam param;
        parcel.ReadInt32(param.ability_type);
        parcel.ReadInt32(param.window_type);
        parcel.ReadInt32(param.left);
        parcel.ReadInt32(param.top);
        parcel.ReadInt32(param.width);
        parcel.ReadInt32(param.height);
        parcel.ReadInt32(param.parent_id);
        parcel.ReadInt32(param.hwnd);
        parcel.ReadInt32(param.owner_pid);
        parcel.ReadInt64(param.features);
        parcel.ReadInt64(param.window_state);
        return param;
    }

    bool ToParcel(IpcParcel& parcel) const {
        parcel.WriteInt32(ability_type);
        parcel.WriteInt32(window_type);
        parcel.WriteInt32(left);
        parcel.WriteInt32(top);
        parcel.WriteInt32(width);
        parcel.WriteInt32(height);
        parcel.WriteInt32(parent_id);
        parcel.WriteInt32(hwnd);
        parcel.WriteInt32(owner_pid);
        parcel.WriteInt64(features);
        parcel.WriteInt64(window_state);
        return true;
    }
};

struct DisplayParam {
    int32_t id = -1;
    std::string name;
    int32_t left = 0;
    int32_t top = 0;
    int32_t width = 0;
    int32_t height = 0;
    float density_dpi = 0.0f;
    float x_dpi = 0.0f;
    float y_dpi = 0.0f;

    static DisplayParam FromParcel(IpcParcel& parcel) {
        DisplayParam param;
        parcel.ReadInt32(param.id);
        parcel.ReadString(param.name);
        parcel.ReadInt32(param.left);
        parcel.ReadInt32(param.top);
        parcel.ReadInt32(param.width);
        parcel.ReadInt32(param.height);
        parcel.ReadFloat(param.density_dpi);
        parcel.ReadFloat(param.x_dpi);
        parcel.ReadFloat(param.y_dpi);
        return param;
    }

    bool ToParcel(IpcParcel& parcel) const {
        parcel.WriteInt32(id);
        parcel.WriteString(name);
        parcel.WriteInt32(left);
        parcel.WriteInt32(top);
        parcel.WriteInt32(width);
        parcel.WriteInt32(height);
        parcel.WriteFloat(density_dpi);
        parcel.WriteFloat(x_dpi);
        parcel.WriteFloat(y_dpi);
        return true;
    }
};

struct WindowAttr {
    int32_t height = 0;
    int32_t width = 0;
    int32_t format = 0;
    int32_t stride = 0;
    int32_t swapInterval = 0;
    int32_t timeout = 0;
    int32_t colorGamut = 0;
    int32_t transform = 0;
    int32_t sourceType = 0;
    int64_t usage = 0;
    std::string app_framework_type;

    static WindowAttr FromParcel(IpcParcel& parcel) {
        WindowAttr attr;
        parcel.ReadInt32(attr.height);
        parcel.ReadInt32(attr.width);
        parcel.ReadInt32(attr.format);
        parcel.ReadInt32(attr.stride);
        parcel.ReadInt32(attr.swapInterval);
        parcel.ReadInt32(attr.timeout);
        parcel.ReadInt32(attr.colorGamut);
        parcel.ReadInt32(attr.transform);
        parcel.ReadInt32(attr.sourceType);
        parcel.ReadInt64(attr.usage);
        parcel.ReadString(attr.app_framework_type);
        return attr;
    }

    bool ToParcel(IpcParcel& parcel) const {
        parcel.WriteInt32(height);
        parcel.WriteInt32(width);
        parcel.WriteInt32(format);
        parcel.WriteInt32(stride);
        parcel.WriteInt32(swapInterval);
        parcel.WriteInt32(timeout);
        parcel.WriteInt32(colorGamut);
        parcel.WriteInt32(transform);
        parcel.WriteInt32(sourceType);
        parcel.WriteInt64(usage);
        parcel.WriteString(app_framework_type);
        return true;
    }
};

struct WindowRect {
    int32_t left = 0;
    int32_t top = 0;
    int32_t width = 0;
    int32_t height = 0;

    static WindowRect FromParcel(IpcParcel& parcel) {
        WindowRect rect;
        parcel.ReadInt32(rect.left);
        parcel.ReadInt32(rect.top);
        parcel.ReadInt32(rect.width);
        parcel.ReadInt32(rect.height);
        return rect;
    }

    bool ToParcel(IpcParcel& parcel) const {
        parcel.WriteInt32(left);
        parcel.WriteInt32(top);
        parcel.WriteInt32(width);
        parcel.WriteInt32(height);
        return true;
    }
};

struct ProcessInfo {
    std::vector<std::string> commands;
    std::vector<std::pair<int, int>> fds;

    static ProcessInfo FromParcel(IpcParcel& parcel) {
        ProcessInfo info;
        int32_t cmd_count = 0;
        parcel.ReadInt32(cmd_count);
        for (int32_t i = 0; i < cmd_count; i++) {
            std::string cmd;
            parcel.ReadString(cmd);
            info.commands.push_back(cmd);
        }
        int32_t fd_count = 0;
        parcel.ReadInt32(fd_count);
        for (int32_t i = 0; i < fd_count; i++) {
            FileDescriptor desc = FileDescriptor::FromParcel(parcel);
            info.fds.push_back({desc.fd, desc.remap});
        }
        return info;
    }

    bool ToParcel(IpcParcel& parcel) const {
        parcel.WriteInt32(static_cast<int32_t>(commands.size()));
        for (const auto& cmd : commands) {
            parcel.WriteString(cmd);
        }
        parcel.WriteInt32(static_cast<int32_t>(fds.size()));
        for (const auto& [fd, remap] : fds) {
            FileDescriptor desc = {fd, remap};
            desc.ToParcel(parcel);
        }
        return true;
    }
};

enum IProcessManagerIpcCode : uint32_t {
    COMMAND_CREATE_PROCESS  = 1,
    COMMAND_KILL_PROCESS    = 2,
    COMMAND_CREATE_WINDOW   = 3,
    COMMAND_SET_BOUNDS      = 4,
    COMMAND_SET_VISIBLE     = 5,
    COMMAND_UPDATE_WINDOW   = 6,
    COMMAND_CLOSE_WINDOW    = 7,
    COMMAND_GET_DISPLAY     = 8,
    COMMAND_OPEN_URL        = 9,
    COMMAND_SET_WINDOW_TITLE = 10,
    COMMAND_GET_FD_IDS_REMAP = 11,
    COMMAND_GET_PID          = 12,
};

enum IChildProcessIpcCode : uint32_t {
    COMMAND_SET_PROCESS_INFO = 1,
    COMMAND_ECHO             = 2,
    COMMAND_SET_WINDOW_INFO  = 3,
    COMMAND_SEND_INPUT_EVENT = 4,
};

enum IpcRequestType : uint32_t {
    REQUEST_PROCESS_MANAGER = 1,
    REQUEST_CHILD_PROCESS   = 2,
};

}  // namespace ohos::adapter::new_ipc

#endif  // OHOS_ADAPTER_NEW_IPC_IPC_TYPES_H_
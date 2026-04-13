/*
 * Copyright (c) 2023-2025 Huawei Device Co., Ltd.
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
 
#include "native_child_process_library.h"
 
#include "ohos/adapter/common/logging.h"
 
namespace ohos::adapter::multiprocess {
 
NativeChildProcessLibrary::NativeChildProcessLibrary()
    : lib_child_process_("child_process") {
  if (lib_child_process_.IsLoaded()) {
    lib_child_process_.LoadFunction(&start_native_child_process_func_,
                                    "OH_Ability_StartNativeChildProcess");
    lib_child_process_.LoadFunction(
        &start_native_child_process_with_config_func_,
        "OH_Ability_StartNativeChildProcessWithConfigs");
    lib_child_process_.LoadFunction(&create_child_process_configs_func_,
                                    "OH_Ability_CreateChildProcessConfigs");
    lib_child_process_.LoadFunction(&destroy_child_process_configs_func_,
                                    "OH_Ability_DestroyChildProcessConfigs");
    lib_child_process_.LoadFunction(
        &set_process_name_func_, "OH_Ability_ChildProcessConfigs_SetProcessName");
    lib_child_process_.LoadFunction(
        &set_isolation_mode_func_,
        "OH_Ability_ChildProcessConfigs_SetIsolationMode");
    lib_child_process_.LoadFunction(
        &create_child_process_with_configs_func_,
        "OH_Ability_CreateNativeChildProcessWithConfigs");
    lib_child_process_.LoadFunction(
        &register_native_child_process_exit_callback_func_,
        "OH_Ability_RegisterNativeChildProcessExitCallback");
    lib_child_process_.LoadFunction(
        &unregister_native_child_process_exit_callback_func_,
        "OH_Ability_UnregisterNativeChildProcessExitCallback");
    lib_child_process_.LoadFunction(&kill_child_process_func_, "OH_Ability_KillChildProcess");
 
    supports_start_child_process_ = start_native_child_process_func_ != nullptr;
    supports_start_child_process_with_configs_ =
        start_native_child_process_with_config_func_ &&
        create_child_process_configs_func_ &&
        destroy_child_process_configs_func_ && set_process_name_func_ &&
        set_isolation_mode_func_;
    supports_create_child_process_with_configs_ =
        create_child_process_with_configs_func_ &&
        create_child_process_configs_func_ &&
        destroy_child_process_configs_func_ && set_process_name_func_ &&
        set_isolation_mode_func_;
    supports_native_child_process_exit_callback_ =
        register_native_child_process_exit_callback_func_ &&
        unregister_native_child_process_exit_callback_func_;
  }
}
 
__attribute__((no_sanitize("cfi", "cfi-icall")))
Ability_NativeChildProcess_ErrCode
NativeChildProcessLibrary::StartNativeChildProcess(
    const char* entry,
    NativeChildProcess_Args args,
    NativeChildProcess_Options options,
    int32_t* pid) {
  if (!start_native_child_process_func_) {
    LOGE("OH_Ability_StartNativeChildProcess not loaded.");
    return NCP_ERR_NOT_SUPPORTED;
  }
  return start_native_child_process_func_(entry, args, options, pid);
}
 
__attribute__((no_sanitize("cfi", "cfi-icall")))
Ability_NativeChildProcess_ErrCode
NativeChildProcessLibrary::StartNativeChildProcessWithConfigs(
    const char* entry,
    NativeChildProcess_Args args,
    Ability_ChildProcessConfigs* configs,
    int32_t* pid) {
  if (!start_native_child_process_with_config_func_) {
    LOGE("OH_Ability_StartNativeChildProcessWithConfigs not loaded.");
    return NCP_ERR_NOT_SUPPORTED;
  }
  return start_native_child_process_with_config_func_(entry, args, configs,
                                                      pid);
}
 
__attribute__((no_sanitize("cfi", "cfi-icall")))
Ability_ChildProcessConfigs*
NativeChildProcessLibrary::CreateChildProcessConfigs() {
  if (!create_child_process_configs_func_) {
    LOGE("OH_Ability_CreateChildProcessConfigs not loaded.");
    return nullptr;
  }
  return create_child_process_configs_func_();
}
 
__attribute__((no_sanitize("cfi", "cfi-icall")))
void NativeChildProcessLibrary::DestroyChildProcessConfigs(
    Ability_ChildProcessConfigs* configs) {
  if (!destroy_child_process_configs_func_) {
    LOGE("OH_Ability_DestroyChildProcessConfigs not loaded.");
    return;
  }
  destroy_child_process_configs_func_(configs);
}
 
__attribute__((no_sanitize("cfi", "cfi-icall")))
Ability_NativeChildProcess_ErrCode NativeChildProcessLibrary::SetProcessName(
    Ability_ChildProcessConfigs* configs,
    const char* process_name) {
  if (!set_process_name_func_) {
    LOGE("OH_Ability_ChildProcessConfigs_SetProcessName not loaded.");
    return NCP_ERR_NOT_SUPPORTED;
  }
  return set_process_name_func_(configs, process_name);
}
 
__attribute__((no_sanitize("cfi", "cfi-icall")))
Ability_NativeChildProcess_ErrCode NativeChildProcessLibrary::SetIsolationMode(
    Ability_ChildProcessConfigs* configs,
    NativeChildProcess_IsolationMode isolation_mode) {
  if (!set_isolation_mode_func_) {
    LOGE("OH_Ability_ChildProcessConfigs_SetIsolationMode not loaded.");
    return NCP_ERR_NOT_SUPPORTED;
  }
  return set_isolation_mode_func_(configs, isolation_mode);
}
 
__attribute__((no_sanitize("cfi", "cfi-icall")))
Ability_NativeChildProcess_ErrCode
NativeChildProcessLibrary::CreateChildProcessWithConfigs(
    const char* lib_name,
    Ability_ChildProcessConfigs* configs,
    OH_Ability_OnNativeChildProcessStarted on_process_started) {
  if (!create_child_process_with_configs_func_) {
    LOGE("OH_Ability_CreateNativeChildProcessWithConfigs not loaded.");
    return NCP_ERR_NOT_SUPPORTED;
  }
  return create_child_process_with_configs_func_(lib_name, configs,
                                                 on_process_started);
}
 
__attribute__((no_sanitize("cfi", "cfi-icall")))
Ability_NativeChildProcess_ErrCode
NativeChildProcessLibrary::RegisterNativeChildProcessExitCallback(
    OH_Ability_OnNativeChildProcessExit callback) {
  if (!register_native_child_process_exit_callback_func_) {
    LOGE("OH_Ability_RegisterNativeChildProcessExitCallback not loaded.");
    return NCP_ERR_NOT_SUPPORTED;
  }
  return register_native_child_process_exit_callback_func_(callback);
}
 
__attribute__((no_sanitize("cfi", "cfi-icall")))
Ability_NativeChildProcess_ErrCode
NativeChildProcessLibrary::UnregisterNativeChildProcessExitCallback(
    OH_Ability_OnNativeChildProcessExit callback) {
  if (!unregister_native_child_process_exit_callback_func_) {
    LOGE("OH_Ability_UnregisterNativeChildProcessExitCallback not loaded.");
    return NCP_ERR_NOT_SUPPORTED;
  }
  return unregister_native_child_process_exit_callback_func_(callback);
}

__attribute__((no_sanitize("cfi", "cfi-icall")))
Ability_NativeChildProcess_ErrCode
NativeChildProcessLibrary::KillChildProcess(int32_t pid) {
  if (!kill_child_process_func_) {
    LOGE("OH_Ability_KillChildProcess not loaded.");
    return NCP_ERR_NOT_SUPPORTED;
  }
  return kill_child_process_func_(pid);
}

bool NativeChildProcessLibrary::SupportsStartChildProcess() {
  return supports_start_child_process_;
}
 
bool NativeChildProcessLibrary::SupportsStartChildProcessWithConfigs() {
  return supports_start_child_process_with_configs_;
}
 
bool NativeChildProcessLibrary::SupportsCreateChildProcessWithConfigs() {
  return supports_create_child_process_with_configs_;
}
 
bool NativeChildProcessLibrary::SupportsNativeChildProcessExitCallback() {
  return supports_native_child_process_exit_callback_;
}
} // namespace ohos::adapter::multiprocess
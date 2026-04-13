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
 
#ifndef OHOS_ADAPTER_MULTIPROCESS_ISOLATE_PROCESS_NATIVE_CHILD_PROCESS_LIBRARY_H_
#define OHOS_ADAPTER_MULTIPROCESS_ISOLATE_PROCESS_NATIVE_CHILD_PROCESS_LIBRARY_H_

 
#include <AbilityKit/native_child_process.h>
 
#include "ohos/adapter/common/shared_library.h"
 
namespace ohos::adapter::multiprocess {
class NativeChildProcessLibrary {
 public:
  using StartNativeChildProcessFunc =
      Ability_NativeChildProcess_ErrCode(const char*,
                                         NativeChildProcess_Args,
                                         NativeChildProcess_Options,
                                         int32_t*);
  using StartNativeChildProcessWithConfigsFunc =
      Ability_NativeChildProcess_ErrCode(const char*,
                                         NativeChildProcess_Args,
                                         Ability_ChildProcessConfigs*,
                                         int32_t*);
  using CreateChildProcessConfigsFunc = Ability_ChildProcessConfigs*();
  using DestroyChildProcessConfigsFunc = void(Ability_ChildProcessConfigs*);
  using SetProcessNameFunc =
      Ability_NativeChildProcess_ErrCode(Ability_ChildProcessConfigs*,
                                         const char*);
  using SetIsolationModeFunc =
      Ability_NativeChildProcess_ErrCode(Ability_ChildProcessConfigs*,
                                         NativeChildProcess_IsolationMode);
  using CreateChildProcessWithConfigsFunc = Ability_NativeChildProcess_ErrCode(
      const char*,
      Ability_ChildProcessConfigs*,
      OH_Ability_OnNativeChildProcessStarted);
  using RegisterNativeChildProcessExitCallbackFunc =
      Ability_NativeChildProcess_ErrCode(OH_Ability_OnNativeChildProcessExit);
  using UnregisterNativeChildProcessExitCallbackFunc =
      Ability_NativeChildProcess_ErrCode(OH_Ability_OnNativeChildProcessExit);
  using KillChildProcessFunc =
      Ability_NativeChildProcess_ErrCode(int32_t);
 
  NativeChildProcessLibrary();
  ~NativeChildProcessLibrary() = default;
 
  // System APIs in libchild_process.so
  Ability_NativeChildProcess_ErrCode StartNativeChildProcess(
      const char* entry,
      NativeChildProcess_Args args,
      NativeChildProcess_Options options,
      int32_t* pid);
  Ability_NativeChildProcess_ErrCode StartNativeChildProcessWithConfigs(
      const char* entry,
      NativeChildProcess_Args args,
      Ability_ChildProcessConfigs* configs,
      int32_t* pid);
  Ability_ChildProcessConfigs* CreateChildProcessConfigs();
  void DestroyChildProcessConfigs(Ability_ChildProcessConfigs* configs);
  Ability_NativeChildProcess_ErrCode SetProcessName(
      Ability_ChildProcessConfigs* configs,
      const char* process_name);
  Ability_NativeChildProcess_ErrCode SetIsolationMode(
      Ability_ChildProcessConfigs* configs,
      NativeChildProcess_IsolationMode isolation_mode);
  Ability_NativeChildProcess_ErrCode CreateChildProcessWithConfigs(
      const char* lib_name,
      Ability_ChildProcessConfigs* configs,
      OH_Ability_OnNativeChildProcessStarted on_process_started);
  Ability_NativeChildProcess_ErrCode RegisterNativeChildProcessExitCallback(
    OH_Ability_OnNativeChildProcessExit callback);
  Ability_NativeChildProcess_ErrCode UnregisterNativeChildProcessExitCallback(
    OH_Ability_OnNativeChildProcessExit callback);
  Ability_NativeChildProcess_ErrCode KillChildProcess(int32_t pid);

  // Helpers to determine which API can be used.
  // API supported since API 13
  bool SupportsStartChildProcess();
  // API supported since API 20
  bool SupportsStartChildProcessWithConfigs();
  bool SupportsCreateChildProcessWithConfigs();
  bool SupportsNativeChildProcessExitCallback();
 
 private:
  StartNativeChildProcessFunc* start_native_child_process_func_ = nullptr;
  StartNativeChildProcessWithConfigsFunc*
      start_native_child_process_with_config_func_ = nullptr;
  CreateChildProcessConfigsFunc* create_child_process_configs_func_ = nullptr;
  DestroyChildProcessConfigsFunc* destroy_child_process_configs_func_ = nullptr;
  SetProcessNameFunc* set_process_name_func_ = nullptr;
  SetIsolationModeFunc* set_isolation_mode_func_ = nullptr;
  CreateChildProcessWithConfigsFunc* create_child_process_with_configs_func_ =
      nullptr;
  RegisterNativeChildProcessExitCallbackFunc*
      register_native_child_process_exit_callback_func_ = nullptr;
  UnregisterNativeChildProcessExitCallbackFunc*
      unregister_native_child_process_exit_callback_func_ = nullptr;
  KillChildProcessFunc* kill_child_process_func_ = nullptr;

  bool supports_start_child_process_ = false;
  bool supports_start_child_process_with_configs_ = false;
  bool supports_create_child_process_with_configs_ = false;
  bool supports_native_child_process_exit_callback_ = false;
 
  common::SharedLibrary lib_child_process_;
};
}  // namespace ohos::adapter::multiprocess
#endif // OHOS_ADAPTER_MULTIPROCESS_ISOLATE_PROCESS_NATIVE_CHILD_PROCESS_LIBRARY_H_
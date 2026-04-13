/*
 * Copyright (c) 2023-2025 Huawei Device Co., Ltd.
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of
 *    conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list
 *    of conditions and the following disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used
 *    to endorse or promote products derived from this software without specific prior written
 *    permission.
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
 
#include "ohos/adapter/node_handle/node_handle_impl.h"
 
#include "ohos/adapter/aki_hook/aki_hook.h"
 
namespace ohos::adapter::nodeHandle {
  
NodeHandleImpl& NodeHandleImpl::GetInstance() {
  static NodeHandleImpl instance;
  return instance;
}
 
NodeHandleImpl::NodeHandleImpl()
    : ace_ndk_lib_("ace_ndk.z") {
  if (is_support_node_handle_) {
    LOGE("NodeHandleImpl node handle is already supported");
    return;
  }
 
  if (!ace_ndk_lib_.IsLoaded()) {
    LOGE("NodeHandleImpl load libace_ndk.z.so failed");
    return;
  }
 
  if (!LoadAllNodeHandleFunctions(ace_ndk_lib_)) {
    LOGE("NodeHandleImpl load functions failed");
    return;
  }
 
  LOGI("NodeHandleImpl Initialize success, support node handle feature");
  is_support_node_handle_ = true;
}

bool NodeHandleImpl::LoadAllNodeHandleFunctions(
    const common::SharedLibrary& lib) {
  return ace_ndk_lib_.LoadFunction(
            &surface_holder_create_func_,
            "OH_ArkUI_SurfaceHolder_Create") &&
         ace_ndk_lib_.LoadFunction(
            &surface_callback_create_func_,
            "OH_ArkUI_SurfaceCallback_Create") &&
         ace_ndk_lib_.LoadFunction(
            &set_user_data_func_,
            "OH_ArkUI_SurfaceHolder_SetUserData") &&
         ace_ndk_lib_.LoadFunction(
            &get_user_data_func_,
            "OH_ArkUI_SurfaceHolder_GetUserData") &&
         ace_ndk_lib_.LoadFunction(
            &xcomponent_initialize_func_,
            "OH_ArkUI_XComponent_Initialize") &&
         ace_ndk_lib_.LoadFunction(
             &add_surface_callback_func_,
             "OH_ArkUI_SurfaceHolder_AddSurfaceCallback") &&
         ace_ndk_lib_.LoadFunction(
            &surface_holder_dispose_func_,
            "OH_ArkUI_SurfaceHolder_Dispose") &&
         ace_ndk_lib_.LoadFunction(
            &surface_callback_dispose_func_,
            "OH_ArkUI_SurfaceCallback_Dispose") &&
         ace_ndk_lib_.LoadFunction(
            &get_native_window_func_,
            "OH_ArkUI_XComponent_GetNativeWindow") &&
         ace_ndk_lib_.LoadFunction(
             &set_surface_created_event_func_,
             "OH_ArkUI_SurfaceCallback_SetSurfaceCreatedEvent") &&
         ace_ndk_lib_.LoadFunction(
             &set_surface_changed_event_func_,
             "OH_ArkUI_SurfaceCallback_SetSurfaceChangedEvent") &&
         ace_ndk_lib_.LoadFunction(
             &set_surface_destroyed_event_func_,
             "OH_ArkUI_SurfaceCallback_SetSurfaceDestroyedEvent");
}

NodeHandleImpl::~NodeHandleImpl() {
  surface_holder_create_func_ = nullptr;
  surface_callback_create_func_ = nullptr;
  set_user_data_func_ = nullptr;
  get_user_data_func_ = nullptr;
  xcomponent_initialize_func_ = nullptr;
  add_surface_callback_func_ = nullptr;
  surface_holder_dispose_func_ = nullptr;
  surface_callback_dispose_func_ = nullptr;
  get_native_window_func_ = nullptr;
  set_surface_created_event_func_ = nullptr;
  set_surface_changed_event_func_ = nullptr;
  set_surface_destroyed_event_func_ = nullptr;
}
 
__attribute__((no_sanitize("cfi", "cfi-icall")))
OH_ArkUI_SurfaceHolder* NodeHandleImpl::SurfaceHolderCreate(ArkUI_NodeHandle node) {
  if (surface_holder_create_func_ == nullptr) {
    LOGE("NodeHandleImpl::SurfaceHolderCreate func null");
    return nullptr;
  }
  return surface_holder_create_func_(node);
}
 
__attribute__((no_sanitize("cfi", "cfi-icall")))
OH_ArkUI_SurfaceCallback* NodeHandleImpl::SurfaceCallbackCreate() {
  if (surface_callback_create_func_ == nullptr) {
    LOGE("NodeHandleImpl::SurfaceCallbackCreate func null");
    return nullptr;
  }
  return surface_callback_create_func_();
}
 
__attribute__((no_sanitize("cfi", "cfi-icall")))
int32_t NodeHandleImpl::SetUserData(OH_ArkUI_SurfaceHolder* surface_holder,
                                    void* user_data) {
  if (set_user_data_func_ == nullptr) {
    LOGE("NodeHandleImpl::SetUserData func null");
    return -1;
  }
  return set_user_data_func_(surface_holder, user_data);
}
 
__attribute__((no_sanitize("cfi", "cfi-icall")))
void* NodeHandleImpl::GetUserData(OH_ArkUI_SurfaceHolder* surface_holder) {
  if (get_user_data_func_ == nullptr) {
    LOGE("NodeHandleImpl::GetUserData func null");
    return nullptr;
  }
  return get_user_data_func_(surface_holder);
}
 
__attribute__((no_sanitize("cfi", "cfi-icall")))
int32_t NodeHandleImpl::XComponentInitialize(ArkUI_NodeHandle node) {
  if (xcomponent_initialize_func_ == nullptr) {
    LOGE("NodeHandleImpl::XComponentInitialize func null");
    return -1;
  }
  return xcomponent_initialize_func_(node);
}
 
__attribute__((no_sanitize("cfi", "cfi-icall")))
void NodeHandleImpl::SetSurfaceCreatedEvent(OH_ArkUI_SurfaceCallback* callback,
                                            SurfaceCreatedCallbackFunc* func) {
  if (set_surface_created_event_func_ == nullptr) {
    LOGE("NodeHandleImpl::SetSurfaceCreatedEvent func null");
    return;
  }
  set_surface_created_event_func_(callback, func);
}
 
__attribute__((no_sanitize("cfi", "cfi-icall")))
void NodeHandleImpl::SetSurfaceChangedEvent(OH_ArkUI_SurfaceCallback* callback,
                                            SurfaceChangedCallbackFunc* func) {
  if (set_surface_changed_event_func_ == nullptr) {
    LOGE("NodeHandleImpl::SetSurfaceChangedEvent func null");
    return;
  }
  set_surface_changed_event_func_(callback, func);
}
 
__attribute__((no_sanitize("cfi", "cfi-icall")))
void NodeHandleImpl::SetSurfaceDestroyedEvent(OH_ArkUI_SurfaceCallback* callback,
                                              SurfaceDestroyedCallbackFunc* func) {
  if (set_surface_destroyed_event_func_ == nullptr) {
    LOGE("NodeHandleImpl::SetSurfaceDestroyedEvent func null");
    return;
  }
  set_surface_destroyed_event_func_(callback, func);
}
 
__attribute__((no_sanitize("cfi", "cfi-icall")))
int32_t NodeHandleImpl::AddSurfaceCallback(OH_ArkUI_SurfaceHolder* surface_holder,
                                           OH_ArkUI_SurfaceCallback* callback) {
  if (add_surface_callback_func_ == nullptr) {
    LOGE("NodeHandleImpl::AddSurfaceCallback func null");
    return -1;
  }
  return add_surface_callback_func_(surface_holder, callback);
}
 
__attribute__((no_sanitize("cfi", "cfi-icall")))
void NodeHandleImpl::SurfaceHolderDispose(OH_ArkUI_SurfaceHolder* surface_holder) {
  if (surface_holder_dispose_func_ == nullptr) {
    LOGE("NodeHandleImpl::SurfaceHolderDispose func null");
    return;
  }
  surface_holder_dispose_func_(surface_holder);
}
 
__attribute__((no_sanitize("cfi", "cfi-icall")))
void NodeHandleImpl::SurfaceCallbackDispose(OH_ArkUI_SurfaceCallback* callback) {
  if (surface_callback_dispose_func_ == nullptr) {
    LOGE("NodeHandleImpl::SurfaceCallbackDispose func null");
    return;
  }
  surface_callback_dispose_func_(callback);
}
 
__attribute__((no_sanitize("cfi", "cfi-icall")))
OHNativeWindow* NodeHandleImpl::GetNativeWindow(OH_ArkUI_SurfaceHolder* surface_holder) {
  if (get_native_window_func_ == nullptr) {
    LOGE("NodeHandleImpl::GetNativeWindow func null");
    return nullptr;
  }
  return get_native_window_func_(surface_holder);
}
 
bool NodeHandleImpl::IsSupportNodeHandle() {
  return false;
}
 
bool IsSupportNodeHandleFeature() {
  return NodeHandleImpl::GetInstance().IsSupportNodeHandle();
}
 
JSBIND_GLOBAL() {
  JSBIND_FUNCTION(IsSupportNodeHandleFeature);
}
 
}  // namespace ohos::adapter::nodeHandle
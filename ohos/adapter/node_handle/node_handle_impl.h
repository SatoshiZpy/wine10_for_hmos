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
 
#ifndef OHOS_ADAPTER_NODEHANDLE_NODE_HANDLE_IMPL_H_
#define OHOS_ADAPTER_NODEHANDLE_NODE_HANDLE_IMPL_H_
 
#include <ace/xcomponent/native_interface_xcomponent.h>
 
#include "ohos/adapter/common/logging.h"
#include "ohos/adapter/common/shared_library.h"
#include "ohos/adapter/export.h"
 
namespace ohos::adapter::nodeHandle {
 
using SurfaceCreatedCallbackFunc = void(OH_ArkUI_SurfaceHolder*);
using SurfaceChangedCallbackFunc = void(OH_ArkUI_SurfaceHolder*,
                                        uint64_t,
                                        uint64_t);
using SurfaceDestroyedCallbackFunc = void(OH_ArkUI_SurfaceHolder*);
using SurfaceHolderCreateFunc = OH_ArkUI_SurfaceHolder*(ArkUI_NodeHandle);
using SurfaceCallbackCreateFunc = OH_ArkUI_SurfaceCallback*();
using SetUserDataFunc = int32_t(OH_ArkUI_SurfaceHolder*, void*);
using GetUserDataFunc = void*(OH_ArkUI_SurfaceHolder*);
using XComponentInitializeFunc = int32_t(ArkUI_NodeHandle);
using SetSurfaceCreatedEventFunc = void(OH_ArkUI_SurfaceCallback*,
                                        SurfaceCreatedCallbackFunc*);
using SetSurfaceChangedEventFunc = void(OH_ArkUI_SurfaceCallback*,
                                        SurfaceChangedCallbackFunc);
using SetSurfaceDestroyedEventFunc = void(OH_ArkUI_SurfaceCallback*,
                                          SurfaceDestroyedCallbackFunc);
using AddSurfaceCallbackFunc = int32_t(OH_ArkUI_SurfaceHolder*,
                                       OH_ArkUI_SurfaceCallback*);
using SurfaceHolderDisposeFunc = void(OH_ArkUI_SurfaceHolder*);
using SurfaceCallbackDisposeFunc = void(OH_ArkUI_SurfaceCallback*);
using GetNativeWindowFunc = OHNativeWindow*(OH_ArkUI_SurfaceHolder*);
 
class ADAPTER_EXPORT_API NodeHandleImpl {
 public:
  static NodeHandleImpl& GetInstance();
  /*
  * FuncName：OH_ArkUI_SurfaceHolder_Create
  * FuncParam: ArkUI_NodeHandle
  * FuncRetVal: OH_ArkUI_SurfaceHolder*
  */
  OH_ArkUI_SurfaceHolder* SurfaceHolderCreate(ArkUI_NodeHandle node);
  /*
  * FuncName: OH_ArkUI_SurfaceCallback_Create
  * FuncParam: void
  * FuncRetVal: OH_ArkUI_SurfaceCallback*
  */
  OH_ArkUI_SurfaceCallback* SurfaceCallbackCreate();
  /*
  * FuncName: OH_ArkUI_SurfaceHolder_SetUserData
  * FuncParam: OH_ArkUI_SurfaceHolder*, void*
  * FuncRetVal: int32_t
  */
  int32_t SetUserData(OH_ArkUI_SurfaceHolder* surface_holder, void* user_data);
  /*
  * FuncName: OH_ArkUI_SurfaceHolder_GetUserData
  * FuncParam: OH_ArkUI_SurfaceHolder*
  * FuncRetVal: void*
  */
  void* GetUserData(OH_ArkUI_SurfaceHolder* surface_holder);
  /*
  * FuncName: OH_ArkUI_XComponent_Initialize
  * FuncParam: ArkUI_NodeHandle
  * FuncRetVal: int32_t
  */
  int32_t XComponentInitialize(ArkUI_NodeHandle node);
  /*
  * FuncName: OH_ArkUI_SurfaceCallback_SetSurfaceCreatedEvent
  * FuncParam: OH_ArkUI_SurfaceCallback*,
  *            void (*onSurfaceCreated)(OH_ArkUI_SurfaceHolder* surfaceHolder))
  * FuncRetVal: void
  */
  void SetSurfaceCreatedEvent(OH_ArkUI_SurfaceCallback* callback,
                              SurfaceCreatedCallbackFunc* func);
  /*
  * FuncName: OH_ArkUI_SurfaceCallback_SetSurfaceChangedEvent
  * FuncParam: OH_ArkUI_SurfaceCallback*,
  *            void (*onSurfaceChanged)(OH_ArkUI_SurfaceHolder* surfaceHolder,
  *                                     uint64_t width, uint64_t height))
  * FuncRetVal: void
  */
  void SetSurfaceChangedEvent(OH_ArkUI_SurfaceCallback* callback,
                              SurfaceChangedCallbackFunc* func);
  /*
  * FuncName: OH_ArkUI_SurfaceCallback_SetSurfaceDestroyedEvent
  * FuncParam: OH_ArkUI_SurfaceCallback*,
  *            void (*uint64_t)(OH_ArkUI_SurfaceHolder* surfaceHolder))
  * FuncRetVal: void
  */
  void SetSurfaceDestroyedEvent(OH_ArkUI_SurfaceCallback* callback,
                                SurfaceDestroyedCallbackFunc* func);
  /*
  * FuncName: OH_ArkUI_SurfaceHolder_AddSurfaceCallback
  * FuncParam: OH_ArkUI_SurfaceHolder*, OH_ArkUI_SurfaceCallback*
  * FuncRetVal: int32_t
  */
  int32_t AddSurfaceCallback(OH_ArkUI_SurfaceHolder* surface_holder,
                             OH_ArkUI_SurfaceCallback* callback);
  /*
  * FuncName: OH_ArkUI_SurfaceHolder_Dispose
  * FuncParam: OH_ArkUI_SurfaceHolder*
  * FuncRetVal: void
  */
  void SurfaceHolderDispose(OH_ArkUI_SurfaceHolder* surface_holder);
  /*
  * FuncName: OH_ArkUI_SurfaceCallback_Dispose
  * FuncParam: OH_ArkUI_SurfaceCallback*
  * FuncRetVal: void
  */
  void SurfaceCallbackDispose(OH_ArkUI_SurfaceCallback* callback);
  /*
  * FuncName: OH_ArkUI_XComponent_GetNativeWindow
  * FuncParam: OH_ArkUI_SurfaceHolder*
  * FuncRetVal: OHNativeWindow*
  */
  OHNativeWindow* GetNativeWindow(OH_ArkUI_SurfaceHolder* surface_holder);
 
  bool IsSupportNodeHandle();
 
 private:
  NodeHandleImpl();
  ~NodeHandleImpl();
  bool LoadAllNodeHandleFunctions(const common::SharedLibrary&);
 
  bool is_support_node_handle_ = false;
  SurfaceHolderCreateFunc* surface_holder_create_func_ = nullptr;
  SurfaceCallbackCreateFunc* surface_callback_create_func_ = nullptr;
  SetUserDataFunc* set_user_data_func_ = nullptr;
  GetUserDataFunc* get_user_data_func_ = nullptr;
  XComponentInitializeFunc* xcomponent_initialize_func_ = nullptr;
  AddSurfaceCallbackFunc* add_surface_callback_func_ = nullptr;
  SurfaceHolderDisposeFunc* surface_holder_dispose_func_ = nullptr;
  SurfaceCallbackDisposeFunc* surface_callback_dispose_func_ = nullptr;
  GetNativeWindowFunc* get_native_window_func_ = nullptr;
  SetSurfaceCreatedEventFunc* set_surface_created_event_func_ = nullptr;
  SetSurfaceChangedEventFunc* set_surface_changed_event_func_ = nullptr;
  SetSurfaceDestroyedEventFunc* set_surface_destroyed_event_func_ = nullptr;
  common::SharedLibrary ace_ndk_lib_;
};
 
}  // namespace ohos::adapter::nodeHandle
 
#endif  // OHOS_ADAPTER_NODEHANDLE_NODE_HANDLE_IMPL_H_
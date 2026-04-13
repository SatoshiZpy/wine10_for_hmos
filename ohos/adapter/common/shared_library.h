// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_COMMON_SHARED_LIBRARY_H_
#define OHOS_ADAPTER_COMMON_SHARED_LIBRARY_H_

#include <memory>
#include <string>

#include "ohos/adapter/common/logging.h"

namespace ohos::adapter::common {

class SharedLibrary {
 public:
  struct LibSuffix {
    static constexpr const char* prefix = "lib";
    static constexpr const char* suffix = ".so";
  };
  using NativeHandleType = void*;
  using NativeSymbolType = void*;

  SharedLibrary(const SharedLibrary&) = delete;
  SharedLibrary& operator=(const SharedLibrary&) = delete;

  SharedLibrary(SharedLibrary&& other) noexcept;

  SharedLibrary& operator=(SharedLibrary&& other) noexcept;

  SharedLibrary(const char* dir_path, const char* lib_name);

  SharedLibrary(const std::string& dir_path, const std::string& lib_name)
      : SharedLibrary(dir_path.c_str(), lib_name.c_str()) {}

  SharedLibrary(const std::string& dir_path, const char* lib_name)
      : SharedLibrary(dir_path.c_str(), lib_name) {}

  SharedLibrary(const char* dir_path, const std::string& lib_name)
      : SharedLibrary(dir_path, lib_name.c_str()) {}

  explicit SharedLibrary(const std::string& lib_name);

  explicit SharedLibrary(const char* lib_name);

  ~SharedLibrary();

  NativeSymbolType GetSymbol(const char* symbol_name) const;

  NativeSymbolType GetSymbol(const std::string& symbol_name) const;

  bool IsLoaded();

  template <typename T>
  T* GetFunction(const char* symbol_name) const {
    return reinterpret_cast<T*>(GetSymbol(symbol_name));
  }

  template <typename T>
  T* GetFunction(const std::string& symbol_name) const {
    return GetFunction<T>(symbol_name.c_str());
  }

  template <typename T>
  T& GetVariable(const char* symbol_name) const {
    return *reinterpret_cast<T*>(GetSymbol(symbol_name));
  }

  template <typename T>
  T& GetVariable(const std::string& symbol_name) const {
    return GetVariable<T>(symbol_name.c_str());
  }

  template <typename T>
  bool LoadFunction(T** func,
                    const std::string& func_name) {
    if (func == nullptr) {
      LOGE("SharedLibrary, Invalid input param, func is nullptr");
      return false;
    }
  
    T* temp_func = GetFunction<T>(func_name);
    if (temp_func == nullptr) {
      LOGE("SharedLibrary, Failed to get function pointer for %{public}s",
           func_name.c_str());
      return false;
    }
    *func = temp_func;
    return true;
  }

  bool HasSymbol(const char* symbol_name) const noexcept;

  bool HasSymbol(const std::string& symbol) const noexcept;

  NativeHandleType GetNativeHandle() noexcept;

 protected:
  NativeHandleType handle_{nullptr};

  static NativeHandleType open(const char* path) noexcept;

  static NativeSymbolType Symbol(NativeHandleType lib,
                                 const char* name) noexcept;

  static void Close(NativeHandleType lib) noexcept;

  static std::string GetErrorInfo() noexcept;
};
}  // namespace ohos::adapter::common

#endif  // OHOS_ADAPTER_COMMON_SHARED_LIBRARY_H_

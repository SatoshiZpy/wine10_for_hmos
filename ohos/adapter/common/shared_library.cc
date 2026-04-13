// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/common/shared_library.h"

#include <dlfcn.h>

namespace ohos::adapter::common {

SharedLibrary::NativeHandleType SharedLibrary::open(const char* path) noexcept {
  return dlopen(path, RTLD_NOW | RTLD_GLOBAL);
}

SharedLibrary::NativeSymbolType SharedLibrary::Symbol(
    SharedLibrary::NativeHandleType lib,
    const char* name) noexcept {
  return dlsym(lib, name);
}

void SharedLibrary::Close(SharedLibrary::NativeHandleType lib) noexcept {
  dlclose(lib);
}

std::string SharedLibrary::GetErrorInfo() noexcept {
  auto error = dlerror();
  return (error == nullptr) ? "Unknown error (dlerror failed)" : error;
}

SharedLibrary::SharedLibrary(SharedLibrary&& other) noexcept
    : handle_(other.handle_) {
  other.handle_ = nullptr;
}

SharedLibrary& SharedLibrary::operator=(SharedLibrary&& other) noexcept {
  if (this != &other) {
    std::swap(handle_, other.handle_);
  }
  return *this;
}

SharedLibrary::SharedLibrary(const char* dir_path, const char* lib_name) {
  if (!dir_path || !lib_name) {
    return;
  }

  std::string final_name = lib_name;
  std::string final_path = dir_path;

  final_name = LibSuffix::prefix + final_name + LibSuffix::suffix;
  if (!final_path.empty() &&
      final_path.find_last_of('/') != final_path.size() - 1) {
    final_path += '/';
  }
  handle_ = open((final_path + final_name).c_str());
  if (handle_ == nullptr) {
    LOGE("SharedLibrary, error= %{public}s", GetErrorInfo().c_str());
  }
}

bool SharedLibrary::HasSymbol(const char* symbol_name) const noexcept {
  if (!handle_ || !symbol_name) {
    return false;
  }
  return Symbol(handle_, symbol_name) != nullptr;
}

bool SharedLibrary::HasSymbol(const std::string& symbol) const noexcept {
  return HasSymbol(symbol.c_str());
}

SharedLibrary::NativeSymbolType SharedLibrary::GetSymbol(
    const char* symbol_name) const {
  if (symbol_name == nullptr || handle_ == nullptr) {
    return nullptr;
  }
  auto symbol = Symbol(handle_, symbol_name);
  return symbol;
}

SharedLibrary::~SharedLibrary() {
  if (handle_ != nullptr) {
    Close(handle_);
  }
}

SharedLibrary::SharedLibrary(const std::string& lib_name)
    : SharedLibrary("", lib_name.c_str()) {}

SharedLibrary::SharedLibrary(const char* lib_name)
    : SharedLibrary("", lib_name) {}

bool SharedLibrary::IsLoaded() {
  return handle_ != nullptr;
}

SharedLibrary::NativeHandleType SharedLibrary::GetNativeHandle() noexcept {
  return handle_;
}

SharedLibrary::NativeSymbolType SharedLibrary::GetSymbol(
    const std::string& symbol_name) const {
  return GetSymbol(symbol_name.c_str());
}
}  // namespace ohos::adapter

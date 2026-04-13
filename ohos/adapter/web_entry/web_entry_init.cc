// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/web_entry/web_entry_init.h"

#include <js_native_api.h>
#include <js_native_api_types.h>
#include <napi/native_api.h>

#include "ohos/adapter/common/constants.h"
#include "ohos/adapter/common/logging.h"
#include "ohos/adapter/common/trace.h"
#include "ohos/adapter/web_entry/web_entry.h"

namespace ohos::adapter::web_entry {

static napi_value RunBrowser(napi_env env, napi_callback_info info) {
  TRACE_EVENT_0("RunBrowser");
  std::vector<std::string> vec_args;
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  
  if (napi_get_cb_info(env, info, &argc, args, nullptr, nullptr) != napi_ok) {
    LOGE("RunBrowser napi_get_cb_info failed");
    return nullptr;
  }
  uint32_t args_len = 0;
  napi_status status = napi_get_array_length(env, args[0], &args_len);
  if (status != napi_ok || args_len == 0) {
    web_entry::RunBrowser(vec_args);
    return nullptr;
  }

  for (uint32_t i = 0; i < args_len; i++) {
    napi_value element;
    status = napi_get_element(env, args[0], i, &element);
    if (status != napi_ok) {
      break;
    }
    size_t element_len = 0;
    status = napi_get_value_string_utf8(env, element, nullptr, 0, &element_len);
    if (status != napi_ok) {
      break;
    }
    char* element_value = new char[element_len + 1];
    if (element_value) {
      status = napi_get_value_string_utf8(env, element, element_value,
                                          element_len + 1, &element_len);
      if (status != napi_ok) {
        delete[] element_value;
        break;
      }
      vec_args.push_back(element_value);
      delete[] element_value;
    }
  }
  
  web_entry::RunBrowser(vec_args);
  return nullptr;
}

static napi_value RunOtherProcessType(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1] = {nullptr};

  if (napi_get_cb_info(env, info, &argc, args, nullptr, nullptr) != napi_ok) {
    LOGE("RunOtherProcessType napi_get_cb_info failed");
    return nullptr;
  }

  if (argc != 1) {
    napi_throw_type_error(env, nullptr, "Wrong number of arguments");
    return nullptr;
  }

  napi_valuetype valuetype;
  if (napi_typeof(env, args[0], &valuetype) != napi_ok) {
    napi_throw_type_error(env, nullptr, "napi_typeof failed");
    return nullptr;
  }

  if (valuetype != napi_number) {
    napi_throw_type_error(env, nullptr, "Wrong type of arguments");
    return nullptr;
  }

  int32_t process_type = -1;
  if (napi_get_value_int32(env, args[0], &process_type) != napi_ok) {
    napi_throw_type_error(env, nullptr, "napi_get_value failed");
    return nullptr;
  }

  TRACE_EVENT_1("RunOtherProcessType", "process_type", process_type);
  web_entry::RunOtherProcessType(process_type);
  return nullptr;
}

static napi_value RunWine(napi_env env, napi_callback_info info) {
  TRACE_EVENT_0("RunWine");
  std::vector<std::string> vec_args;
  std::vector<std::string> env_args;
  size_t argc = 2;
  napi_value args[2] = {nullptr};
  
  if (napi_get_cb_info(env, info, &argc, args, nullptr, nullptr) != napi_ok) {
    LOGE("RunWine napi_get_cb_info failed");
    return nullptr;
  }
  uint32_t args_len = 0;
  napi_status status = napi_get_array_length(env, args[0], &args_len);
  if (status != napi_ok || args_len == 0) {
    LOGE("RunWine napi_get_array_length failed");
    return nullptr;
  }

  for (uint32_t i = 0; i < args_len; i++) {
    napi_value element;
    status = napi_get_element(env, args[0], i, &element);
    if (status != napi_ok) {
      break;
    }
    size_t element_len = 0;
    status = napi_get_value_string_utf8(env, element, nullptr, 0, &element_len);
    if (status != napi_ok) {
      break;
    }
    char* element_value = new char[element_len + 1];
    if (element_value) {
      status = napi_get_value_string_utf8(env, element, element_value,
                                          element_len + 1, &element_len);
      if (status != napi_ok) {
        delete[] element_value;
        break;
      }
      vec_args.push_back(element_value);
      delete[] element_value;
    }
  }
  
  status = napi_get_array_length(env, args[1], &args_len);
  if (status != napi_ok || args_len == 0) {
    LOGE("RunWine napi_get_array_length failed");
    return nullptr;
  }

  for (uint32_t i = 0; i < args_len; i++) {
    napi_value element;
    status = napi_get_element(env, args[1], i, &element);
    if (status != napi_ok) {
      break;
    }
    size_t element_len = 0;
    status = napi_get_value_string_utf8(env, element, nullptr, 0, &element_len);
    if (status != napi_ok) {
      break;
    }
    char* element_value = new char[element_len + 1];
    if (element_value) {
      status = napi_get_value_string_utf8(env, element, element_value,
                                          element_len + 1, &element_len);
      if (status != napi_ok) {
        delete[] element_value;
        break;
      }
      env_args.push_back(element_value);
      delete[] element_value;
    }
  }
  web_entry::RunWine(vec_args, env_args);
  return nullptr;
}

void Register(napi_env env, napi_value exports) {
  napi_property_descriptor desc[] = {
      DECLARE_NAPI_METHOD("runBrowser", web_entry::RunBrowser),
      DECLARE_NAPI_METHOD("runOtherProcessType", web_entry::RunOtherProcessType),
      DECLARE_NAPI_METHOD("runWine", web_entry::RunWine)
  };
  if (napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]),
                             desc) != napi_ok) {
    LOGE("napi_define_properties failed");
    return;
  }
}
}  // namespace ohos::adapter::web_entry

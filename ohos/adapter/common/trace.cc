// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/common/trace.h"

#include <hitrace/trace.h>
#include <string>
#include <utility>

AdapterScopedTracer::~AdapterScopedTracer() {
  if (trace_enabled_) {
    OH_HiTrace_FinishTrace();
  }
}

void AdapterScopedTracer::Initialize(bool enabled, std::string& arg_str) {
  trace_enabled_ = enabled;
  if (enabled) {
    OH_HiTrace_StartTrace(arg_str.c_str());
  }
}

// TraceArgValue functions
std::string TraceArgValue::ToString(TraceArgType type) const {
  switch (type) {
    case TYPE_BOOL:
      return this->as_bool ? "true" : "false";
    case TYPE_UINT:
      return std::to_string(this->as_uint);
    case TYPE_INT:
      return std::to_string(this->as_int);
    case TYPE_DOUBLE:
      return std::to_string(this->as_double);
    case TYPE_STRING:
      return this->as_string;
    default:
      return "type-error";
  }
}

// TraceArg functions
TraceArg::TraceArg(bool v) {
  type = TYPE_BOOL;
  value.as_bool = v;
}

TraceArg::TraceArg(uint32_t v) {
  type = TYPE_UINT;
  value.as_uint = v;
}

TraceArg::TraceArg(int32_t v) {
  type = TYPE_INT;
  value.as_int = v;
}

TraceArg::TraceArg(double v) {
  type = TYPE_DOUBLE;
  value.as_double = v;
}

TraceArg::TraceArg(const std::string& v) {
  type = TYPE_STRING;
  value.as_string = v.c_str();
}

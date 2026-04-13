// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_FONT_FONT_ADAPTER_H_
#define OHOS_ADAPTER_FONT_FONT_ADAPTER_H_

#include <string>
#include <vector>

#include "ohos/adapter/export.h"

namespace ohos::adapter::font {

struct OhosFontInfo {
  std::string path;
  std::string post_script_name;
  std::string full_name;
  std::string family;
  std::string subfamily;
  int32_t weight = 0;
  int32_t width = 0;
  bool italic = false;
  bool mono_space = false;
  bool symbolic = false;
};

class ADAPTER_EXPORT_API FontAdapter {
 public:
  FontAdapter(const FontAdapter&) = delete;
  FontAdapter(FontAdapter&&) = delete;
  FontAdapter& operator=(const FontAdapter&) = delete;
  static FontAdapter& GetInstance();

  std::vector<std::string> GetSystemFontList();
  std::vector<std::string> GetConfigFontList();
  OhosFontInfo GetFontInfo(const std::string& name);

 private:
  FontAdapter() = default;
  ~FontAdapter() = default;
};

}  // namespace ohos::adapter::font
#endif  // OHOS_ADAPTER_FONT_FONT_ADAPTER_H_

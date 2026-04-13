// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ohos/adapter/common/logging.h"
#include "ohos/adapter/web_entry/chrome/chrome_loader.h"
#include "ohos/adapter/web_entry/shell/content_shell_loader.h"
#include "ohos/adapter/web_entry/wine/wine_loader.h"
#include "ohos/adapter/web_entry/web_loader.h"

namespace ohos::adapter::web_entry {

namespace {

static std::unique_ptr<BasicLoader> current_default_loader = nullptr;
const std::string kShellEntry = "content_shell_web";
const std::string kChromeEntry = "chrome_main_web";
const std::string kWineEntry = "wine";
const std::string kWineServerEntry = "wineserver";

std::unique_ptr<BasicLoader> GetCurrentDefaultLoader(int loaderType, int process_type) {
  std::unique_ptr<BasicLoader> loader;
  switch (loaderType) {
    case WebLoaderType::kContent_shell:
      loader = std::make_unique<ContentShellLoader>("", kShellEntry);
      break;
    case WebLoaderType::kChrome:
      loader = std::make_unique<ChromeMainLoader>("", kChromeEntry);
      break;
    case WebLoaderType::kWine:
      switch (process_type) {
        case ProcessType::kMainProcess:
        case ProcessType::kChildProcess:
          loader = std::make_unique<WineMainLoader>("", kWineEntry);
          break;
        case ProcessType::kServerProcess:
          loader = std::make_unique<WineMainLoader>("", kWineServerEntry);
          break;
        default:
          LOGE("unknown process type: %{public}d", process_type);
          break;
      }
      break;
    default:
      LOGE("create web loader error unknown loaderType: %{public}d", loaderType);
      break;
  }
  return loader && loader->Initialize() ? std::move(loader) : nullptr;
}

}  // namespace

// static
BasicLoader* BasicLoader::Create(int loaderType) {
  return BasicLoader::Create(loaderType, 0);
}

// static
BasicLoader* BasicLoader::Create(int loaderType, int process_type) {
  if (!current_default_loader) {
    current_default_loader = GetCurrentDefaultLoader(loaderType, process_type);
  }
  return current_default_loader.get();
}

// static
bool HasCurrentDefaultLoader() {
  return current_default_loader != nullptr;
}

// static
void ResetLoader() {
  current_default_loader.reset();
}

}  // namespace ohos::adapter::web_entry

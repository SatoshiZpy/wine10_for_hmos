// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_FILE_PICKER_FILE_SELECT_PICKER_H_
#define OHOS_ADAPTER_FILE_PICKER_FILE_SELECT_PICKER_H_

#include <napi/native_api.h>

#include <functional>
#include <string>

#include "ohos/adapter/export.h"

namespace ohos::adapter::file_select_picker {

using FileSelectedCallback = std::function<void(const std::string&)>;
using ContinueDwonloadConfirmCallback = std::function<void(const bool)>;

struct SelectFileDialogParams {
  bool multi_files;
  std::vector<std::vector<std::string>> extensions;
  std::vector<std::string> descriptions;
  bool include_all_files;
};

struct SaveAsDialogParams {
  std::string file_name;
  std::string dir_name;
  std::vector<std::vector<std::string>> extensions;
  std::vector<std::string> descriptions;
  bool include_all_files;
};

class ADAPTER_EXPORT_API FileSelectPicker {
 public:
  FileSelectPicker() = default;
  ~FileSelectPicker() = default;
  static FileSelectPicker& GetInstance();
  void ShowFilePickerDialog(const SelectFileDialogParams& params,
                            FileSelectedCallback);
  void ShowFilePickerDirDialog(bool file_access_persist, FileSelectedCallback);
  void ShowSaveAsDialog(const SaveAsDialogParams& params, FileSelectedCallback);
  void ShowInstallationPackageDialog(ContinueDwonloadConfirmCallback);

 private:
};

}  // namespace ohos::adapter::file_select_picker

#endif  // OHOS_ADAPTER_FILE_SELECT_PICKER_H_

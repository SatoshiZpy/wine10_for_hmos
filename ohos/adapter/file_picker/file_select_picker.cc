// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos/adapter/file_picker/file_select_picker.h"

#include "ohos/adapter/aki_hook/aki_hook.h"
#include "ohos/adapter/common/logging.h"

namespace ohos::adapter::file_select_picker {

FileSelectPicker& FileSelectPicker::GetInstance() {
  static FileSelectPicker helper;
  return helper;
}

void FileSelectPicker::ShowFilePickerDialog(
    const SelectFileDialogParams& params,
    FileSelectedCallback callback) {
  auto func =
      ohos::adapter::GetJSFunction("FilePickerAdapter.ShowDocumentViewPicker");
  if (func) {
    func->Invoke<void>(params, callback);
  }
}

void FileSelectPicker::ShowFilePickerDirDialog(bool file_access_persist,
                                               FileSelectedCallback callback) {
  auto func = ohos::adapter::GetJSFunction(
      "FilePickerAdapter.ShowDirDocumentViewPicker");
  if (func) {
    func->Invoke<void>(file_access_persist, callback);
  }
}

void FileSelectPicker::ShowSaveAsDialog(const SaveAsDialogParams& params,
                                        FileSelectedCallback callback) {
  auto func = ohos::adapter::GetJSFunction(
      "FilePickerAdapter.ShowSaveAsDocumentViewPicker");
  if (func) {
    func->Invoke<void>(params, callback);
  }
}

void FileSelectPicker::ShowInstallationPackageDialog(
    ContinueDwonloadConfirmCallback callback) {
  auto func = ohos::adapter::GetJSFunction(
      "FilePickerAdapter.ShowInstallationPackageDialog");
  if (func) {
    func->Invoke<void>(callback);
  }
}

JSBIND_CLASS(SelectFileDialogParams) {
  JSBIND_CONSTRUCTOR<>();
  JSBIND_PROPERTY(multi_files);
  JSBIND_PROPERTY(extensions);
  JSBIND_PROPERTY(descriptions);
  JSBIND_PROPERTY(include_all_files);
}

JSBIND_CLASS(SaveAsDialogParams) {
  JSBIND_CONSTRUCTOR<>();
  JSBIND_PROPERTY(file_name);
  JSBIND_PROPERTY(dir_name);
  JSBIND_PROPERTY(extensions);
  JSBIND_PROPERTY(descriptions);
  JSBIND_PROPERTY(include_all_files);
}

}  // namespace ohos::adapter::file_select_picker

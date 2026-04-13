// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos_print_adapter.h"
#include "ohos/adapter/aki_hook/aki_hook.h"

namespace ohos::adapter::print {
PrintAdapter& PrintAdapter::GetInstance() {
  static PrintAdapter helper;
  return helper;
}

void PrintAdapter::OhosPrint(std::string printFile) {
  auto func = ohos::adapter::GetJSFunction(
      "PrintAdapter.ShowPrintDialog");
  if (func) {
    return func->Invoke<void>(printFile);
  }
}

void PrintAdapter::SystemPrinterSettings() {
  if (auto func = ohos::adapter::GetJSFunction(
          "PrintAdapter.StartSystemPrinterSettingsAbility")) {
    func->Invoke<void>();
  }
}

void PrintAdapter::PrintPdfFile() {
  if (auto func = ohos::adapter::GetJSFunction("PrintAdapter.PrintPdfFiles")) {
    aki::ArrayBuffer pdfArrayBuffer((uint8_t*)pdf_buffer.get(), pdf_length);
    func->Invoke<void>(std::move(pdfArrayBuffer));
  }
}

void PrintAdapter::PrintPdfFile(std::shared_ptr<char[]> pdf_data, uint32_t length) {
  if (auto func = ohos::adapter::GetJSFunction("PrintAdapter.PrintPdfFiles")) {
    aki::ArrayBuffer pdfArrayBuffer((uint8_t*)pdf_data.get(), length);
    func->Invoke<void>(std::move(pdfArrayBuffer));
  }
}

void PrintAdapter::SetPdfBuff(std::shared_ptr<char[]> pdf_data,
                              uint32_t length) {
  pdf_buffer = pdf_data;
  pdf_length = length;
}

void PrintAdapter::ShowPrintTipsDialog() {
  if (auto func =
          ohos::adapter::GetJSFunction("PrintAdapter.ShowPrintTipsDialog")) {
    func->Invoke<void>();
  }
}

void PrintAdapter::AddPrinters(PrinterInfo& printInfo,
                               std::shared_ptr<PrintCallback> printCallback) {}
}  // namespace ohos::adapter::print

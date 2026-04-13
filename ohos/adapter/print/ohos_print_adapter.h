// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_ADAPTER_PRINT_ADAPTER_H_
#define OHOS_ADAPTER_PRINT_ADAPTER_H_

#include <cstdint>
#include <string>
#include <vector>
#include "ohos/adapter/export.h"

namespace ohos::adapter::print {
struct PrinterRange {
  int startPage;
  int endPage;
  std::vector<int> pages;
};

struct PreviewAttribute {
  PrinterRange previewRange;
  int result;
};

enum class PrinterState {
  PRINTER_ADDED = 0,
  PRINTER_REMOVED = 1,
  PRINTER_CAPABILITY_UPDATED = 2,
  PRINTER_CONNECTED = 3,
  PRINTER_DISCONNECTED = 4,
  PRINTER_RUNNING = 5
};

struct PrinterCapability {
  int colorMode;
  int duplexMode;
  std::vector<int> pageSize;
  std::vector<int> resolution;
};

struct PrinterInfo {
  std::string printerId;
  std::string printerName;
  PrinterState printerState;
  int64_t printerIcon;
  PrinterCapability capability;
  std::string options;
};

class ADAPTER_EXPORT_API PrintCallback {
 public:
  virtual int32_t PrintPropertiesChange() = 0;
  virtual int32_t PrintUnavailable() = 0;
};

class ADAPTER_EXPORT_API PrintAdapter {
 public:
  static PrintAdapter& GetInstance();
  virtual ~PrintAdapter() = default;
  void OhosPrint(std::string printFile = "");
  void SystemPrinterSettings();
  void SetPdfBuff(std::shared_ptr<char[]> pdf_data, uint32_t length);
  void PrintPdfFile();
  void PrintPdfFile(std::shared_ptr<char[]> pdf_data, uint32_t length);
  void ShowPrintTipsDialog();
  void AddPrinters(PrinterInfo& printInfo,
                   std::shared_ptr<PrintCallback> printCallback);

 private:
  std::shared_ptr<char[]> pdf_buffer;
  uint32_t pdf_length;
};
}  // namespace ohos::adapter::print
#endif  // OHOS_ADAPTER_PRINT_ADAPTER_H_

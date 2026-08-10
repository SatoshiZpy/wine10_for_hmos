// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ohos_print_adapter.h"
#include "ohos/adapter/aki_hook/aki_hook.h"
#include <cstring>
#include <new>
#include <cstdio>
#include <algorithm>
#include <chrono>
#include <functional>
#include <condition_variable>
#include <mutex>
#include "ohos/adapter/common/logging.h"

namespace {

template <typename T>
T ReadValue(aki::Value value, T default_value) {
  if (!value.IsUndefined() && !value.IsNull()) return value.As<T>();
  return default_value;
}

uint32_t ArrayLength(const aki::Value& value) {
  uint32_t length = 0;
  if (value.IsArray())
    napi_get_array_length(aki::JSBind::GetScopedEnv(), value.GetHandle(), &length);
  return length;
}

template <size_t Size>
void CopyString(char (&dest)[Size], const std::string& source) {
  std::snprintf(dest, Size, "%s", source.c_str());
}

uint32_t GetModeMask(aki::Value modes) {
  uint32_t mask = 0;
  if (!modes.IsArray()) return mask;
  for (uint32_t i = 0; i < ArrayLength(modes); ++i) {
    int32_t mode = ReadValue<int32_t>(modes[i], -1);
    if (mode >= 0 && mode < 32) mask |= 1u << mode;
  }
  return mask;
}

std::string GetPageSizes(aki::Value sizes) {
  std::string result;
  if (!sizes.IsArray()) return result;
  for (uint32_t i = 0; i < ArrayLength(sizes); ++i) {
    aki::Value size = sizes[i];
    if (!result.empty()) result += ';';
    result += ReadValue<std::string>(size["id"], "");
    result += ',' + std::to_string(ReadValue<int32_t>(size["width"], 0));
    result += ',' + std::to_string(ReadValue<int32_t>(size["height"], 0));
  }
  return result;
}

struct PrinterQuery {
  std::mutex mutex;
  std::condition_variable condition;
  std::vector<ohos_printer_info> printers;
  bool complete = false;
};

}  // namespace

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

bool PrintAdapter::PrintPostScript(const std::string& printer_id,
                                   const std::string& job_name,
                                   std::shared_ptr<char[]> data, uint32_t length) {
  if (auto func = ohos::adapter::GetJSFunction("PrintAdapter.PrintPostScript")) {
    aki::ArrayBuffer array_buffer(reinterpret_cast<uint8_t *>(data.get()), length);
    func->Invoke<void>(printer_id, job_name, std::move(array_buffer));
  } else {
    return false;
  }
  return true;
}

int PrintAdapter::GetPrinters(struct ohos_printer_info* printers, uint32_t capacity) {
  auto func = ohos::adapter::GetJSFunction("PrintAdapter.GetPrinters");
  if (!func) {
    LOGE("PrintAdapter.GetPrinters is not bound");
    return -1;
  }

  auto query = std::make_shared<PrinterQuery>();
  std::function<void(aki::Value, int32_t)> callback =
      [query](aki::Value values, int32_t count) {
        std::lock_guard<std::mutex> lock(query->mutex);
        if (values.IsArray()) {
          count = std::min<int32_t>(count, ArrayLength(values));
          for (int32_t i = 0; i < count; ++i) {
            aki::Value value = values[i];
            aki::Value capability = value["capability"];
            ohos_printer_info info{};
            CopyString(info.id, ReadValue<std::string>(value["printerId"], ""));
            CopyString(info.name, ReadValue<std::string>(value["printerName"], std::string(info.id)));
            CopyString(info.description,
                       ReadValue<std::string>(value["description"], ""));
            CopyString(info.make, ReadValue<std::string>(value["printerMake"], ""));
            CopyString(info.uri, ReadValue<std::string>(value["uri"], ""));
            CopyString(info.options, ReadValue<std::string>(value["options"], ""));
            CopyString(info.page_sizes, GetPageSizes(capability["supportedPageSizes"]));
            info.status = ReadValue<uint32_t>(value["printerStatus"], 2);
            info.color_modes = GetModeMask(capability["supportedColorModes"]);
            info.duplex_modes = GetModeMask(capability["supportedDuplexModes"]);
            info.is_default = ReadValue<bool>(value["isDefault"], false);
            if (info.id[0]) query->printers.push_back(info);
          }
        }
        query->complete = true;
        query->condition.notify_all();
      };

  func->Invoke<void>(callback);
  std::unique_lock<std::mutex> lock(query->mutex);
  if (!query->condition.wait_for(lock, std::chrono::seconds(5),
                                 [&query] { return query->complete; })) {
    LOGE("Timed out while discovering OHOS host printers");
    return -1;
  }

  int count = static_cast<int>(query->printers.size());
  if (printers) {
    uint32_t copy_count = std::min<uint32_t>(capacity, query->printers.size());
    std::copy_n(query->printers.begin(), copy_count, printers);
  }
  return count;
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

extern "C" int OhosPrintPostScript(const char *printer_id, const char *job_name,
                                    const void *data, uint32_t length) {
  if (!data || !length) return -1;
  std::shared_ptr<char[]> buffer(new (std::nothrow) char[length]);
  if (!buffer) return -1;
  std::memcpy(buffer.get(), data, length);
  return ohos::adapter::print::PrintAdapter::GetInstance().PrintPostScript(
      printer_id ? printer_id : "", job_name ? job_name : "", buffer, length)
      ? 0 : -1;
}

extern "C" int OhosGetPrinters(struct ohos_printer_info *printers,
                                uint32_t capacity) {
  return ohos::adapter::print::PrintAdapter::GetInstance().GetPrinters(printers, capacity);
}

// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import JsBindingUtils from '../utils/JsBindingUtils';
import Inject from '../common/InjectModule';
import lazy { PrintAdapter } from '../adapter/PrintAdapter';

function implPrintAdapter(): PrintAdapter {
  return Inject.getOrCreate(PrintAdapter);
}

function showPrintDialog(value: string) {
  implPrintAdapter().showPrintDialog(value);
}

function PrintPdfFiles(pdfData: ArrayBuffer) {
  implPrintAdapter().PrintPdfFiles(pdfData);
}

function savePdf(data: ArrayBuffer) {
  implPrintAdapter().savePdf(data);
}

function printPostScript(printerId: string, jobName: string, data: ArrayBuffer) {
  void implPrintAdapter().printPostScript(printerId, jobName, data);
}

function showPrintTipsDialog() {
  implPrintAdapter().showPrintTipsDialog();
}

function getPrinters(callback: Function) {
  void implPrintAdapter().getPrinters(callback);
}

export class PrintAdapterBind {
  static bind() {
    JsBindingUtils.bindFunction("PrintAdapter.ShowPrintDialog", showPrintDialog);
    JsBindingUtils.bindFunction("PrintAdapter.PrintPdfFiles", PrintPdfFiles);
    JsBindingUtils.bindFunction("PrintAdapter.SavePdf", savePdf);
    JsBindingUtils.bindFunction("PrintAdapter.PrintPostScript", printPostScript);
    JsBindingUtils.bindFunction("PrintAdapter.ShowPrintTipsDialog", showPrintTipsDialog);
    JsBindingUtils.bindFunction("PrintAdapter.GetPrinters", getPrinters);
  }
}
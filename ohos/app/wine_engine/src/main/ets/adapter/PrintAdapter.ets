// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { inject, injectable } from 'inversify';
import { BusinessError } from '@ohos.base';
import print from '@ohos.print';
import fs, { ListFileOptions } from '@ohos.file.fs';
import picker from '@ohos.file.picker';

import { BaseAdapter } from '../common/BaseAdapter';
import LogUtil from '../utils/LogUtil';
import LogMethod from '../common/LogDecorator';

import promptAction from '@ohos.promptAction'
import { ContextAdapter } from './ContextAdapter';

interface HostPrinterInfo {
  printerId: string;
  printerName: string;
  printerStatus: print.PrinterStatus;
  description?: string;
  printerMake?: string;
  uri?: string;
  capability?: print.PrinterCapabilities;
  options?: string;
  isDefault: boolean;
}


const TAG: string = 'Print';

@injectable()
export class PrintAdapter extends BaseAdapter {
  private printFilesPath: string [] = [];
  private ctxAdapter: ContextAdapter;

  constructor(@inject(ContextAdapter) ctxAdapter: ContextAdapter) {
    super();
    this.ctxAdapter = ctxAdapter;
    this.init();
  }

  override init() {
    let dirPath = this.ctxAdapter.getContext().getApplicationContext().tempDir + "/print";
    fs.mkdir(dirPath).catch((err: BusinessError) => {
      LogUtil.error(TAG, 'mkdir error: ' + JSON.stringify(err));
    });
  }
  @LogMethod
  async getPrinters(callback?: Function): Promise<void> {
    const printers: HostPrinterInfo[] = [];
    try {
      const printerIds = await print.getAddedPrinters();
      for (const printerId of printerIds) {
        try {
          const info = await print.getPrinterInformationById(printerId);
          const printer: HostPrinterInfo = {
            printerId: info.printerId || printerId,
            printerName: info.printerName || printerId,
            printerStatus: info.printerStatus,
            description: info.description ?? '',
            printerMake: info.printerMake ?? '',
            uri: info.uri ?? '',
            capability: info.capability,
            options: info.options ?? '',
            isDefault: printers.length === 0
          };
          printers.push(printer);
        } catch (err) {
          LogUtil.error(TAG, `Failed to read printer ${printerId}: ${JSON.stringify(err)}`);
          const printer: HostPrinterInfo = {
            printerId: printerId,
            printerName: printerId,
            printerStatus: print.PrinterStatus.PRINTER_UNAVAILABLE,
            isDefault: printers.length === 0
          };
          printers.push(printer);
        }
      }
    } catch (err) {
      LogUtil.error(TAG, `Failed to discover printers: ${JSON.stringify(err)}`);
    }
    if (callback !== undefined) callback(printers, printers.length);
  }

  @LogMethod
  showPrintDialog(value: string) {
    // TODO: need adapter print file
    let dirPath = this.ctxAdapter.getContext().getApplicationContext().tempDir + "/print";
    let listFileOption: ListFileOptions = {
      recursion: false,
      listNum: 0,
      filter: {}
    }
    fs.listFile(dirPath, listFileOption).then((filenames: Array<string>) => {
      for (let i = 0; i < filenames.length; i++) {
        let pngPath = dirPath + '/' + filenames[i];
        let tempFile: fs.File = fs.openSync(pngPath);
        let fileFd = 'fd://' + tempFile.fd;
        this.printFilesPath.push(fileFd);
      }
      print.print(this.printFilesPath, this.ctxAdapter.getActiveContext())
        .then((printTask: print.PrintTask) => {
          printTask.on('succeed', () => {
            LogUtil.info(TAG, 'print state is succeed');
          })
          printTask.on('cancel', () => {
            LogUtil.info(TAG, 'print state is cancel');
          })
        }).catch((error: Error) => {
        LogUtil.error(TAG, 'print err ' + JSON.stringify(error));
      })
    }).catch((err: BusinessError) => {
      LogUtil.error(TAG, 'list file error: ' + JSON.stringify(err));
    });
  }

  @LogMethod
  PrintPdfFiles(pdfData: ArrayBuffer) {
    let printAdapter: print.PrintDocumentAdapter = {
      onStartLayoutWrite(jobId: string,
                         oldAttrs: print.PrintAttributes,
                         newAttrs: print.PrintAttributes,
                         fd: number,
                         writeResultCallback: (jobId: string, writeResult: print.PrintFileCreationState) => void): void {
        const contentBuf: Uint8Array = new Uint8Array(pdfData);
        fs.writeSync(fd, contentBuf.buffer);
        writeResultCallback(jobId, print.PrintFileCreationState.PRINT_FILE_CREATED_UNRENDERED);
      },
      onJobStateChanged(jobId: string, state: print.PrintDocumentAdapterState): void {}
    }
    // default print start parameter
    let printAttributes: print.PrintAttributes = {
      pageSize: print.PrintPageType.PAGE_ISO_A4,
      directionMode: print.PrintDirectionMode.DIRECTION_MODE_AUTO,
      colorMode: print.PrintColorMode.COLOR_MODE_MONOCHROME,
      duplexMode: print.PrintDuplexMode.DUPLEX_MODE_NONE
    }
    print.print('', printAdapter, printAttributes, this.ctxAdapter.getActiveContext())
      .then(() => {
        LogUtil.info(TAG, 'Succeeded in print pdf files.');
      }).catch((err: BusinessError) => {
      LogUtil.error(TAG, 'Failed to print pdf files. Cause:' + JSON.stringify(err));
    });
  }

  @LogMethod
  async printPostScript(printerId: string, jobName: string, data: ArrayBuffer): Promise<void> {
    const sourcePrinter = printerId || "OHOS PDF Printer";
    LogUtil.info(TAG,
      `Received Wine PostScript (${data.byteLength} bytes) for printer ${sourcePrinter}.`);
    if (data.byteLength == 0) {
      LogUtil.error(TAG, "Refusing an empty Wine PostScript job.");
      return;
    }
    const postScriptData = new Uint8Array(data).slice();

    try {
      let targetPrinterId = printerId;
      let targetPrinterName = printerId;
      const isVirtualPdfJob = targetPrinterId.length == 0;

      if (isVirtualPdfJob) {
        const printerIds = await print.getAddedPrinters();
        for (const candidateId of printerIds) {
          try {
            const printerInfo = await print.getPrinterInformationById(candidateId);
            const printerText = [candidateId, printerInfo.printerName ?? "",
              printerInfo.description ?? "", printerInfo.printerMake ?? "", printerInfo.uri ?? ""]
              .join(" ").toLowerCase();
            if (printerText.includes("pdf") || printerText.includes("print to pdf") ||
              printerText.includes("save as pdf")) {
              targetPrinterId = candidateId;
              targetPrinterName = printerInfo.printerName || candidateId;
              break;
            }
          } catch (err) {
            LogUtil.error(TAG, `Failed to query printer ${candidateId}: ${JSON.stringify(err)}`);
          }
        }
        if (targetPrinterId.length == 0) {
          throw new Error("No system PDF printer is available.");
        }
      } else {
        try {
          const printerInfo = await print.getPrinterInformationById(targetPrinterId);
          targetPrinterName = printerInfo.printerName || targetPrinterId;
        } catch (err) {
          LogUtil.error(TAG, `Failed to query target printer ${targetPrinterId}: ${JSON.stringify(err)}`);
        }
      }

      const now = new Date();
      const timestamp = `${now.getFullYear()}${this.twoDigits(now.getMonth() + 1)}` +
        `${this.twoDigits(now.getDate())}-${this.twoDigits(now.getHours())}` +
        `${this.twoDigits(now.getMinutes())}${this.twoDigits(now.getSeconds())}`;
      const resolvedJobName = jobName.trim() ||
        `WinePrint-${timestamp}.${isVirtualPdfJob ? "pdf" : "ps"}`;
      const printJob: print.PrintJobData = {
        printerId: targetPrinterId,
        jobName: resolvedJobName,
        documentFormat: print.PrintDocumentFormat.DOCUMENT_FORMAT_POSTSCRIPT,
        docFlavor: print.DocFlavor.BYTES,
        copyNumber: 1,
        isLandscape: false,
        colorMode: print.PrintColorMode.COLOR_MODE_COLOR,
        duplexMode: print.PrintDuplexMode.DUPLEX_MODE_NONE,
        pageSize: { id: "ISO_A4", name: "ISO_A4", width: 8268, height: 11692 },
        binaryData: postScriptData
      };

      LogUtil.info(TAG, `Submitting Wine PostScript to ${targetPrinterName} (${targetPrinterId}).`);
      await print.startPrint(printJob);
      LogUtil.info(TAG, `Wine print job submitted: ${printJob.jobName}.`);
    } catch (err) {
      LogUtil.error(TAG, "Failed to submit Wine PostScript: " + JSON.stringify(err));
    }
  }

  @LogMethod
  savePdf(data: ArrayBuffer) {
    LogUtil.info(TAG, 'Received Wine PDF (' + data.byteLength + ' bytes).');
    const options = new picker.DocumentSaveOptions();
    const now = new Date();
    const timestamp = `${now.getFullYear()}${this.twoDigits(now.getMonth() + 1)}` +
      `${this.twoDigits(now.getDate())}-${this.twoDigits(now.getHours())}` +
      `${this.twoDigits(now.getMinutes())}${this.twoDigits(now.getSeconds())}`;
    options.newFileNames = [`WinePrint-${timestamp}.pdf`];
    options.defaultFilePathUri = 'file://docs/Documents';
    options.fileSuffixChoices = ['PDF document|.pdf'];

    const documentPicker = new picker.DocumentViewPicker();
    LogUtil.info(TAG, 'Opening PDF save picker.');
    documentPicker.save(options).then((uris: Array<string>) => {
      if (uris.length == 0) {
        LogUtil.error(TAG, 'PDF save picker returned no destination.');
        return;
      }
      LogUtil.info(TAG, 'PDF save destination selected: ' + uris[0]);
      let output: fs.File | undefined;
      try {
        output = fs.openSync(uris[0], fs.OpenMode.WRITE_ONLY | fs.OpenMode.TRUNC);
        const written = fs.writeSync(output.fd, data);
        if (written != data.byteLength) {
          throw new Error(`Incomplete PDF write: ${written}/${data.byteLength}`);
        }
        LogUtil.info(TAG, `Saved Wine PDF (${written} bytes).`);
      } catch (err) {
        LogUtil.error(TAG, 'Failed to save Wine PDF: ' + JSON.stringify(err));
      } finally {
        if (output) fs.closeSync(output);
      }
    }).catch((err: BusinessError) => {
      LogUtil.error(TAG, 'PDF save was cancelled or failed: ' + JSON.stringify(err));
    });
  }

  private twoDigits(value: number): string {
    return value < 10 ? `0${value}` : `${value}`;
  }

  @LogMethod
  showPrintTipsDialog() {
    try {
      promptAction.showDialog({
        title: $r('app.string.print_title'),
        message: $r('app.string.print_tip'),
        buttons: [
          {
            text: $r('app.string.print_ok'),
            color: $r('app.color.print_tip_button')
          },
        ],
      }).catch((err: Error) => {
          LogUtil.error(TAG, 'showDialog error:' + err);
        })
    } catch (error) {
      LogUtil.error(TAG, 'list file error: ' + JSON.stringify(error));
    }
    ;
  }
}
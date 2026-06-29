// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import fs from '@ohos.file.fs';
import resmgr from '@ohos.resourceManager';
import { inject, injectable } from 'inversify';
import { BaseAdapter } from '../common/BaseAdapter';
import LogMethod from '../common/LogDecorator';
import LogUtil from '../utils/LogUtil';
import lazy { ContextAdapter } from './ContextAdapter';
import StringUtil from '../utils/StringUtil';

const TAG: string = 'PresetDeployerAdapter';

interface PresetMapping {
  rawDir: string;
  destAbsDir: string;
}

interface PresetConfig {
  version: string;
  mappings: PresetMapping[];
}

interface ResolvedMapping {
  rawDir: string;
  destAbsDir: string;
}

@injectable()
export class PresetDeployerAdapter extends BaseAdapter {
  private ctxAdapter: ContextAdapter;

  constructor(@inject(ContextAdapter) ctxAdapter: ContextAdapter) {
    super();
    this.ctxAdapter = ctxAdapter;
  }

  private hasPreset(): boolean {
    try {
      const context = this.ctxAdapter.getContext();
      return (context.resourceManager.getRawFileListSync('preset') ?? []).length > 0;
    } catch (err) {
      return false;
    }
  }

  private checkMarkerExists(markerPath: string): boolean {
    try {
      if (fs.accessSync(markerPath)) {
        return true;
      }
      return false;
    } catch (err) {
      LogUtil.error(TAG, `file access failed: ${JSON.stringify(err)}`);
      return false;
    }
  }

  private loadPresetConfig(configFile: string): PresetConfig | undefined {
    try {
      const context = this.ctxAdapter.getContext();
      const uint8Array = context.resourceManager.getRawFileContentSync(configFile);
      const configContent = StringUtil.uint8ArrayToString(uint8Array);
      return JSON.parse(configContent);
    } catch (err) {
      LogUtil.error(TAG, `parse config failed: ${JSON.stringify(err)}`);
      return undefined;
    }
  }

  private resolveMappingPaths(mappings: PresetMapping[], prefixDir: string): ResolvedMapping[] {
    const result: ResolvedMapping[] = [];
    for (let i = 0; i < mappings.length; i++) {
      const mapping: PresetMapping = mappings[i];
      result.push({
        rawDir: mapping.rawDir,
        destAbsDir: mapping.destAbsDir.replace(/\{prefixDir\}/g, prefixDir)
      });
    }
    return result;
  }

  private async deployDirectory(rawDir: string, destAbsDir: string): Promise<void> {
    this.ensureDirExists(destAbsDir);
    await this.copyEntry(rawDir, destAbsDir);
  }

  @LogMethod
  async deployIfNeeded(prefixDir: string): Promise<void> {
    if (!this.hasPreset()) {
      LogUtil.info(TAG, 'rawfile/preset not found, skipping');
      return;
    }

    const config = this.loadPresetConfig('preset_config.json');
    if (!config) {
      LogUtil.error(TAG, 'load preset config error');
      return;
    }
    const markerPath = `${prefixDir}/${config.version}`;
    if (this.checkMarkerExists(markerPath)) {
      LogUtil.info(TAG, 'preset already deployed, skipping');
      return;
    }

    LogUtil.info(TAG, `start preset deploy, prefix=${prefixDir}, version=${config.version}`);
    this.ensureDirExists(prefixDir);
    const mappings = this.resolveMappingPaths(config.mappings, prefixDir);
    for (const mapping of mappings) {
      LogUtil.info(TAG, `deploy ${mapping.rawDir} -> ${mapping.destAbsDir}`);
      await this.deployDirectory(mapping.rawDir, mapping.destAbsDir);
    }
    this.touchMarker(markerPath);
    LogUtil.info(TAG, `preset deploy done, marker=${markerPath}`);
  }

  private async copyEntry(
    rawPath: string,
    destAbsPath: string
  ): Promise<void> {
    const children = this.listRawDir(rawPath);
    if (children.length === 0) {
      await this.copyRawFile(rawPath, destAbsPath);
      return;
    }
    this.ensureDirExists(destAbsPath);
    for (let i = 0; i < children.length; i++) {
      const child = children[i];
      await this.copyEntry(`${rawPath}/${child}`, `${destAbsPath}/${child}`);
    }
  }

  private async copyRawFile(
    rawPath: string,
    destAbsPath: string
  ): Promise<void> {
    let rawFd: resmgr.RawFileDescriptor | undefined = undefined;
    let dest: fs.File | undefined = undefined;
    const context = this.ctxAdapter.getContext();
    try {
      rawFd = await context.resourceManager.getRawFd(rawPath);
      dest = fs.openSync(destAbsPath, fs.OpenMode.CREATE | fs.OpenMode.READ_WRITE | fs.OpenMode.TRUNC);
      const chunkSize = 256 * 1024;
      const buffer = new ArrayBuffer(chunkSize);
      let remaining = rawFd.length;
      let srcOffset = rawFd.offset;
      let dstOffset = 0;
      while (remaining > 0) {
        const toRead = remaining > chunkSize ? chunkSize : remaining;
        const read = fs.readSync(rawFd.fd, buffer, { offset: srcOffset, length: toRead });
        if (read <= 0) {
          break;
        }
        fs.writeSync(dest.fd, buffer, { offset: dstOffset, length: read });
        srcOffset += read;
        dstOffset += read;
        remaining -= read;
      }
    } catch (err) {
      LogUtil.error(TAG, `copyRawFile failed: ${rawPath} -> ${destAbsPath}, err=${JSON.stringify(err)}`);
    } finally {
      if (dest) {
        try {
          fs.closeSync(dest);
        } catch (err) {
          LogUtil.error(TAG, `close dest failed: ${destAbsPath}, err=${JSON.stringify(err)}`);
        }
      }
      if (rawFd) {
        try {
          await context.resourceManager.closeRawFd(rawPath);
        } catch (err) {
          LogUtil.error(TAG, `closeRawFd failed: ${rawPath}, err=${JSON.stringify(err)}`);
        }
      }
    }
  }

  private listRawDir(rawPath: string): string[] {
    try {
      const context = this.ctxAdapter.getContext();
      return context.resourceManager.getRawFileListSync(rawPath) ?? [];
    } catch (err) {
      return [];
    }
  }

  private ensureDirExists(absPath: string): void {
    try {
      if (!fs.accessSync(absPath)) {
        fs.mkdirSync(absPath, true);
      }
    } catch (err) {
      LogUtil.error(TAG, `ensureDirExists failed: ${absPath}, err=${JSON.stringify(err)}`);
    }
  }

  private touchMarker(markerPath: string): void {
    try {
      const file = fs.openSync(markerPath, fs.OpenMode.CREATE | fs.OpenMode.READ_WRITE);
      fs.closeSync(file);
    } catch (err) {
      LogUtil.error(TAG, `touchMarker failed: ${markerPath}, err=${JSON.stringify(err)}`);
    }
  }
}

export { PresetMapping };

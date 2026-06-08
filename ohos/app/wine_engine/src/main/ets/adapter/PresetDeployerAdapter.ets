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

const TAG: string = 'PresetDeployerAdapter';

interface PresetMapping {
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

  @LogMethod
  hasSapPreset(): boolean {
    try {
      const context = this.ctxAdapter.getContext();
      return (context.resourceManager.getRawFileListSync('preset') ?? []).length > 0;
    } catch (err) {
      return false;
    }
  }

  @LogMethod
  async deployIfNeeded(
    prefixDir: string,
    markerName: string,
    mappings: PresetMapping[]
  ): Promise<void> {
    const markerPath = `${prefixDir}/${markerName}`;
    try {
      if (fs.accessSync(markerPath)) {
        return;
      }
    } catch (err) {
      LogUtil.info(TAG, `file access failed: ${JSON.stringify(err)}`);
      return;
    }

    LogUtil.info(TAG, `start preset deploy, prefix=${prefixDir}`);
    this.ensureDir(prefixDir);

    const lockDir = `${prefixDir}/.deploy.lock`;
    this.reclaimStaleLock(lockDir);

    try {
      fs.mkdirSync(lockDir, false);
    } catch (err) {
      LogUtil.info(TAG, `deploy lock busy, skip: ${JSON.stringify(err)}`);
      return;
    }

    try {
      if (fs.accessSync(markerPath)) {
        return;
      }

      for (let i = 0; i < mappings.length; i++) {
        const m = mappings[i];
        LogUtil.info(TAG, `deploy ${m.rawDir} -> ${m.destAbsDir}`);
        this.ensureDir(m.destAbsDir);
        await this.copyChildrenToDir(m.rawDir, m.destAbsDir);
      }

      this.touchMarker(markerPath);
      LogUtil.info(TAG, `preset deploy done, marker=${markerPath}`);
    } catch (err) {
      LogUtil.info(TAG, `preset deploy failed: ${JSON.stringify(err)}`);
      return;
    } finally {
      try {
        fs.rmdirSync(lockDir);
      } catch (err) {
        LogUtil.error(TAG, `rmdir lock failed: ${lockDir}, err=${JSON.stringify(err)}`);
      }
    }
  }

  private async copyChildrenToDir(
    rawDir: string,
    destAbsDir: string
  ): Promise<void> {
    const entries = this.listRawDir(rawDir);
    for (let i = 0; i < entries.length; i++) {
      const entry = entries[i];
      const childRaw = `${rawDir}/${entry}`;
      const childDest = `${destAbsDir}/${entry}`;
      await this.copyEntry(childRaw, childDest);
    }
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
    this.ensureDir(destAbsPath);
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

  private reclaimStaleLock(lockDir: string): void {
    const STALE_LOCK_TIMEOUT_SEC = 600;
    try {
      if (!fs.accessSync(lockDir)) {
        return;
      }
      const stat = fs.statSync(lockDir);
      const ageSec = Math.floor(Date.now() / 1000) - stat.mtime;
      if (ageSec > STALE_LOCK_TIMEOUT_SEC) {
        LogUtil.info(TAG, `reclaim stale deploy lock: ${lockDir}, ageSec=${ageSec}`);
        fs.rmdirSync(lockDir);
      }
    } catch (err) {
      LogUtil.error(TAG, `reclaimStaleLock failed: ${lockDir}, err=${JSON.stringify(err)}`);
    }
  }

  private ensureDir(absPath: string): void {
    try {
      if (!fs.accessSync(absPath)) {
        fs.mkdirSync(absPath, true);
      }
    } catch (err) {
      LogUtil.error(TAG, `ensureDir failed: ${absPath}, err=${JSON.stringify(err)}`);
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

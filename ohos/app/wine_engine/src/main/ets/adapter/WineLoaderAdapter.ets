// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { inject, injectable } from 'inversify';
import { BaseAdapter } from '../common/BaseAdapter';
import fs from '@ohos.file.fs';
import lazy { ContextAdapter } from './ContextAdapter';
import lazy { PresetDeployerAdapter } from './PresetDeployerAdapter';
import LogUtil from '../utils/LogUtil';
import CheckEmptyUtils from '../utils/CheckEmptyUtils';

const TAG: string = 'WineLoaderAdapter';

@injectable()
export class WineLoaderAdapter extends BaseAdapter {
  private ctxAdapter: ContextAdapter;
  private presetDeployer: PresetDeployerAdapter;

  constructor(
    @inject(ContextAdapter) ctxAdapter: ContextAdapter,
    @inject(PresetDeployerAdapter) presetDeployer: PresetDeployerAdapter,
  ) {
    super();
    this.ctxAdapter = ctxAdapter;
    this.presetDeployer = presetDeployer;
  }

  async loadWine(exe_path: string, sock_path: string, file_path: string, argv?: string[]): Promise<void> {
    const context = this.ctxAdapter.getActiveContext();
    if (!context) {
      LogUtil.error(TAG, 'context is undefined');
      return;
    }

    const bundleDir = context.bundleCodeDir ?? '';
    const resDir = context.resourceDir ?? '';
    const dataDir = context.filesDir ?? '';
    const cacheDir = context.cacheDir ?? '';
    const preferenceDir = context.preferencesDir ?? '';

    if (!bundleDir || !resDir || !dataDir || !cacheDir || !preferenceDir) {
      LogUtil.error(TAG,
        `invalid path, bundleDir=${bundleDir}, resDir=${resDir}, dataDir=${dataDir},
        cacheDir=${cacheDir}, preferenceDir=${preferenceDir}`);
      return;
    }

    const binDir = `${resDir}/bin`;
    const libDir = `${resDir}/lib`;
    const dllDir = `${libDir}/wine`;
    const loader = `${bundleDir}/libs/arm64/libwine.so`;

    const prefixDir = `${dataDir}/prefix`;
    const logPath = `${dataDir}/log.txt`;

    this.ensureDir(prefixDir);
    this.ensureFile(logPath);

    await this.deployPreset(prefixDir);

    const environ = new Map<string, string>();
    this.setEnv(environ, 'WINERESPATH', resDir);

    this.setEnv(environ, 'XDG_DATA_HOME', dataDir);
    this.setEnv(environ, 'XDG_CACHE_HOME', cacheDir);
    this.setEnv(environ, 'XDG_CONFIG_HOME', preferenceDir);

    this.setEnv(environ, 'WINELOADER', loader);
    this.setEnv(environ, 'WINEDLLPATH', dllDir);
    this.setEnv(environ, 'WINEPREFIX', prefixDir);
    this.setEnv(environ, 'WINEDEBUGLOG', logPath);

    // Force Vulkan renderer for D3D11/D3D12 via WINE_D3D_CONFIG
    // this.setEnv(environ, 'WINE_D3D_CONFIG', 'renderer=vulkan');

    // DXVK integration: force native DLL loading for D3D11/DXGI/D3D12
    // This makes Wine load DXVK's d3d11.dll and dxgi.dll instead of built-in versions
    // DXVK (d3d8/9/10core/11/dxgi) and vkd3d-proton (d3d12) are genuine native PEs
    // deployed in the engine dll dir; =n makes the prefix/resfile native copy win
    // over any builtin-stamped file. d3d10/d3d10_1 are wine builtins (thin layers
    // over d3d10core) and must NOT be overridden.
    this.setEnv(environ, 'WINEDLLOVERRIDES', 'd3d8=n;d3d9=n;d3d10core=n;d3d11=n;dxgi=n;d3d12=n');

    // DXVK options via env (newline-separated; single source of DXVK config):
    // - no forced dxgi.syncInterval: apps present with SyncInterval=0, DXVK
    //   then prefers MAILBOX (latest-frame replacement, no queued backlog);
    //   if the surface does not support it it falls back to FIFO, where
    //   acquire backpressure phase-locks production to the panel rate.
    //   Forcing syncInterval=1 stacked DXVK's own vblank wait on top of the
    //   OHOS queue pacing and produced an uneven cadence.
    // - maxFrameLatency=3: keep the queue deep enough that acquire does not
    //   serialize with the render time (latency 1/2 measured 74/81fps).
    // - numCompilerThreads=0: auto (multi-threaded) pipeline compilation,
    //   overriding the stale single-thread entry in the persistent conf.
    this.setEnv(environ, 'DXVK_CONFIG', 'dxgi.maxFrameLatency = 3\ndxvk.numCompilerThreads = 0');
    // No DXVK_FRAME_RATE: present pacing is done by the native window's
    // swap interval (set when the Vulkan surface is created), which aligns
    // every present to the vsync grid.  DXVK's sleep-based limiter composed
    // badly with the OHOS buffer queue (a 115/120 cap measured 74/76fps).

    // DXVK relax features (Maleoon driver capability gaps)
    this.setEnv(environ, 'DXVK_RELAX_FEATURES', '1');

    const config = context.resourceManager.getConfigurationSync();
    const locale = config?.locale;
    this.setEnv(environ, 'LC_ALL', locale);
    this.setEnv(environ, 'LANG', locale);

    const env: string[] = [];
    environ.forEach((value, key) => {
      env.push(key, value);
    });
    LogUtil.info(TAG, `env=${env.toString()}, prefixDir=${prefixDir}`);

    if (CheckEmptyUtils.checkStrIsEmpty(sock_path)) {
      const exePath = CheckEmptyUtils.checkStrIsEmpty(exe_path) ?
        'c:\\windows\\system32\\notepad.exe' : exe_path;
      const cmd: string[] = [
        loader,
        !CheckEmptyUtils.checkStrIsEmpty(file_path) ? file_path : exePath,
        ...(argv || [])
      ];
      LogUtil.info(TAG, `cmd=${cmd.toString()}`);
      this.nativeContext.runWine(cmd, env);
    } else {
      LogUtil.info(TAG, `sock_path=${sock_path}`);
      this.nativeContext.runWineProcess(env, sock_path);
    }
  }

  private async deployPreset(prefixDir: string): Promise<void> {
    try {
      await this.presetDeployer.deployIfNeeded(prefixDir);
    } catch (err) {
      LogUtil.error(TAG, `deployPreset failed: ${JSON.stringify(err)}`);
    }
  }

  private setEnv(environ: Map<string, string>, key: string, value?: string): void {
    if (value && value.trim().length > 0) {
      environ.set(key, value);
    }
  }

  private ensureDir(path: string): void {
    try {
      const exists = fs.accessSync(path);
      if (!exists) {
        fs.mkdirSync(path);
      }
    } catch (err) {
      LogUtil.error(TAG, `ensureDir failed: ${path}, err=${JSON.stringify(err)}`);
    }
  }

  private ensureFile(path: string): void {
    try {
      const exists = fs.accessSync(path);
      if (!exists) {
        const file = fs.openSync(path, fs.OpenMode.CREATE | fs.OpenMode.READ_WRITE);
        const randomAccessFile = fs.createRandomAccessFileSync(file);
        randomAccessFile.close();
      }
    } catch (err) {
      LogUtil.error(TAG, `ensureFile failed: ${path}, err=${JSON.stringify(err)}`);
    }
  }
}
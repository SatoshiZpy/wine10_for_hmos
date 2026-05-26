// Copyright (c) 2026 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import fs from '@ohos.file.fs';
import resmgr from '@ohos.resourceManager';
import LogUtil from './LogUtil';

const TAG: string = 'PresetDeployer';

/**
 * 单条预置映射：把 rawfile 下的某个目录展开到 prefix 内一个绝对路径下。
 *
 * - rawDir：相对 entry/src/main/resources/rawfile/ 的子路径，例如 'preset/sap'。
 *   本工程把 SAP GUI 部署包按 sap/add/modify/regs/root 五块放在 'preset/' 下。
 * - destAbsDir：HarmonyOS 沙箱内的绝对目标路径，例如 ${prefixDir}/dosdevices/c:/sap。
 *
 * 展开语义：把 rawDir 的每个直接孩子拷到 destAbsDir 下，rawDir 自身的目录名不出现在最终路径里。
 * 例：rawDir='preset/sap'、destAbsDir='.../c:/sap'，结果是 '.../c:/sap/SAPGUI/...'。
 */
interface PresetMapping {
  rawDir: string;
  destAbsDir: string;
}

/**
 * 把 rawfile/preset 下的预置资源一次性展开到 Wine prefix。
 *
 * 设计原则：
 * 1) 幂等。第一次部署完成后写入 markerName 标记文件，后续启动直接 return。
 * 2) 只追加、不覆盖目录结构。每条 mapping 自己负责把内容塞进 destAbsDir。
 * 3) 出错就地降级。任何单个文件拷贝失败只记日志，不抛出，避免把 Wine 启动整个拖垮。
 *
 * 调用方（WineWindow / WineWindowNode）必须在调用 nativeContext.runWine 之前 await 完成。
 */
export class PresetDeployer {
  /**
   * 入口：判断是否需要部署，需要就按 mappings 顺序执行；不需要就直接返回。
   *
   * 并发保护：主进程（WineWindow）和渲染子进程（WineWindowNode）会几乎同时
   * 调到这里。如果只判 marker 后就开拷贝，两边可能同时进入循环，对同一目标
   * 文件并行 fs.openSync(... TRUNC)，造成互相截断、零字节文件等坏结果。
   * 这里在 prefixDir 下用 fs.mkdirSync(lockDir, false) 原子创建一个 .deploy.lock
   * 目录——HarmonyOS @ohos.file.fs 的 OpenMode 没有 POSIX 的 EXCL 标志，但
   * mkdirSync 在目标已存在时会抛 13900015，与 O_CREAT|O_EXCL 在原子语义上等价。
   * 后到的进程拿不到锁直接返回，让先到的进程独占完成部署。
   *
   * @param resourceManager HarmonyOS 资源管理器，用于读取 rawfile
   * @param prefixDir       Wine prefix 绝对路径（也是 marker / 锁目录落盘位置）
   * @param markerName      部署完成标记文件名（带版本号方便后续强制重置）
   * @param mappings        要执行的映射列表，按数组顺序串行执行
   */
  static async deployIfNeeded(
    resourceManager: resmgr.ResourceManager,
    prefixDir: string,
    markerName: string,
    mappings: PresetMapping[]
  ): Promise<void> {
    const markerPath = `${prefixDir}/${markerName}`;
    // 已经部署过：marker 存在则直接跳过，避免每次启动重复 IO（SAP 部署包很大）
    if (fs.accessSync(markerPath)) {
      return;
    }

    LogUtil.info(TAG, `start preset deploy, prefix=${prefixDir}`);
    // 先把 prefix 自身建出来（首次启动时 prefixDir 可能还不存在），
    // 锁目录随后要落到这个目录里
    PresetDeployer.ensureDir(prefixDir);

    const lockDir = `${prefixDir}/.deploy.lock`;
    // 陈旧锁回收：上一次部署进程崩溃时来不及释放，会留下锁目录让 mkdir 永远失败。
    // 取锁前先看锁目录存在多久（mtime 是秒级 Unix 时间戳），超过 STALE_LOCK_TIMEOUT
    // 就当作崩溃残留删掉。阈值取 10 分钟：SAP 部署包实测 1~3 分钟，留足慢设备
    // 余量；同时远短于"用户重启 App 的最短间隔"，避免误删另一进程进行中的部署。
    PresetDeployer.reclaimStaleLock(lockDir);

    try {
      // 用"创建目录"作为原子取锁原语：HarmonyOS @ohos.file.fs 的 OpenMode 没有 EXCL，
      // 但 mkdirSync(path, false) 在目标已存在时会抛 13900015 (File exists)，
      // 与 POSIX O_CREAT|O_EXCL 在原子语义上等价（mkdir 在 VFS 层就是原子操作）。
      fs.mkdirSync(lockDir, false);
    } catch (err) {
      // 锁已被另一进程持有：让它独占完成部署，本进程不重复跑
      LogUtil.info(TAG, `deploy lock busy, skip: ${JSON.stringify(err)}`);
      return;
    }

    try {
      // 持锁后再 double-check marker：另一进程可能在我们拿锁前刚刚完成并释放
      if (fs.accessSync(markerPath)) {
        return;
      }

      for (let i = 0; i < mappings.length; i++) {
        const m = mappings[i];
        LogUtil.info(TAG, `deploy ${m.rawDir} -> ${m.destAbsDir}`);
        // 每条 mapping 的目标根目录都先确保存在（destAbsDir 上层链可能不存在，例如 .../dosdevices/c:/users/100）
        PresetDeployer.ensureDir(m.destAbsDir);
        await PresetDeployer.copyChildrenToDir(resourceManager, m.rawDir, m.destAbsDir);
      }

      // 全量映射结束才写 marker。中途单文件异常已经在 copyRawFile 内被吞掉，这里仍会写 marker，
      // 这是有意的——丢失个别非关键文件比每次启动重复展开几百兆 SAPGUI 资源代价低；如果发现
      // 部署不完整，删 marker 文件即可触发再来一次。
      PresetDeployer.touchMarker(markerPath);
      LogUtil.info(TAG, `preset deploy done, marker=${markerPath}`);
    } finally {
      // 不论成功失败都要释放锁：删掉锁目录，避免下次启动 mkdir 永远拿不到
      try {
        fs.rmdirSync(lockDir);
      } catch (err) {
        LogUtil.error(TAG, `rmdir lock failed: ${lockDir}, err=${JSON.stringify(err)}`);
      }
    }
  }

  /**
   * 展开 rawDir 的孩子到 destAbsDir：rawDir 自身的目录名不出现在最终路径里。
   * 例：rawDir='preset/sap'、destAbsDir='.../c:/sap'，
   *     结果是 '.../c:/sap/SAPGUI/...'，不是 '.../c:/sap/sap/SAPGUI/...'。
   */
  private static async copyChildrenToDir(
    resourceManager: resmgr.ResourceManager,
    rawDir: string,
    destAbsDir: string
  ): Promise<void> {
    const entries = PresetDeployer.listRawDir(resourceManager, rawDir);
    for (let i = 0; i < entries.length; i++) {
      const entry = entries[i];
      const childRaw = `${rawDir}/${entry}`;
      const childDest = `${destAbsDir}/${entry}`;
      await PresetDeployer.copyEntry(resourceManager, childRaw, childDest);
    }
  }

  /**
   * 拷贝 rawfile 下任意条目（文件或目录）。
   *
   * rawfile 是一个被打进 HAP 的只读资源树；HarmonyOS 的 ResourceManager 没有直接的
   * isDir API，只能借用 getRawFileListSync 的返回值：文件返回空数组、目录返回非空
   * 数组。空目录会被当成文件——但 rawfile 打包时空目录本身不会进 HAP，所以这里
   * 不会遇到 "目录但 list 为空" 的情况。
   *
   * 用 listRawDir 一次取列表后按长度分流，避免 isDir 判断和取孩子各调一次 native。
   */
  private static async copyEntry(
    resourceManager: resmgr.ResourceManager,
    rawPath: string,
    destAbsPath: string
  ): Promise<void> {
    const children = PresetDeployer.listRawDir(resourceManager, rawPath);
    if (children.length === 0) {
      // 文件：转成沙箱真实路径
      await PresetDeployer.copyRawFile(resourceManager, rawPath, destAbsPath);
      return;
    }
    // 目录：在目标侧建出来再深入孩子
    PresetDeployer.ensureDir(destAbsPath);
    for (let i = 0; i < children.length; i++) {
      const child = children[i];
      await PresetDeployer.copyEntry(resourceManager, `${rawPath}/${child}`, `${destAbsPath}/${child}`);
    }
  }

  /**
   * 把 rawfile 中一个具体文件流式拷到 destAbsPath。
   *
   * 关键点：
   * - rawfile 不是文件系统文件，不能用 fs.copyFileSync；必须先 getRawFd 拿到底层 fd 与 offset。
   * - rawFd.fd 实际指向 HAP 内的索引文件，所以读时要带 srcOffset；写时目标是普通沙箱文件，dstOffset 从 0 开始。
   * - 用 256KB 缓冲区分块读写，避免大文件（SAP 部署包里有几十 MB 的 OCX/DLL）一次性占内存。
   */
  private static async copyRawFile(
    resourceManager: resmgr.ResourceManager,
    rawPath: string,
    destAbsPath: string
  ): Promise<void> {
    let rawFd: resmgr.RawFileDescriptor | undefined = undefined;
    let dest: fs.File | undefined = undefined;
    try {
      rawFd = await resourceManager.getRawFd(rawPath);
      // TRUNC 让重新部署时把已存在的旧文件清零再写，避免残留
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
          // 提前 EOF：不当致命错误，跳出循环让外层 finally 关流
          break;
        }
        fs.writeSync(dest.fd, buffer, { offset: dstOffset, length: read });
        srcOffset += read;
        dstOffset += read;
        remaining -= read;
      }
    } catch (err) {
      // 单文件失败不向上抛，只记日志：deployIfNeeded 仍会继续处理其他文件
      LogUtil.error(TAG, `copyRawFile failed: ${rawPath} -> ${destAbsPath}, err=${JSON.stringify(err)}`);
    } finally {
      // 双重关闭：dest 是普通文件，rawFd 是资源管理器持有的描述符，必须分别关闭，
      // 否则会泄漏 fd（SAP GUI 几千个文件，泄漏会很快撑爆 ulimit）。
      if (dest) {
        try {
          fs.closeSync(dest);
        } catch (err) {
          LogUtil.error(TAG, `close dest failed: ${destAbsPath}, err=${JSON.stringify(err)}`);
        }
      }
      if (rawFd) {
        try {
          await resourceManager.closeRawFd(rawPath);
        } catch (err) {
          LogUtil.error(TAG, `closeRawFd failed: ${rawPath}, err=${JSON.stringify(err)}`);
        }
      }
    }
  }

  /**
   * 列出 rawfile 中某个路径的直接孩子。
   *
   * 同时承担"是不是目录"的判断：返回非空数组即目录、空数组即文件或不存在。
   * 调用方（copyEntry）按 length 分流，避免重复 native 调用。
   */
  private static listRawDir(resourceManager: resmgr.ResourceManager, rawPath: string): string[] {
    try {
      return resourceManager.getRawFileListSync(rawPath) ?? [];
    } catch (err) {
      return [];
    }
  }

  /**
   * 陈旧锁回收：锁目录存活超过阈值时，视为上次部署进程崩溃残留并删除。
   * 阈值 10 分钟（600 秒）覆盖 SAP 部署包正常拷贝时长（实测 1~3 分钟）。
   *
   * fs.Stat.mtime 是秒级 Unix 时间戳；Date.now() 返回毫秒，所以这里换算到秒比对。
   * 任何异常一律静默吞掉——回收只是优化，失败时让 mkdirSync 走原路径报错即可。
   */
  private static reclaimStaleLock(lockDir: string): void {
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

  /**
   * 确保 absPath 目录存在，缺则递归创建（mkdirSync 第二参 true 等同 mkdir -p）。
   */
  private static ensureDir(absPath: string): void {
    try {
      if (!fs.accessSync(absPath)) {
        fs.mkdirSync(absPath, true);
      }
    } catch (err) {
      LogUtil.error(TAG, `ensureDir failed: ${absPath}, err=${JSON.stringify(err)}`);
    }
  }

  /**
   * 创建一个空的 marker 文件，表示部署已完成。
   * 内容不重要，存在性才是信号。
   */
  private static touchMarker(markerPath: string): void {
    try {
      const file = fs.openSync(markerPath, fs.OpenMode.CREATE | fs.OpenMode.READ_WRITE);
      fs.closeSync(file);
    } catch (err) {
      LogUtil.error(TAG, `touchMarker failed: ${markerPath}, err=${JSON.stringify(err)}`);
    }
  }
}

export { PresetMapping };

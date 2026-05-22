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
 * - mergeChildren：决定"展开方式"：
 *     true  → 把 rawDir 里的每个孩子拷到 destAbsDir 下（孩子并入目标目录）；
 *     false → 同上（当前两个分支实现相同；保留旗标是为日后区分语义留接口）。
 *   现阶段 WineWindow / WineWindowNode 全部传 true。
 */
interface PresetMapping {
  rawDir: string;
  destAbsDir: string;
  mergeChildren: boolean;
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
   * @param resourceManager HarmonyOS 资源管理器，用于读取 rawfile
   * @param prefixDir       Wine prefix 绝对路径（也是 marker 写入位置）
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
    // 先把 prefix 自身建出来（首次启动时 prefixDir 可能还不存在）
    PresetDeployer.ensureDir(prefixDir);

    for (let i = 0; i < mappings.length; i++) {
      const m = mappings[i];
      LogUtil.info(TAG, `deploy ${m.rawDir} -> ${m.destAbsDir}, mergeChildren=${m.mergeChildren}`);
      // 每条 mapping 的目标根目录都先确保存在（destAbsDir 上层链可能不存在，例如 .../dosdevices/c:/users/100）
      PresetDeployer.ensureDir(m.destAbsDir);
      if (m.mergeChildren) {
        await PresetDeployer.copyChildrenToDir(resourceManager, m.rawDir, m.destAbsDir);
      } else {
        await PresetDeployer.copyDirToDir(resourceManager, m.rawDir, m.destAbsDir);
      }
    }

    // 全量映射成功才写 marker。中途异常已经在 copyRawFile 内被吞掉，这里仍会写 marker，
    // 这是有意的——丢失个别文件比每次启动重复展开几百兆 SAPGUI 资源代价低；如果发现
    // 部署不完整，删 marker 文件即可触发再来一次。
    PresetDeployer.touchMarker(markerPath);
    LogUtil.info(TAG, `preset deploy done, marker=${markerPath}`);
  }

  /**
   * 把 rawDir 的每个直接孩子拷到 destAbsDir 下。
   *
   * 注意当前实现与 copyChildrenToDir 完全一致；保留独立函数是为日后区分
   * "整棵树拷贝过去并多套一层 rawDir 名"和"展开孩子"两种语义留接口。
   */
  private static async copyDirToDir(
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
   * rawfile 是一个被打进 HAP 的只读资源树；这里通过 isRawDir 判断它是不是子目录，
   * 是的话就递归处理孩子，是文件就走 copyRawFile → 沙箱真实路径。
   */
  private static async copyEntry(
    resourceManager: resmgr.ResourceManager,
    rawPath: string,
    destAbsPath: string
  ): Promise<void> {
    if (PresetDeployer.isRawDir(resourceManager, rawPath)) {
      // 目录：在目标侧建出来再深入孩子
      PresetDeployer.ensureDir(destAbsPath);
      const children = PresetDeployer.listRawDir(resourceManager, rawPath);
      for (let i = 0; i < children.length; i++) {
        const child = children[i];
        await PresetDeployer.copyEntry(resourceManager, `${rawPath}/${child}`, `${destAbsPath}/${child}`);
      }
      return;
    }
    // 文件：转成沙箱真实路径
    await PresetDeployer.copyRawFile(resourceManager, rawPath, destAbsPath);
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
   * 失败或非目录返回空数组（让上层把它当文件处理）。
   */
  private static listRawDir(resourceManager: resmgr.ResourceManager, rawPath: string): string[] {
    try {
      const list = resourceManager.getRawFileListSync(rawPath);
      return list ?? [];
    } catch (err) {
      return [];
    }
  }

  /**
   * 判断 rawPath 是不是目录。
   *
   * HarmonyOS 的 ResourceManager 没有直接的 isDir API，只能借用：
   * 文件返回空数组、目录返回非空数组。空目录会被当成文件——但 rawfile 打包时
   * 空目录本身不会进 HAP，所以这里不会遇到 "目录但 list 为空" 的状况。
   */
  private static isRawDir(resourceManager: resmgr.ResourceManager, rawPath: string): boolean {
    try {
      const list = resourceManager.getRawFileListSync(rawPath);
      return list !== undefined && list !== null && list.length > 0;
    } catch (err) {
      return false;
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

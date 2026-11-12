/*
 * Wine EXE launcher.
 *
 * Copyright 2026 Wine OHOS contributors
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

import { common, contextConstant, Want, StartOptions } from '@kit.AbilityKit';
import { fileIo as fs, fileUri, picker } from '@kit.CoreFileKit';

@Entry
@Component
struct Launcher {
  @State private folderUri: string = '';
  @State private folderPath: string = '';
  @State private exePath: string = '';
  @State private argumentsText: string = '';
  @State private busy: boolean = false;
  @State private status: string = '';
  @State private failed: boolean = false;

  private context(): common.UIAbilityContext {
    return this.getUIContext().getHostContext() as common.UIAbilityContext;
  }

  private showError(action: string, error: Error): void {
    this.failed = true;
    this.status = `${action}：${error.message || JSON.stringify(error)}`;
  }

  private async chooseFolder(): Promise<void> {
    if (this.busy) return;
    this.busy = true;
    this.failed = false;
    this.status = '';
    try {
      const options = new picker.DocumentSelectOptions();
      options.selectMode = picker.DocumentSelectMode.FOLDER;
      options.maxSelectNumber = 1;
      const uris = await new picker.DocumentViewPicker(this.context()).select(options);
      if (uris.length === 0) return;
      const path = new fileUri.FileUri(uris[0]).path.replace(/\/+$/, '');
      if (!path.startsWith('/')) throw new Error('请选择本机已解压的游戏文件夹');
      // Check the path Wine will use, not just the document-provider URI.
      const info = await fs.stat(path);
      if (!info.isDirectory()) throw new Error('所选位置不是文件夹');
      this.folderUri = uris[0];
      this.folderPath = path;
      this.exePath = '';
      this.status = '文件夹已授权，请选择其中的游戏 EXE。';
    } catch (error) {
      this.showError('无法打开游戏文件夹', error as Error);
    } finally {
      this.busy = false;
    }
  }

  private async chooseExe(): Promise<void> {
    if (this.busy || !this.folderPath) return;
    this.busy = true;
    this.failed = false;
    this.status = '';
    try {
      const options = new picker.DocumentSelectOptions();
      options.maxSelectNumber = 1;
      options.defaultFilePathUri = this.folderUri;
      options.fileSuffixFilters = ['Windows 程序|.exe'];
      const uris = await new picker.DocumentViewPicker(this.context()).select(options);
      if (uris.length === 0) return;
      const path = new fileUri.FileUri(uris[0]).path;
      if (!path.startsWith(this.folderPath + '/') || path.split('/').includes('..')) {
        throw new Error('请在已授权的游戏文件夹内选择 EXE，或先重新选择文件夹');
      }
      if (!path.toLowerCase().endsWith('.exe')) throw new Error('请选择 .exe 文件');
      if (/["\r\n\x00]/.test(path)) throw new Error('文件路径不能包含双引号或控制字符');
      const info = await fs.stat(path);
      if (!info.isFile()) throw new Error('所选项目不是普通文件');
      const file = await fs.open(path, fs.OpenMode.READ_ONLY);
      await fs.close(file);
      this.exePath = path;
      this.status = '已选择游戏，点击“运行游戏”启动。';
    } catch (error) {
      this.showError('无法选择 EXE', error as Error);
    } finally {
      this.busy = false;
    }
  }

  private async launch(notepad: boolean): Promise<void> {
    if (this.busy || (!notepad && !this.exePath)) return;
    this.busy = true;
    this.failed = false;
    this.status = '正在请求启动…';
    try {
      if (!notepad && !await fs.access(this.exePath)) {
        throw new Error('EXE 已移动或授权已失效，请重新选择游戏文件夹和 EXE');
      }
      const args = this.argumentsText.trim();
      if (!notepad && (args.split('"').length % 2 === 0 || /[\r\n\x00]/.test(args))) {
        throw new Error('启动参数的双引号需要成对，且不能包含换行');
      }
      const key = `launcher-${Date.now()}`;
      // start /unix resolves the Unix path and sets the game's working directory
      // to its parent folder. Pass arguments through Wine, never through a shell.
      const want: Want = {
        bundleName: this.context().abilityInfo.bundleName,
        moduleName: 'entry',
        abilityName: 'EntryAbility',
        parameters: {
          instanceKey: key + ':1',
          processKey: key,
          wineExePath: notepad ? 'C:\\windows\\system32\\notepad.exe' :
            'C:\\windows\\system32\\start.exe',
          wineArgs: notepad ? '' : `/wait /unix "${this.exePath}"${args ? ' ' + args : ''}`
        }
      };
      const options: StartOptions = {
        processMode: contextConstant.ProcessMode.NEW_PROCESS_ATTACH_TO_PARENT,
        startupVisibility: contextConstant.StartupVisibility.STARTUP_SHOW
      };
      await this.context().startAbility(want, options);
      this.status = '已提交启动请求。程序会在独立窗口打开；此提示不代表游戏已成功运行。';
    } catch (error) {
      this.showError('启动失败', error as Error);
    } finally {
      this.busy = false;
    }
  }

  build() {
    Scroll() {
      Column({ space: 18 }) {
        Text('Wine 游戏试运行')
          .fontSize(28)
          .fontWeight(FontWeight.Bold)
        Text('先选择完整的游戏文件夹，再选择其中的 EXE。请先解压游戏，并保留 DLL、数据和资源目录。')
          .fontSize(16)
          .fontColor('#526071')
          .width('100%')
        Button('1. 选择游戏文件夹')
          .width('100%')
          .enabled(!this.busy)
          .onClick(() => { this.chooseFolder(); })
        if (this.folderPath) {
          Text(this.folderPath).fontSize(13).width('100%').wordBreak(WordBreak.BREAK_ALL)
        }
        Button('2. 选择 EXE')
          .width('100%')
          .enabled(!this.busy && this.folderPath.length > 0)
          .onClick(() => { this.chooseExe(); })
        if (this.exePath) {
          Text(this.exePath).fontSize(13).width('100%').wordBreak(WordBreak.BREAK_ALL)
        }
        TextInput({ placeholder: '启动参数（可选，例如 -windowed）', text: this.argumentsText })
          .enabled(!this.busy)
          .onChange((value: string) => { this.argumentsText = value; })
        Button(this.busy ? '处理中…' : '运行游戏')
          .width('100%')
          .enabled(!this.busy && this.exePath.length > 0)
          .onClick(() => { this.launch(false); })
        Button('打开记事本（测试 Wine）', { type: ButtonType.Normal })
          .width('100%')
          .enabled(!this.busy)
          .onClick(() => { this.launch(true); })
        if (this.status) {
          Text(this.status)
            .fontSize(14)
            .fontColor(this.failed ? '#B42318' : '#344054')
            .width('100%')
        }
        Text('首次启动可能需要初始化。运行期间请保留本启动页；关闭游戏窗口后可返回选择其他程序。')
          .fontSize(13)
          .fontColor('#667085')
          .width('100%')
      }
      .padding(28)
      .width('100%')
      .constraintSize({ maxWidth: 680 })
      .alignItems(HorizontalAlign.Start)
    }
    .width('100%')
    .height('100%')
    .backgroundColor('#F5F7FA')
  }
}


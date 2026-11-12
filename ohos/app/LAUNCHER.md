# 游戏 EXE 启动页

桌面图标进入 `LauncherAbility`，原 `EntryAbility` 继续承载 Wine 窗口。

## 使用

1. 将游戏完整解压到系统文件管理器可访问的位置。
2. 打开 Wine，点击“选择游戏文件夹”，授权包含游戏及其资源的目录。
3. 点击“选择 EXE”，选择该目录或其子目录中的游戏程序。
4. 可填写启动参数，然后点击“运行游戏”。
5. 游戏在新进程的窗口中启动。运行期间保留启动页，关闭游戏后可返回选择其他程序。

“打开记事本”使用 Wine 自带程序，不需要外部文件授权。
所选目录和 EXE 仅保存在本次页面会话中；重启应用后需重新选择。
不复制、移动或修改游戏文件。

## 接入

- 新主页：`entry/src/main/ets/pages/Launcher.ets`。
- 新入口：`entry/src/main/ets/entryability/LauncherAbility.ets`。
- `module.json5` 将桌面入口和 mainElement 指向 LauncherAbility，保留原 Wine abilities。
- 使用 `NEW_PROCESS_ATTACH_TO_PARENT` 启动 EntryAbility，每次启动独立 processKey/instanceKey。
- 游戏经现有 `wineExePath` / `wineArgs` 接口执行
  `start.exe /wait /unix "<EXE 的 Unix 路径>"`；
  Wine 的 start 程序会将工作目录设为 EXE 所在目录。
- 参数通过现有 ArgvUtil 解析，不经过 shell。含空格参数使用成对双引号；
  该解析器不支持参数中的字面双引号。
- 只有 startAbility 请求成功时展示“已提交启动请求”，不宣称游戏成功。
  同目录资源访问、游戏图形和退出回收仍需真机验收。

## 验证状态

使用本机 DevEco SDK API 24 对两个新增 ArkTS 文件进行隔离编译与无签名 HAP
打包检查，通过。隔离工程位于忽略目录 `.hvigor/launcher-check`；
其 HAP **只用于界面编译检查，不包含 Wine，不能用于游戏试运行**。
编译器提示文件夹选择系统能力并非所有设备均支持；实际支持情况需在目标平板/电脑确认。

完整项目保持原有 SDK 配置，没有修改 Wine、UBT 或构建脚本。

## 2026-11-11：32 位运行库测试包

选择页已在设备上打开。运行 BlackSouls1 的 32 位 `Game.exe` 时，日志报告
`C:\windows\syswow64\ntdll.dll` 加载失败，错误为 `c0000135`。
用户提供的 Wine 10.12 HAP 只有 aarch64-windows 文件集，缺少 i386-windows。

基于同一源码版本，在独立构建快照中完成 i386 运行文件的编译，向原 HAP 增加
800 个 PE32 文件，包括系统库、Wine 自带程序、32 位 DXVK/vkd3d-proton
及后者所需的 libc++/libunwind。保留此前编译的选择页和原包所有 64 位原生库、UBT。
更新 wine.inf 的注释和时间戳，使 Wine 能检测环境资源更新。

交付文件：
`/Users/zhangpeiyu/Downloads/wine-hmos-pc-arm64-api26-exe-launcher-wow32-unsigned.hap`

SHA-256：`dfc3c00fe14e361a78b93391beaf61063eb812feecebeb6fa3dfc520c1403d65`。
大小 542945339 字节。需要使用与已安装应用一致的签名签名后覆盖安装。
退出旧版后安装，再选择游戏文件夹和 Game.exe；首次初始化可能稍慢。

已通过 ZIP CRC、全部新增 PE32 位数、常规 DLL 导入及函数导入检查，
并按原包 API-set schema 验证 CRT 重定向。原包未提供 GStreamer Unix 视频后端，
ir50_32/wmvcore 的可选延迟导入仍保留这一限制，未宣称视频解码可用。
新包尚未进行设备运行验证，不能据此判定游戏已兼容。

独立构建、重打包脚本、日志和文件清单位于忽略目录 `.hvigor/hap-launcher-patch/`。
HAP 同目录的 `.verification.json` 记录新增文件哈希及静态检查结果。

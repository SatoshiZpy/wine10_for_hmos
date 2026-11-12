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
- 使用 `NEW_PROCESS_ATTACH_TO_PARENT` 启动 `standard` 模式的 ProgramRunnerAbility，
  每次启动独立进程；EntryAbility 保留 `specified` 模式供引擎窗口路由使用。
- 游戏经现有 `wineExePath` / `wineArgs` 接口执行
  `start.exe /wait /unix "<EXE 的 Unix 路径>"`；
  Wine 的 start 程序会将工作目录设为 EXE 所在目录。
- 参数通过现有 ArgvUtil 解析，不经过 shell。含空格参数使用成对双引号；
  该解析器不支持参数中的字面双引号。
- 只有 startAbility 请求成功时展示“已提交启动请求”，不宣称游戏成功。
  同目录资源访问、游戏图形和退出回收仍需真机验收。

## 完整构建包（2026-11-11）

本入口已合入 `codex/quickstart-build-20261111` worktree，由完整工程的
`assembleHap` 正常编译并使用本机已有配置签名。没有复用侧会话重打包的 ABC
或旧版原生核心。当前 HAP 的 `ntdll.so`、`libadapter.so`、`libwineserver.so`
与本 worktree 本轮完整编译包逐字节一致。

- ARM64/ARM64EC Windows 文件：752 个，含 UBT、DXVK、vkd3d-proton。
- i386 Windows 文件：800 个，含完整系统运行库及 32 位图形库。
- x86_64 Windows 文件：738 个。
- ARM64X 和 i386 的 `libc++.dll`、`libunwind.dll` 来自本轮相同 LLVM-MinGW 工具链。
- `build.sh` 已补充 C++ 运行库和 x86_64 文件集的打包步骤。
- ZIP CRC、核心库一致性、三套文件集的常规 DLL 导入依赖检查通过。
- GStreamer 后端仍未提供；`ir50_32.dll` / `wmvcore.dll` 的相关延迟导入尚不可用。

交付包为 worktree 下 `out/wine_hap/wine-exe-launcher-signed.hap`。
检查结果记录在 `.build-logs/launcher-hap-verification.json`；
真机结果另见 `.build-logs/launcher-device-verification.json`。
编译和依赖检查通过不代表游戏已兼容。

侧会话旧混合测试包曾在 32 位程序中出现 SIGBUS；其原因尚未确认，
不能直接归因于 UBT，也不能把旧包结果视作本次完整构建包的运行结果。

## 本包真机结果

在 HUAWEI MateBook Pro（HarmonyOS 7.0.0.111）上保留数据覆盖安装成功。
选择页、目录授权、`Game.exe` 选择和文件读取检查通过；点击“打开记事本”后
实际显示 Wine 记事本窗口。

当前仍有以下问题：

- 记事本实例仍存在时，启动另一程序返回 `The ability is already running`。
  本次通过退出整个应用并重新打开选择页，单独验证游戏。
- 单独启动 BlackSouls1 的 32 位 `Game.exe`，进程运行约 2 秒后崩溃。
  2026-11-11 13:50:42 的新包日志记录 `SIGBUS(BUS_ADRALN)@0x21ade`，
  PC 为 `0x7ff700512c`。根因尚未确定，不能宣称游戏已运行成功。
- 13:45:32 的 Wine 初始化子进程 `rundll32.exe` 也有
  `SIGBUS(BUS_ADRALN)@0x21b0e`；记事本可显示不代表初始化的所有步骤均成功。
- 本次未单独验证最小 32 位 EXE 或带空格路径的 DX9 示例。

相关新包崩溃记录保存在 `.build-logs/launcher-game-crash.txt` 和
`.build-logs/launcher-initialization-crash.txt`。


## 后续修复进展（2026-11-11 下午）

上述交付包和初次真机结果是历史快照，`out/wine_hap` 中的旧包尚未更新。
后续开发包直接从 `entry/build/default/outputs/default/entry-default-signed.hap` 安装。

- 修复未对齐 LDAR/STLR 恢复、Unix 模块路径、内存权限及共享句柄映射后，
  BlackSouls1 已创建窗口并加载 RGSS300.dll，显示缺少 RPGVXAce RTP 的提示。
- 用户安装官方 VX Ace RTP 后，已越过此前的缺失提示；14:53 的运行在
  RGSS300.dll 的 `0x10036360` 发生读 `0xd1f434e8` 异常，游戏尚未正常运行。
- 初始调试器报告为 `Couldn't get first exception` / `No backtrace available`。
  当前诊断包开启 `+seh` 以直接采集异常寄存器。
- 启动入口改为独立 `ProgramRunnerAbility`，避免 `specified` Ability 搭配
  新进程启动选项在已有实例时返回 16000068。已编译并检查包内配置，
  重复启动行为仍待真机复测。


## 自修改代码调查（15:20 后）

独立 x86 模拟展开 RGSS300.dll 后，`0x101c3be8` 的正确值为 `0x101c27e8`
（字符串 `setinlinecache`）。若错误地对第二数据段再次运行 ASPack CALL/JMP
过滤器，恰好得到真机故障值 `0xd1f434e8`。过滤器通过修改自身 `mov bl,imm8`
的立即数来控制只运行一次，因此当前重点是假设 UBT 复用了修改前的翻译缓存。
这是强关联证据，尚未完成真机最小复现或修复验证。

当前实验包添加无游戏依赖的 32 位自修改代码探针：按钮将探针复制到前缀并启动，
结果写入 `C:\wine-smc-probe.log`。探针使用原配置（`EXAGEAR_SAFE_MODE=0`），
游戏使用 UBT 自带兼容模式（`EXAGEAR_SAFE_MODE=1`）作实验。
探针在独立 x86 模拟器的正确结果为 `0,1,2,3`。所有临时诊断均需在交付前清理，
兼容模式的有效性和性能仍待测，不可宣称此包已修复游戏。


## 后续验证与当前实验（15:39 后）

- 自检 v2 真机三项（映像冷执行、映像热执行、动态 RWX）全部 PASS，退出码 0。
  不支持“所有自修改代码均失败”的结论。
- BlackSouls 在 `EXAGEAR_SAFE_MODE=1` 实验下仍于 `0x10036360` 读取
  `0xd1f434e8` 失败；该配置已撤掉。
- 当前实验通过 `WINE_UBT_SMC_SYNC=1`，在 WOW64 当前进程虚拟内存分配、
  释放成功后调用已有 `BTCpuFlushInstructionCache2` 刷新整个 guest 范围。
  Wine 默认不开启；当前应用诊断包显式开启。目的是确认 API 边界是否残留旧翻译。
  该方案可能有明显性能成本，仍属实验，不是已验证的正式修复。


## 音频初始化修复（主动刷新翻译缓存实验之后）

- PID 49795 的 BlackSouls1 已越过原 RGSS 无效指针异常，显示“初始化 DirectX Audio 失败”。
  `dsound.dll`、`mmdevapi.dll`、`wineohaudio.drv` 均加载成功，随后查询 Unix 调用表
  返回 `c0000139`。OHOS 驱动原本只有原生调用表，缺少 WOW64 调用表。
- `wineohaudio.drv/ohos_audio.c` 补齐受支持音频接口的 WOW64 参数转换，涵盖输出指针、
  HANDLE、PROPVARIANT 和 64 位不透明 stream_handle；未实现功能仍保持原状。
  参考现有 ALSA 驱动，在 WOW64 进程中将返回给应用的音频缓冲区限制在 guest 地址范围。
- 本地 ABI 检查七项通过（端点、流、缓冲区、优先级、属性、句柄、MIDI 退出标记）；
  ARM64 原生库和签名 HAP 构建成功，包内库哈希与编译产物一致。
  这些检查不代表真实音频播放已经成功，仍需用户启动游戏验证。
- 保留 `WINE_UBT_SMC_SYNC=1` 实验以继续走到音频路径；其性能和根本修复仍待处理。


## 默认字体兼容（音频 WOW64 修复之后）

- 真机 PID 64044 已不再记录音频 Unix 函数表 `c0000139`，用户反馈“未找到默认字体”。
  尚未听到实际播放，不能据此宣称音频功能全部通过。
- RGSS300 展开映像的默认字体字符串为“黑体”（0x101a255c），而真机字体注册表
  有 `HeiT-SC`、`Noto Sans CJK SC` 等家族，没有该 Windows 名称。
- OHOS 专用默认字体家族映射补充黑体/SimHei、宋体/SimSun、新宋体/NSimSun。
  使用现有家族 replacement 机制，因此枚举和字体选择都能识别别名；已安装或
  用户配置的同名字体优先。系统候选不存在时不创建空映射，不附带第三方字体文件。
- 默认映射、重复初始化、已有配置优先、后备字体和候选缺失五类本地检查通过；
  游戏字体显示效果仍待真机验证。


## 字体映射 UTF-16 修正

首版字体映射仍失败（PID 5270）。原因是 Unix 编译默认 wchar_t 为 32 位，
而映射使用 L 字面量并传给 16 位 WCHAR 指针，名字被截断。原测试使用
-fshort-wchar，错误地掩盖了问题。现改为显式 WCHAR 字符数组及 ASCII 转 UTF-16，
测试取消 -fshort-wchar 并把不兼容指针警告当作错误；旧实现失败、新实现通过。
之前七项音频 ABI 检查不涉及此字体字符串问题。

当前可确认的图形初始化是 ddraw.dll → WineD3D → OpenGL/EGL，日志报告
EGL client extensions 查询失败、GL context 创建失败。尚未看到 D3D11/DXGI
加载或成功游戏画面，不应根据包内 DXVK 配置宣称游戏正使用 DX11/Vulkan。


## BlackSouls1 启动验证通过

用户在字体 UTF-16 修正版覆盖安装后反馈“能跑了，正常”。对应 BlackSouls1
进程 PID 10862 日志已保存，未再出现此前的默认字体错误弹窗。
成功包单独保存在 `out/wine_hap/wine-blacksouls1-verified-signed.hap`，
哈希与验证范围见同名 JSON。原来的 launcher HAP 为历史包，未覆盖。

这是用户确认的启动/正常运行结果，未单独验证长时间游玩、声音完整性或帧率。
该包仍保留翻译缓存主动刷新实验和诊断日志，不应当作性能收尾后的正式版本。
日志仍有 EGL 初始化失败，最终游戏绘制后端尚未直接证实；不能宣称 DX11/Vulkan。


## Gecko 离线安装资源

为避免真机安装器下载缓慢，增加 Wine Gecko 2.47.4 x86/x86_64 官方 MSI 到
`wine_engine/src/main/resources/resfile/share/wine/gecko/`。两份文件均从
`https://dl.winehq.org/wine/wine-gecko/2.47.4/` 下载，SHA-256 与
`dlls/appwiz.cpl/addons.c` 内置校验值一致；包内资源也已核对哈希。
Wine 现有 `WINEDATADIR/gecko` 搜索在联网下载之前，因此无需改动安装器或启动逻辑。
这是离线安装资源准备，不代表 Gecko 已成功安装或新游戏 HTML 功能已经通过。
BlackSouls1 验证包仍单独保留，不受此次资源包构建影响。
证据见 `.build-logs/gecko/offline-assets.json`、`offline-package.json`。


## Probably Stolen：旧图形 DLL 迁移

Probably Stolen 自解压包已直接提取并传回桌面 Probably Stolen-unpacked，
230 个文件全部 SHA-256 核对通过。实际游戏为 x64 Unity。PID 34473 加载
d3d11/dxgi 后在 dxgi+0x40c74 出现写访问异常；尚未正常进入游戏。

从设备取回 prefix/system32 图形库，发现其与当前资源包不同。旧 dxgi 的
ARM64X 两份 LoadConfig 的 DVRT 偏移为 0x385a/0x3870，当前包为一致的
0x389c/0x389c。覆盖安装未更新已有 native DLL，导致旧构建缺陷仍被使用。
WineLoaderAdapter 增加定点迁移：仅匹配两份已知旧 dxgi/d3d11 的 SHA-256，
保留 .pre-engine-update 备份，复制并校验新文件后 rename 替换。其他文件
和用户自定义版本不覆盖。实际迁移及游戏结果待用户启动验证。


## Probably Stolen：独立 ARM64EC 对照

PID 41080 已确认 prefix 的 dxgi/d3d11 哈希与新包一致，迁移成功。日志不再
出现旧 dxgi+0x40c74 的写异常，但仍停止在 DXGI PROCESS_ATTACH；Unity
Player.log 停在 GfxDevice creating device。没有足够证据判定 Vulkan 驱动失败。

为分离 ARM64X 合并与运行时问题，临时把同源码 buildec 的独立 ARM64EC
dxgi.dll 和 d3d11.dll 放入桌面 Probably Stolen-unpacked（此前不存在这两个文件），
传输后校验 SHA-256。没有修改系统图形库或游戏可执行文件。此为诊断对照，
验证后应收敛通用引擎方案并移除游戏目录的临时覆盖文件。
候选文件及路径见 .build-logs/probably-stolen-refresh-run/ec-candidate-check.json。


## Probably Stolen：Vulkan 版本门槛与 legacy 对照

独立 ARM64EC 2.6.2 在 PID 49192 已进入 DXVK，Wine Vulkan 能枚举 Maleoon 916。
日志明确记录 `Skipping Vulkan 1.2 adapter: Maleoon 916`，随后 `No adapters found`，
Unity 返回 887a0002 并显示 Failed to initialize graphics。此轮失败原因是
DXVK 2.6.2 的 Vulkan 1.3 门槛，不能据此判定显卡完全不支持游戏。

构建现有 DXVK legacy 1.10.3 OHOS 源码的独立 ARM64EC 版本（请求 Vulkan 1.1）。
构建脚本改用 -iquote 引用仓库头文件，避免根目录 VERSION 遮蔽 C++ <version>；
新增 DXVK_EC_ONLY=1 选项，默认构建路径不变。构建、ARM64EC/CHPE 元数据检查通过，
依赖 vulkan-1.dll 已确认在设备 prefix 中。

两份 legacy dxgi/d3d11 已放入 Probably Stolen-unpacked，逐文件 SHA-256 校验通过；
原诊断 2.6.2 文件保留为 .dxvk262-backup。此轮不改 HAP、prefix 或 BlackSouls1。
实际创建设备与游戏运行尚待用户复测，不能仅凭 Vulkan 版本判断其他特性是否足够。
证据：.build-logs/probably-stolen-ec-run/ 下的 result.json、legacy-build.log、
legacy-pe-check.txt、legacy-candidate-check.json。


## Probably Stolen：真实能力检查

legacy 1.10.3 的 PID 59024 已识别 Maleoon 916 / Vulkan 1.2.309，版本门槛通过。
Unity 返回 80004005；DXVK 选择 11_1 后创建设备失败。原代码没有记录底层
VkResult，D3D11 捕获 DxvkError 后也丢弃了异常信息，尚不能确定具体缺失能力。

发现 DXVK_RELAX_FEATURES=1 会让 checkFeatureSupport 无条件返回 true，绕过
所有硬件能力检查。删除此提前返回，保留调用方已实现的 BC 特性请求策略；
补齐 geometryStreams 检查，防止只支持 transformFeedback 时错误启用它。
新增 OHOS 放宽模式下的实际支持特性日志，并在创建设备失败时保留 VkResult
和 DxvkError 消息。未伪造能力或直接关闭未实现回退的必需功能。

从实际源码抽取能力检查函数的回归验证 8 项通过，旧代码其中 4 项失败；
独立 ARM64EC 编译成功。游戏目录的两份 DLL 已更新并校验，旧版本保留为
.legacy-before-capability-check；实际运行待用户复测。
证据位于 .build-logs/probably-stolen-legacy-run/，包括 DXVK/Unity/runtime 原始
日志、capabilities-regression.log、capabilities-build.log、capabilities-candidate.json。


## Probably Stolen：geometryStreams 受限兼容路径

PID 7545 的实际能力日志确认 geometryStreams、vertexPipelineStoresAndAtomics、
variableMultisampleRate 均为 0。前者被旧 DXVK 请求为所有 10_0 及以上级别的
必需能力；后两者还阻止 11_1。Unity 最终 80070057，尚未进入游戏。

在 OHOS 且 DXVK_RELAX_FEATURES=1 时，geometryStreams 改为按设备实际能力
启用；其他能力仍严格检查，使本机能力集合可以选择受限 11_0。该编译器的
流输出路径即便 stream=0 也会声明 GeometryStreams，故当能力未启用时，
CreateGeometryShaderWithStreamOutput 明确返回 DXGI_ERROR_INVALID_CALL 并记录
原因，避免把不支持的着色器交给驱动。此方案不实现流输出，也不代表完整
D3D11 功能达标；游戏使用该接口时仍需后续适配。严格模式和完全支持设备
的能力请求不变。Vulkan 对 geometryStreams 的定义参见：
https://docs.vulkan.org/refpages/latest/refpages/source/VkPhysicalDeviceTransformFeedbackFeaturesEXT.html

使用真机捕获特性作为输入、执行实际 GetDeviceFeatures/checkFeatureSupport
函数的 7 项回归检查全部通过，修改前 3 项失败；独立 ARM64EC 构建成功。
设备曾短暂断开，重连后完成两份游戏目录 DLL 的备份、替换和 SHA-256 校验。
旧文件保留 .before-limited-streams，未改 HAP/prefix。尚待用户启动验证。
证据：.build-logs/probably-stolen-capabilities-run/ 中的原始日志、result.json、
feature-level-regression.log、limited-features-build.log、limited-features-candidate.json。


## Probably Stolen：更换设备并恢复 DXVK 2.6 对照

新设备 8AC0226928004070 / MOR-M2，系统 OpenHarmony-7.0.0.109。用户说明
新设备支持 DXVK 2.6，但尚未从应用内日志验证 GPU/Vulkan 能力。设备初始
未安装 com.huawei.ohos_wine，桌面没有游戏。已传输解压后的 230 个文件并
全部校验 SHA-256，再放入原始独立 ARM64EC DXVK 2.6.2 的 dxgi/d3d11，
哈希核对成功。未沿用旧设备的 legacy 受限功能覆盖。

安装现有签名 HAP 被系统拒绝：9568423，设备 UDID 未包含在签名 profile。
已请求用户在 worktree 工程的 DevEco Signing Configs 更新新设备自动签名，
当前尚未安装成功。原生 Vulkan 只读探测程序在 /data/local/tmp 执行被拒绝，
已移除临时设备文件，待应用安装后从运行日志核实实际能力。
证据：.build-logs/probably-stolen-new-device/。

新设备签名更新后，重新构建 HAP（11.7 秒成功）并安装，hdc 返回 install bundle
successfully，bm dump 确认安装。签名包哈希见 resigned-package.json。游戏目录
两份 DXVK 2.6.2 哈希再次核对通过；等待用户在新设备首次启动 Wine/游戏。


## Probably Stolen：新设备 DX11 成功，音频可选参数崩溃

新设备首次游戏 PID 54816，DXVK 2.6.2 创建设备成功。Unity Player.log 明确记录
Direct3D 11.0 [level 11.0]、Maleoon 935、31562 MB。未出现可游玩画面；本轮退出
发生在音频初始化，而非旧设备的 Vulkan 版本门槛。

Unix call index 17 为 get_device_period，故障 PC 与 ohos_get_device_period+0x54
对应（str x8,[x9]，x9=0，x8=50000）。OHOS 驱动无条件解引用 min_period；
mmdevapi 的 GetDevicePeriod 允许单个输出为 NULL。异常返回 c0000005 后触发
mmdevapi_private.h 的 assert(!status)，进程退出码 3。

修复为分别检查 def_period/min_period 再写入，与现有 ALSA 驱动处理一致，
不移除断言、不屏蔽音频。直接提取实际 native 函数和 WOW64 包装执行回归：
64 位/32 位分别测试默认周期单独、最小周期单独、两者同时三种组合；新代码
6 项通过，旧代码 4 项复现 SIGSEGV。

独立音频库增量构建、签名 HAP 构建和包内 SHA-256 核对通过；使用 -r 覆盖
安装，保留游戏及 prefix。真机复测结果需以随后启动日志为准。
证据：.build-logs/probably-stolen-new-device/run1/。


## Probably Stolen：Vulkan 客户端表面窗口句柄

音频修复后 PID 60358 不再出现 get_device_period 空指针/断言，Unity 已完成
Mono reload 和输入初始化。随后切换 3120x1955 全屏失败，DXVK 创建交换链时
报告 VK_ERROR_OUT_OF_HOST_MEMORY；Wine 日志先记录 ohos_client_surface_create
失败。该错误是表面创建失败的统一映射，不能据此推断物理内存耗尽。

发现 ohos_client_surface 的私有 hwnd 未赋值：win32u client_surface_create
只初始化基类 client.hwnd；OHOS 绑定/更新/销毁流程却读取私有 surface->hwnd。
calloc 留下的零值导致 get_win_data(NULL) 失败，无法绑定已存在的窗口 0x30074/2。
在创建后立即保存 hwnd，覆盖立即绑定及延迟绑定路径。

实际创建函数的宿主隔离回归 5 项通过，旧代码 3 项失败；验证了立即/延迟
绑定窗口传递以及分配/绑定失败清理。Wine OHOS 驱动增量构建完成，签名 HAP
随后打包覆盖安装，真机显示结果仍需复测。
证据：.build-logs/probably-stolen-new-device/run2/。


## Probably Stolen：完整 SIGSEGV 退出现场与内存保护诊断

窗口句柄修复后的 PID 4235 通过 DX11 设备初始化，Player.log 记录 Maleoon 935、
Direct3D 11.0、Mono reload 和输入初始化。18:03:55 的首份 cppcrash 并非最终
退出时间，进程随后继续执行；持续 hilog 记录最终在 18:04:07.627 以 signal 11
退出。不能只根据最早的 Mono memcpy/UBT JIT 栈判断最终根因。

运行日志包含 5949 次 native syscall 访问异常。符号与反汇编核对到
NtAlertThreadByThreadId+0x110 和 NtWaitForAlertByThreadId+0x108 的 stlxr，
目标为 0x5ab2e3020c 等同一页上的内部 tid alert futex 表项。表按实现应通过
anon_mmap_alloc(PROT_READ|PROT_WRITE) 分配，但当前尚未确定保护失效原因。
没有据此修改同步语义或屏蔽 SIGSEGV。

新增临时有界诊断：mprotect 失败的地址/长度/权限/errno；write fault 前后
vprot、对应 view 和处理结果；tid alert block 分配地址和初值。启动日志暂时
增加 +virtual，以追踪分配、保护和释放。完成 ntdll 增量编译、签名打包及
包内 ntdll/音频/窗口三份原生库哈希核对。此版本为诊断包，非已验证修复。
证据：.build-logs/probably-stolen-new-device/run4/；连续系统日志在 run3/live-hilog.log。


## Probably Stolen：固定地址分配覆盖宿主同步表

诊断包 PID 18040 确认：NtAllocateVirtualMemory 请求 0x5ab2e30000/0x10000，
紧接着 map_fixed_area 的 EEXIST 分支调用 remove_reserved_area 解除整个区间
映射并重试成功。随后游戏把该区间首个 4KB 保护为只读，原生
NtAlertThreadByThreadId/NtWaitForAlertByThreadId 仍访问其中的同步表，发生
重复写入异常。此前 native 映射不在 Wine views_tree 中，因而 find_view_range
不能证明地址空闲。该分支也不是注释所声称的“仅限固定基址 EXE”，普通
VirtualAlloc 的指定地址请求同样会进入。

删除 EEXIST 后强制解映射/重试的分支，返回 STATUS_CONFLICTING_ADDRESSES。
保留已有 Wine 预留区映射和失败时仅撤销本次已成功映射前缀的逻辑，不改变
同步表布局、不放宽内存保护、不忽略异常。移除 run4 临时诊断及 +virtual。

从实际源码提取 map_fixed_area、anon_mmap_tryfixed、unmap_area 等函数，在
本地 Linux 容器使用真实 mmap/mprotect/munmap 执行宿主隔离回归：10 项均通过，
旧代码 6 项失败。覆盖读写/只读宿主映射、部分重叠、哨兵字节保持、空闲固定
地址、既有 Wine view、自有预留区和未对齐地址。该测试未替代真机游戏验收。
证据：.build-logs/probably-stolen-new-device/run5/，生成测试的脚本为
.build-cache/check-fixed-map.py。新签名包真机游戏结果待复测。

新包完成签名构建及包内三份原生库哈希核对，hdc install -r 返回 install bundle successfully。已请求用户复测，持续日志为 run5/live-after-fix.log。


## Probably Stolen：固定地址修复后首次出画面，后续嵌套异常

PID 24867 不再出现 native handle_syscall_fault（运行日志计数为 0）。DXVK 明确
记录 Presenter: Actual swapchain properties，RGBA8、FIFO、3120x1955、4 张图像。
用户确认短暂看到画面后退出；这证明显示路径有进展，不能视为游戏稳定可玩。
Player.log 新增触摸初始化与 UnloadTime。

持续 hilog 最终捕获 18:38:06.646 的 nested exception on signal stack，PC 为
0x5a2bc23bcc，SP 为 0x7ffcfe60；PC 位于系统 [shmm] 可执行共享映射。
18:38:08.733 appspawn 记录退出码 0，不能沿用上一轮最终 SIGSEGV 退出结论。
最早 18:37:45 的 Mono/UBT cppcrash 仍早于最终退出，不能替代最后现场。

增加仅嵌套异常触发的临时定点诊断：异常码、参数、ESR、PSTATE、寄存器、
故障位置 8 条指令与 256 字节栈内容。内存通过 process_vm_readv 读取，避免
诊断直接解引用不可读地址。保留原异常退出行为，不屏蔽异常。原生库编译
通过；新诊断包随后打包安装，待用户复测。
证据：.build-logs/probably-stolen-new-device/run6/。


## Probably Stolen：启动成功反馈与持续爆音修复

用户在 PID 36360 运行后确认能启动，但背景音乐持续断续/爆音。此轮未再修改
异常处理行为，因此不能声称上一轮嵌套异常已被定点诊断“修复”；仍需关注
其间歇性。当前启动包备份至 out/wine_hap/wine-probably-stolen-starts-signed.hap，
SHA-256 记录在 run7/result.json。日志另有主线程临界区等待超时，游戏长期
流畅性并未验收，不将启动成功扩大为全部稳定性通过。

音频回调存在独立可复现缺陷：while 每次按原始 held_frames 限制读取量，却只在
循环结束后扣减 held_frames，导致请求大于现有数据时读取未提交/已消费内容，
推进读指针越过生产者并错误归零 padding。改为一次计算 min(requested, held)，
至多两段复制处理环形尾部，然后仅推进实际消费帧数，剩余区域补静音。
8-bit PCM 静音值同步纠正为 0x80，移除实时回调中逐次 underrun 日志。

提取实际 callback/silence_buffer，在 Linux 容器以 ASan+UBSan 执行 7060 组
8/16-bit、单/双声道、空/部分/满缓冲、所有环形偏移和非整帧尾部组合；新代码
全部通过，旧代码 5686 组失败。Mac ASan 在初始化阶段卡住，因此该平台测试
没有计为通过；实际结果来自 Linux。原生音频库和签名包已构建，真机听感待测。
证据：.build-logs/probably-stolen-new-device/run7/。

音频修复签名包及三份原生库哈希核验通过，覆盖安装成功。安装前确认游戏进程已退出。已请求用户在同一背景音乐下复听 30 秒；实际听感尚未确认。


## Hollow Knight：DXGI 初始化阶段翻译器递归

用户请求检查桌面 Hollow Knight（非 Silksong）。hollow_knight.exe 为 x86-64。
最近启动 PID 57238 来自 Huawei Share/Hollow Knight；19:41:40 的故障报告记录
UBT +0x24ef8/+0x14f50 反复递归，SP=0x6634000fe0，写入栈保护地址0x6634000ff0。
Wine 日志最后停在公共 system32/dxgi.dll 的 PROCESS_ATTACH CALL，没有返回。
尚未进入 DX11 设备创建，因此当前不据此判断 GPU 能力不足。

桌面新拷贝 /storage/media/100/local/files/Docs/Desktop/Hollow Knight 原本没有
本地 dxgi.dll/d3d11.dll。放入与 Probably Stolen 同哈希的独立 ARM64EC DXVK
2.6.2 两份库，临时文件传输并校验后改名，最终哈希再次核验。未替换公共
prefix，也未安装 HAP。需要用户从桌面新路径启动，避免复用 Huawei Share 旧路径。
证据：.build-logs/hollow-knight/run1/。是否越过初始化仍待真机验证。


## Hollow Knight：原生 BC 纹理支持被强制禁用

桌面新路径 PID 5576 已越过 DXGI 初始化并进入 Unity 6000.0.61f1 主菜单。
用户反馈纹理异常；run2/screen.jpeg 可见标题、背景和部分图标花屏，文字正常。
运行日志两次记录启用 textureCompressionBC=1，但 DXGIVkFormatTable 仍因
DXVK_RELAX_FEATURES 强制把所有 BC 格式映射到未压缩纹理。代码只有
InitDeviceLocalTexture 调用 DecodeD3D11BcImage；UpdateTexture/UpdateImage
仍上传原始 BC 数据到重映射后的纹理，存在格式和数据布局不匹配。

改为根据逻辑设备实际启用的 textureCompressionBC 决定是否保留原生 BC，
与其他能力检查放宽开关解耦；缺少 BC 支持的兼容分支暂时保留，其完整更新/
复制语义仍未补全。本轮仅针对当前支持 BC 的设备验证。
独立 ARM64EC dxgi.dll 编译成功，备份旧库后以临时文件校验并替换桌面游戏
目录内 dxgi.dll，未修改公共 prefix 或 HAP。对象头 0xa641，最终 PE 为
0x8664 + CHPE 元数据；最终 SHA-256 和备份见 run2/native-bc-deployment.json。
已请求用户重启游戏；画面修复效果待复测，尚不宣称已解决花屏。


### BC 修复部署补正

run3/PID 24959 仍记录旧的强制 remapping，未出现 native BC 日志。查明
d3d11/meson.build 的 dxgi_common_src 同样静态编译 dxgi_format.cpp，上一轮
仅更新 dxgi.dll 未覆盖实际 D3D11 纹理表实现。故上一轮用户反馈不能用于
判断原生 BC 路径的渲染效果。现补编 d3d11.dll，通过编译、两份 DLL 新标记
检查及设备 SHA 校验；旧 d3d11.dll 保存于 run3，最终记录 deployment.json。
两库均已更新，下一次启动需核对 native BC texture compression enabled: 1
并检查截图。当前尚未完成真机画面验证。


## DXVK 性能悬浮层（按游戏配置）

在游戏 EXE 同目录放置 dxvk.conf，当前启动器的 start /unix 会将工作目录
设到游戏目录，DXVK 默认读取其中配置。当前 Hollow Knight 已部署：

```ini
dxvk.hud = devinfo,fps,frametimes,gpuload,memory,compiler,scale=1.5
```

重启游戏生效。显示显卡信息、FPS、帧时间曲线、GPU 忙碌比例、DXVK 内存
统计和编译提示；不是整机 CPU/GPU 使用率或显示面板刷新率。关闭可改为
`dxvk.hud = 0`。可复用到使用 DXVK 的其他游戏；不覆盖纯 OpenGL/GDI。
本次未更换 HAP 或运行库，当前游戏效果待用户重启验证。
证据：.build-logs/hollow-knight/hud/。
另 PID 44171 已确认原生 BC 日志 enabled: 1，截屏时画面为黑屏，
尚不足以宣称此前纹理花屏问题已解决。


## Hollow Knight 卡顿采样（未修复）

PID 7700 的 HUD 7.3 FPS / GPU 10%。hiperf 10 秒 16537 样本无丢样，热点包含
未对齐访存 SIGBUS 恢复与 musl 信号链日志。hitrace 20 秒采集因环形覆盖保留
4.861 秒；43 次 QueuePresentKHR，相邻间隔平均 113.12 ms、最大 206.49 ms，
提交函数本身平均 2.51 ms。不能将自动报告的 1649 系统帧当作游戏帧。
资源元数据确认两张 8192×8192 BC7、两张 4096×8192 BC7，尚未关联当前关卡。
未改运行库、游戏资源或日志设置。等待同关卡重启后作日志级别对照。
证据及分析报告：.build-logs/hollow-knight/perf1/report.html。


### Hollow Knight 日志级别对照未生效

PID 38394，20:44:14—20:44:46 临时 hilog -b E -T MUSL-SIGCHAIN，
参数设置成功但仍记录 3077 条该进程 W 级信号链日志。因此不能把 trace FPS
5.81→8.40 解读为收益（用户移动且采集有效跨度不同）；HUD 仍为个位数。
5 秒复采 8952 样本无丢样，musl 信号链和未对齐模拟仍是热点。
结束后 param set hilog.loggable.tag.MUSL-SIGCHAIN 空字符串，读回空值，
恢复继承全局 I。musl.log.level/enable 读取权限错误 1002，未修改。
本轮未修改运行库，后续需进程内针对 SIGBUS 热点优化。
证据：.build-logs/hollow-knight/perf-ab/report.html。


## Hollow Knight SIGBUS 快速入口（待真机验证）

根据两轮采样，新增 opt-in WINE_OHOS_FAST_SIGBUS=1。仅 OHOS ARM64：
在既有特殊信号处理链建立后读取内核 SIGBUS action，保留 flags/mask/restorer，
只替换入口；现有 LDAR/STLR 模拟成功时直接返回，其他信号/不支持指令/
无法访问地址仍转发到保存的原始 SA_SIGINFO 入口。查询或安装失败、默认/
忽略处理、非 SA_SIGINFO 或 SA_RESETHAND 均不启用快速入口。没有修改指令
模拟的 process_vm_readv/writev 权限检查和 acquire/release fence。
原始探针计数改为达到 16 次后只读，避免所有信号仍写同一共享计数及溢出。

宿主回归：提取实际快速入口与安装代码，用 mock 内核 action 检查 opt-in、
查询/安装失败、disposition guards、flags/mask/restorer 保留、fallback 和幂等性；
通过。既有未对齐访存 66 组指令/结果/故障回归通过。独立 OHOS 原生探针
直接执行及 loader 执行都被设备拒绝，不计为真机测试成功。

构建保留当前已安装 6591f601... 包中的 Silksong 地址范围与 UBT host-protect
改动：以其隔离构建对象为基线，只重编 signal_arm64.o；应用也从该版本
快照构建，只新增环境变量。包内所有其他 .so/.dll/.exe SHA256 与基线一致。
变更条目 ntdll.so、ets/modules.abc、sourceMaps.map、打包生成的 .pages.info。
新包 a18e0c631d3faec54f1b1b48936ca769e31b4c4d3a0f03220e5e36d816f80bb1。
尚未安装；用户需要先保存并退出当前游戏。证据及包路径见
.build-logs/hollow-knight/sigbus-fast/package-manifest.json。

SIGBUS 快速入口测试包于用户退出游戏后覆盖安装成功，数据保留；实际启用日志及游戏帧率待复测。

SIGBUS 快速入口已在 PID 16291 运行日志确认启用。8 秒 hiperf 18800 样本无丢样，
musl 模块采样占比由此前 44.31% 降至 1.17%；热点转为未对齐模拟自身。
本轮渲染 trace：171 次提交、跨度 5.969 秒，平均间隔 35.11 ms，28.48 FPS，
P95 43.91 ms，最大 77.04 ms，无 ≥100 ms 间隔。截图瞬时 17.2 FPS。
与此前 8.84 FPS / 113.12 ms 采样相比改善，但场景时刻不同，不宣称固定倍数
或全程稳定 28 FPS，长期及其他游戏回归待做。证据见 sigbus-fast/verify/report.html。


## Hollow Knight 批量读取优化（2026-11-11，待真机复测）

快速入口启用后的第二轮采样 PID 16291：10 秒 hiperf 21903 样本，无丢样。
线程 16548 的 handler/emulator 为 7.08% / 5.80%，线程 16538 为 5.23% / 4.16%。
另采 6 秒渲染 trace 得到 124 次提交，跨度 5.979 秒，20.57 FPS，平均间隔
48.61 ms，P95 73.50 ms，最大 109.995 ms，2 个 >=100ms 间隔。与前次场景
时刻不同，不能把 28.48→20.57 解释为版本回退。分析器系统帧数不是游戏 FPS；
本轮不采 sched，CPU 归因使用独立 hiperf。报告 perf-fast2/report.html。

新增 opt-in WINE_OHOS_BATCH_UNALIGNED_READS=1（默认由测试启动器设置）。
256 项 PC/指令预测表仅用于 LDAR：一次 process_vm_readv 依次读取实际指令和
预测数据；完整返回且实际指令逐位一致时才提交寄存器和 PC，保留 acquire fence。
还要求预测地址等于 si_addr。不命中、地址不符、JIT 改写、短读均回退；不预测
写入、不对预测地址调用 virtual_handle_fault，不缓存权限或数据。
表字段 relaxed atomic 访问；并发读到混合预测同样经过实际指令校验。
特殊信号处理器复用已安全读取的指令解码，避免原先第二次直接读取代码地址。

Linux 实际 process_vm_readv + ASan/UBSan 回归 40779 例通过，包含全位宽/寄存器、
SP/XZR、四线程槽冲突、指令改变为不同位宽/寄存器/写入、跨页/PROT_NONE/解除映射；
既有 66 例模拟及信号链回退测试通过。测试脚本 .build-cache/sigbus-batch/test-batch.py。
只重编上次已验证对象集的 signal_arm64.o，保留其他线程的 Silksong 等修复。
2137 个 native/PE 条目比较仅 ntdll.so 改变；另有启动环境变量产生的 abc/sourceMap。
HAP SHA256: 25ba5e587fb261b7b2e85869711dc328a0d6f487696a97acb50c2d6b8372f778。
ntdll SHA256: 2622630db3dc39238110437d1c763c96d3715d92059690cd4c5c26c4c0dfba0f。
构建包 .build-cache/sigbus-batch/app/entry/build/default/outputs/default/entry-default-signed.hap。
尚无新版本真机帧率结论；用户已退出游戏，开始覆盖安装。

用户退出后，批量读取优化包覆盖安装成功（install bundle successfully），未卸载或清理数据。等待用户进入相同关卡后核验 BATCH read verified 探针、运行日志及复采帧率/CPU。


批量读取真机复测 PID 56801：日志明确启用，探针 BATCH read verified / CHAIN batched。
10 秒 hiperf 20535 样本无丢样，ntdll 模块占比前次 26.04% 本次 19.43%，但不同场景/负载不作因果提升结论。
6 秒渲染采样 18.876 FPS，P95 90.387ms，最大 374.336ms，6 个 >=100ms 间隔；截图 HUD 9FPS。
后续独立调度采样 4.022 秒，22.662 FPS，最大 105.086ms；不得直接用后者归因前者 374ms 长帧。
线程 57160 运行 3.216s，R/R+ 合计 0.514s；UnityGfxDeviceW 57151 运行 2.817s，R/R+ 0.472s。
dxvk-submit 运行 0.143s、睡眠 3.588s，QueuePresentKHR 平均耗时 1.277ms，最大 10.079ms。
好压 PID41281 运行合计 3.448 CPU 秒/4.022 秒窗口，是额外后台负载，未擅自停止。
用户已收到等待解压完成、保持同一画面再复采的请求；游戏仍存活，未重装或改运行配置。
证据 .build-logs/hollow-knight/sigbus-batch/verify/report.html。净帧率收益尚未确认。


2026-11-11 21:47–21:49 后台空闲复采，仍为 PID56801 批量读取版。
第一段用户确认正在切场景：15.84FPS，最大478.606ms，7个>=100ms；单独保留不作稳定场景比较。
正常画面恢复后：183次提交/7.941s，22.918FPS，均值43.634ms，P95 53.045ms，最大77.983ms，无>=100ms。
后续独立4.022s调度采样23.213FPS，最大69.769ms；好压CPU仅0.002368s（单核约0.059%）。
游戏CPU合计11.220s/4.022s≈2.79核；两个热点线程运行2.983s/2.915s，R/R+等待0.622s/0.567s。
QueuePresentKHR调用平均0.950ms、最大2.686ms；供帧仍主要受CPU侧处理影响。
8s hiperf19846样本无丢样，fast SIGBUS在多个线程仍为热点（7.31%、6.39%、2.34%、1.17%）。
CPU采样前段与trace传输重叠，仅用作热点定位；帧率和调度采集独立完成。
画面与旧版本不一致，不宣称严格A/B收益；本轮未改代码/包、未操作游戏或停止其他应用。
证据 .build-logs/hollow-knight/sigbus-batch/verify-idle/report.html。


## Sekiro BC 回退及显示诊断（2026-11-11 晚）

- `WineSubWindow` 补齐 ClientSurfaceManager 注册/销毁和 Vulkan client XComponent，
  解决 `renderer of window 2 not found` 导致的 surface 创建失败。
- DXVK 按 BC 格式族逐一检查支持情况，BC4/5/6/7 不支持时映射到解码后的
  R8/RG8/RGBA16F/RGBA8，并更新 view family；补齐 UpdateImage/动态映射上传。
  BC4/5 signed/unsigned、BC7 mode 6 及边界尺寸/跨层 pitch 共 255 个断言通过
  ASan/UBSan。BC6 及 staging readback 路径尚未完整覆盖。
- PID 39975、47284 均记录 BC4–7 CPU fallback；纹理创建失败从 84 条降至 0。
  黑屏未完全解决，不能将这些结果视为游戏兼容通过。
- 显示实验：WineSubWindow 两个 XComponent 使用 RESIZE_FILL，开启游戏目录
  `dxvk.conf` 的 `fps,drawcalls,submissions,compiler` HUD。PID 47284 的 HUD
  可见，起初约 56 FPS / 0 draw，随后变成约 42–46 FPS / 4 draws / 2 passes，
  中央出现黑色区域。尚未对 TOP_LEFT 与 RESIZE_FILL 做独立 A/B，不能把
  RenderService dump 中的 `Frame[-inf ...]` 单独认定为显示错误。
- 8 秒 CPU 采样显示 Core.Res.PostPr 大量时间进入 ohos_fast_sigbus_handler；
  后续热点从 tid 47525 切换到 47524，工作集继续增长，说明有加载进展，
  未证实死锁。原始日志/采样汇总位于 `/tmp/codex-sekiro-surface-20261111/`。
- 另外发现 `get_window_features_for_style` 以缺少 WS_CAPTION 为由禁止激活。
  Sekiro 使用未拥有的 WS_POPUP 窗口，点击后鸿蒙焦点仍停留于启动器。
  新修复在 HWND 层允许无 owner、非 tool/tooltip/WS_EX_NOACTIVATE 的
  顶层 popup 激活；保留 owned popup 现有策略。测试包位于
  `.build-cache/sekiro-focus/app/entry/build/default/outputs/default/entry-default-signed.hap`，
  已编译并核对包内仅 wineohos.so 和 source map 变化，尚未安装/验证。

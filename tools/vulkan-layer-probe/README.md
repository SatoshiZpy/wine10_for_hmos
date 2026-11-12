# Vulkan Layer 探针

用于验证第三方 Vulkan layer 能否在 Wine 应用进程内拦截 Vulkan 调用，
并记录游戏运行时实际用到的 vk 接口。**不进入默认构建**，只在诊断时临时接入。

## 鸿蒙 loader 的 layer 发现机制

设备 `/system/lib64/libvulkan.so` 没有导入 `getenv`，`loader_getenv` 全部通过
`CachedParameterCreate`（OHOS 系统参数）读取。`VK_LAYER_PATH`、`VK_LOADER_DEBUG`、
`HOME`、`XDG_*` 会被当作系统参数名解释，等效于永远未设置，所以 `setenv` 无效。

layer 通过以下系统参数发现：

| 系统参数 | 作用 |
|---|---|
| `debug.graphic.debug_layer` | manifest 文件基名（不含 .json） |
| `debug.graphic.debug_hap` | 必须等于当前应用 bundle 名（比对 bundle 名，而不是模块名） |
| `debug.graphic.vklayer_json_path` | 直接指定 manifest 完整路径（优先于拼接） |
| `debug.graphic.system_layer_flag=1` | 改从 `/vendor/etc/vulkan/debuglayer/` 读取，要求 debug 签名 |

默认 manifest 路径（HAP 模块名须为 entry）：
`/data/storage/el2/base/haps/entry/files/<debug_layer>.json`。

`vklayerprobe.c` 在 EntryAbility onCreate 时写入**隐式层** manifest，
因此 DXVK 这类不请求 layer 的客户端也会被挂上。

## 使用

1. 编译 layer 和 napi 模块（输出到 `ohos/app/entry/libs/arm64-v8a/`）：

   ```bash
   tools/vulkan-layer-probe/build.sh
   ```

2. 临时接入 entry 模块：
   - `entry/oh-package.json5` 的 dependencies 加
     `"libvklayerprobe.so": "file:../../../tools/vulkan-layer-probe/src/types/libvklayerprobe"`；
   - `EntryAbility.onCreate` 在 `super.onCreate` 之前调用：

     ```ts
     import { run as runVkLayerProbe } from 'libvklayerprobe.so';
     ...
     runVkLayerProbe();
     ```

3. 用**正常的完整构建**打包安装。不要拿旧 HAP 或其他 worktree 的
   `wine_engine/libs` 拼包：2026-11-12 曾因探针包带着旧版 `ntdll.so`
   丢失 fast SIGBUS 支持，导致 Sekiro 报 `Internal error 0x0A`。

4. 设置参数（重启后丢失）：

   ```bash
   hdc shell param set debug.graphic.debug_layer codex_layer_probe
   hdc shell param set debug.graphic.debug_hap com.huawei.ohos_wine
   ```

5. 日志 tag 为 `CodexVkLayer`，trace 写入
   `/data/storage/el2/base/haps/entry/files/vk-trace/trace-<pid>.log`。

## 诊断结束后

撤掉第 2 步的改动、删掉 `entry/libs/arm64-v8a/` 下的两个探针 so，并清空参数，
否则所有 Wine 进程都会挂上这个 layer：

```bash
hdc shell param set debug.graphic.debug_layer ''
hdc shell param set debug.graphic.debug_hap ''
```

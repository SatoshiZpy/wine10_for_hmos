#!/bin/bash

# ============================================================
# 准备 arm64ec 运行时 sysroot (方案A, 不编译任何东西)
#
# 不下载/不解压 llvm-mingw: 工具链视为只读外部资源, 仅认环境变量
# LLVM_MINGW_SYSROOT (指向 llvm-mingw 工具链根目录, 即官方 release
# 解压后的 llvm-mingw-<tag>-ucrt-...-x86_64 目录), 与 env.sh 的
# OHOS_SDK / LLVM_TOOLCHAIN 同一模式: 缺失或组件不全即硬失败并
# 提示 export 语句。
#
# 只取其中的 arm64ec-w64-mingw32 目录作为 sysroot, 供 LLVM_TOOLCHAIN
# 的 clang 编译 DXVK arm64ec 时使用:
#
#   clang --target=arm64ec-w64-mingw32 --sysroot=${EC_SYSROOT} ...
#
# sysroot 内容: mingw-w64 头文件, CRT, compiler-rt builtins,
# libc++/libunwind, 全套 mingw-w64 导入库 (含 DXVK 需要的
# d3d9/d3d11/dxgi/gdi32/version)
#
# 注意: 环境变量名不能复用 LLVM_MINGW —— env.sh 的 _find_llvm_pe
# 已把它当 PE 编译器路径消费 (会 export LLVM_TOOLCHAIN=$LLVM_MINGW),
# 单独设置时会让 Wine PE 侧意外换用 llvm-mingw 的 clang。
# ============================================================

set -ex

# ============================================================
# 1. 发现 (仅环境变量 LLVM_MINGW_SYSROOT, 不回退、不默认)
# ============================================================
if [ -z "${LLVM_MINGW_SYSROOT:-}" ] || [ ! -d "${LLVM_MINGW_SYSROOT}/arm64ec-w64-mingw32" ]; then
    echo "ERROR: llvm-mingw 未就绪——arm64ec 构建中断。须用环境变量指定工具链根目录:"
    echo "  export LLVM_MINGW_SYSROOT=/path/to/llvm-mingw-<tag>-ucrt-ubuntu-22.04-x86_64"
    echo "路径须存在且含: arm64ec-w64-mingw32/ (sysroot)、lib/clang (compiler-rt)"
    echo "下载: https://github.com/mstorsjo/llvm-mingw/releases"
    echo "当前 LLVM_MINGW_SYSROOT=${LLVM_MINGW_SYSROOT:-<未设置>} (若已设置, 请检查路径存在性与上述组件)"
    exit 1
fi

# 导出给 build_dxvk.sh 使用的变量
EC_SYSROOT="${LLVM_MINGW_SYSROOT}/arm64ec-w64-mingw32"

# ============================================================
# 2. 组件校验
# ============================================================
if [ ! -d "${EC_SYSROOT}" ]; then
    echo "ERROR: arm64ec sysroot missing: ${EC_SYSROOT}"
    echo "该 release 可能未捆绑 arm64ec 目标, 换更新的 tag 重试:"
    echo "https://github.com/mstorsjo/llvm-mingw/releases"
    exit 1
fi

# 2.1 DXVK 链接需要的导入库
for lib in d3d9 d3d11 dxgi gdi32 version; do
    if ls ${EC_SYSROOT}/lib/lib${lib}.a >/dev/null 2>&1; then
        echo "OK: import lib ${lib}"
    else
        echo "MISSING: import lib ${lib} in ${EC_SYSROOT}/lib"
        exit 1
    fi
done

# 2.2 C++ 运行时 (libc++ 头文件 + 静态库)
[ -d "${EC_SYSROOT}/include/c++/v1" ] || { echo "ERROR: libc++ headers missing"; exit 1; }
ls ${EC_SYSROOT}/lib/libc++*.a >/dev/null 2>&1 || { echo "ERROR: libc++ static lib missing"; exit 1; }

# 2.3 compiler-rt builtins: arm64ec 复用 aarch64 变体, 位于 llvm-mingw
#     的 clang resource 目录 (通用 clang 发行包只有 linux-x86_64 的)。
#     通过 -resource-dir 指过去, -rtlib=compiler-rt 即可自动解析
EC_BUILTINS_LIB=$(find ${LLVM_MINGW_SYSROOT}/lib/clang -path '*/lib/windows/libclang_rt.builtins-aarch64.a' 2>/dev/null | head -1)
if [ -z "${EC_BUILTINS_LIB}" ]; then
    echo "WARN: compiler-rt builtins (aarch64) not found in llvm-mingw, 链接期若出现 undefined __multi3/__ashlti3 符号需补齐"
else
    # builtins 路径 <toolchain>/lib/clang/21/lib/windows/xxx.a
    # resource 目录即向上三级: <toolchain>/lib/clang/21
    EC_RESOURCE_DIR=$(dirname $(dirname $(dirname "${EC_BUILTINS_LIB}")))
    echo "OK: builtins ${EC_BUILTINS_LIB}"
    echo "OK: clang resource dir ${EC_RESOURCE_DIR}"
fi

echo "arm64ec sysroot ready: ${EC_SYSROOT}"

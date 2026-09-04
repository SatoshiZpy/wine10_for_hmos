#!/bin/bash

# ============================================================
# 准备 arm64ec 运行时 sysroot (方案A, 不编译任何东西)
#
# 下载/解压 llvm-mingw 官方预编译工具链, 只取其中的
# arm64ec-w64-mingw32 目录作为 sysroot, 供 third_party/llvm
# 的 clang 编译 DXVK arm64ec 时使用:
#
#   clang --target=arm64ec-w64-mingw32 --sysroot=${EC_SYSROOT} ...
#
# sysroot 内容: mingw-w64 头文件, CRT, compiler-rt builtins,
# libc++/libunwind, 全套 mingw-w64 导入库 (含 DXVK 需要的
# d3d9/d3d11/dxgi/gdi32/version)
# ============================================================

set -ex

# --- 版本与下载源 (按需更新 tag, 优先选捆绑 LLVM >= 20 的版本) ---
# https://github.com/mstorsjo/llvm-mingw/releases
LLVM_MINGW_TAG="20251216"
LLVM_MINGW_ARCHIVE="${ROOT_DIR}/third_party/llvm-mingw-${LLVM_MINGW_TAG}-ucrt-ubuntu-22.04-x86_64.tar.xz"
LLVM_MINGW_URL="https://github.com/mstorsjo/llvm-mingw/releases/download/${LLVM_MINGW_TAG}/llvm-mingw-${LLVM_MINGW_TAG}-ucrt-ubuntu-22.04-x86_64.tar.xz"
# 校验和 (可选, 填 sha256 或留空跳过)
LLVM_MINGW_SHA256=""

LLVM_MINGW_DIR="${ROOT_DIR}/third_party/llvm-mingw"
LLVM_MINGW_EXTRACTED="${LLVM_MINGW_DIR}/llvm-mingw-${LLVM_MINGW_TAG}-ucrt-ubuntu-22.04-x86_64"

# 导出给 build_dxvk.sh 使用的变量
EC_SYSROOT="${LLVM_MINGW_EXTRACTED}/arm64ec-w64-mingw32"

# ============================================================
# 1. 获取/解压 (幂等, 同 build.sh 处理 LLVM-21.1.0-Linux-X64 的惯例)
#    解压目录已存在时直接放行 (压缩包可不在场)
# ============================================================
if [ ! -d "${LLVM_MINGW_EXTRACTED}" ]; then
    if [ ! -f "${LLVM_MINGW_ARCHIVE}" ]; then
        if [ "${ALLOW_DOWNLOAD}" = "1" ]; then
            echo "Downloading llvm-mingw (${LLVM_MINGW_TAG})..."
            curl -L --retry 3 -o "${LLVM_MINGW_ARCHIVE}" "${LLVM_MINGW_URL}"
        else
            echo "ERROR: ${LLVM_MINGW_ARCHIVE} not found."
            echo "手动下载: ${LLVM_MINGW_URL}"
            echo "或放到 ${LLVM_MINGW_DIR}/ 后重试"
            exit 1
        fi
    fi

    if [ -n "${LLVM_MINGW_SHA256}" ]; then
        echo "${LLVM_MINGW_SHA256}  ${LLVM_MINGW_ARCHIVE}" | sha256sum -c -
    fi

    mkdir -p ${LLVM_MINGW_DIR}
    tar -xJf "${LLVM_MINGW_ARCHIVE}" -C "${LLVM_MINGW_DIR}"
else
    echo "${LLVM_MINGW_EXTRACTED} exists, skip extracting."
fi

# ============================================================
# 3. 校验 sysroot 完整性
# ============================================================
if [ ! -d "${EC_SYSROOT}" ]; then
    echo "ERROR: arm64ec sysroot missing: ${EC_SYSROOT}"
    echo "该 release 可能未捆绑 arm64ec 目标, 换更新的 tag 重试:"
    echo "https://github.com/mstorsjo/llvm-mingw/releases"
    exit 1
fi

# 3.1 DXVK 链接需要的导入库
for lib in d3d9 d3d11 dxgi gdi32 version; do
    if ls ${EC_SYSROOT}/lib/lib${lib}.a >/dev/null 2>&1; then
        echo "OK: import lib ${lib}"
    else
        echo "MISSING: import lib ${lib} in ${EC_SYSROOT}/lib"
        exit 1
    fi
done

# 3.2 C++ 运行时 (libc++ 头文件 + 静态库)
[ -d "${EC_SYSROOT}/include/c++/v1" ] || { echo "ERROR: libc++ headers missing"; exit 1; }
ls ${EC_SYSROOT}/lib/libc++*.a >/dev/null 2>&1 || { echo "ERROR: libc++ static lib missing"; exit 1; }

# 3.3 compiler-rt builtins: arm64ec 复用 aarch64 变体, 位于 llvm-mingw
#     的 clang resource 目录 (通用 clang 发行包只有 linux-x86_64 的)。
#     通过 -resource-dir 指过去, -rtlib=compiler-rt 即可自动解析
EC_BUILTINS_LIB=$(find ${LLVM_MINGW_EXTRACTED}/lib/clang -path '*/lib/windows/libclang_rt.builtins-aarch64.a' 2>/dev/null | head -1)
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

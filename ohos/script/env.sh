#!/bin/bash
# ohos/script/env.sh — 鸿蒙 SDK + PE 交叉编译 LLVM 工具链的统一发现与校验
#

# 防重复 source（sourced 时 return，直接执行时 exit）
if [ -n "${_OHOS_ENV_LOADED:-}" ]; then
    return 0 2>/dev/null || exit 0
fi
_OHOS_ENV_LOADED=1

# 仓库根（env.sh 在 ohos/script/，向上两级）
_ENV_DIR="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" && pwd)"
OHOS_BUILD_ROOT="$(cd "$_ENV_DIR/../.." && pwd)"
export OHOS_BUILD_ROOT

# ── 日志/校验工具 ──
_ohos_log() { printf '\033[32m[env]\033[0m %s\n' "$*" >&2; }
_ohos_warn() { printf '\033[33m[env WARN]\033[0m %s\n' "$*" >&2; }
_ohos_err() { printf '\033[31m[env ERROR]\033[0m %s\n' "$*" >&2; }
_ohos_die() { _ohos_err "$*"; exit 1; }

# ============================================================
# 1. OHOS SDK 发现
#   仅环境变量 OHOS_SDK（须存在且含 native/llvm/bin）；不回退、不默认。
# ============================================================
_find_ohos_sdk() {
    # 仅认环境变量 OHOS_SDK（须存在且含 native/llvm/bin）；不回退、不默认。
    if [ -n "${OHOS_SDK:-}" ] && [ -d "$OHOS_SDK/native/llvm/bin" ]; then return 0; fi
    return 1
}

# ============================================================
# 2. PE 交叉编译 LLVM 发现
#   仅环境变量 LLVM_MINGW；须存在且含 bin/clang；不回退、不默认。
# ============================================================
_find_llvm_pe() {
    # 仅认环境变量 LLVM_MINGW；须存在且含 bin/clang；不回退、不默认。
    if [ -n "${LLVM_MINGW:-}" ] && [ -x "$LLVM_MINGW/bin/clang" ]; then return 0; fi
    return 1
}

# ============================================================
# 3. 组件校验
# ============================================================
_ohos_sdk_check() {
    local missing=""
    [ -x "$OHOS_SDK/native/llvm/bin/clang" ]    || missing="$missing native/llvm/bin/clang"
    [ -d "$OHOS_SDK/native/sysroot" ]           || missing="$missing native/sysroot"
    [ -f "$OHOS_SDK/native/build/cmake/ohos.toolchain.cmake" ] || missing="$missing native/build/cmake/ohos.toolchain.cmake"
    [ -x "$OHOS_SDK/native/build-tools/cmake/bin/cmake" ] || missing="$missing native/build-tools/cmake/bin/cmake"
    [ -x "$OHOS_SDK/native/build-tools/cmake/bin/ninja" ] || missing="$missing native/build-tools/cmake/bin/ninja"
    [ -z "$missing" ] || { _ohos_err "OHOS SDK($OHOS_SDK) 缺组件:$missing"; return 1; }
}
_ohos_llvm_check() {
    local missing=""
    [ -x "$LLVM_MINGW/bin/clang" ]   || missing="$missing bin/clang"
    [ -x "$LLVM_MINGW/bin/clang++" ] || missing="$missing bin/clang++"
    # lld-link（PE 链接器；可能是 ld.lld 的符号链接）
    { [ -e "$LLVM_MINGW/bin/lld-link" ] || [ -e "$LLVM_MINGW/bin/ld.lld" ]; } || missing="$missing bin/lld-link|ld.lld"
    [ -z "$missing" ] || { _ohos_err "LLVM($LLVM_MINGW) 缺组件:$missing"; return 1; }
}

# ============================================================
# 4. libxml2.so.2 兼容性检测（lld-link 运行期依赖）
#   官方 LLVM 的 lld-link NEEDED libxml2.so.2（v2 ABI）；
#   现代发行版（Ubuntu 26.04 等）只提供 libxml2.so.16。
#   不匹配时 lld-link 无法加载 → PE 交叉编译检测失败。
# ============================================================
_ohos_check_libxml2() {
    local lld="$LLVM_MINGW/bin/lld"
    [ -e "$lld" ] || lld="$LLVM_MINGW/bin/lld-link"
    [ -e "$lld" ] || return 0
    # ldd 是否 NEEDED libxml2
    ldd "$lld" 2>/dev/null | grep -q 'libxml2\.so' || return 0
    # 能否加载（清 LD_LIBRARY_PATH 模拟干净环境）
    if (env -u LD_LIBRARY_PATH "$lld" --version 2>&1 || true) | grep -qi 'libxml2.*cannot'; then
        _ohos_warn "lld-link 依赖 libxml2.so.2(v2 ABI), 本机仅有 libxml2.so.16, 无法加载。"
        _ohos_warn "修复: sudo ln -sf /usr/lib/x86_64-linux-gnu/libxml2.so.16 /usr/lib/x86_64-linux-gnu/libxml2.so.2 && sudo ldconfig"
        _ohos_warn "(v16 顶替 v2, 符号兼容可加载)"
        return 1
    fi
    return 0
}

# ============================================================
# 5. 执行发现 + 校验（失败给清晰指引）
# ============================================================
if ! _find_ohos_sdk; then
    _ohos_die "OHOS SDK 未就绪——构建中断。须用环境变量指定 SDK 路径。
  export OHOS_SDK=/path/to/sdk/default/openharmony
路径须存在且含: native/llvm/bin/clang、native/sysroot、native/build/cmake/ohos.toolchain.cmake、native/build-tools/cmake/bin/{cmake,ninja}
当前 OHOS_SDK=${OHOS_SDK:-<未设置>}（若已设置，请检查路径存在性与上述组件）"
fi
_ohos_sdk_check || _ohos_die "OHOS SDK 组件不完整（见上）；请重装或换完整 command-line-tools"

if ! _find_llvm_pe; then
    _ohos_die "PE LLVM 工具链未就绪——构建中断。须用环境变量指定 LLVM 路径。
  export LLVM_MINGW=/path/to/llvm-mingw-<tag>-ucrt-ubuntu-22.04-x86_64
（须含 bin/clang、bin/clang++、bin/lld-link、arm64ec-w64-mingw32/ sysroot; llvm-mingw 发行包）
当前 LLVM_MINGW=${LLVM_MINGW:-<未设置>}（若已设置，请检查路径存在性与上述组件）"
fi
_ohos_llvm_check || _ohos_die "LLVM 组件不完整（见上）"

# ============================================================
# 6. 派生路径与变量（单一来源，供 build.sh / ohos/script/*.sh 消费）
# ============================================================
OHOS_SYSROOT="$OHOS_SDK/native/sysroot"
OHOS_TOOLCHAIN="$OHOS_SDK/native/llvm"
CMAKE_PATH="$OHOS_SDK/native/build-tools/cmake/bin/cmake"
NINJA_PATH="$OHOS_SDK/native/build-tools/cmake/bin/ninja"
CMAKE_FILE_PATH="$OHOS_SDK/native/build/cmake/ohos.toolchain.cmake"
CLANG="$OHOS_TOOLCHAIN/bin/clang"
export OHOS_SDK OHOS_SYSROOT OHOS_TOOLCHAIN CMAKE_PATH NINJA_PATH CMAKE_FILE_PATH CLANG

# Wine Unix 侧编译器（aarch64-linux-ohos，OHOS SDK clang）
# 注意：CFLAGS/CXXFLAGS 不在此设（由 build.sh 按DEBUG/各deps脚本自定），仅定编译器。
export CC="$OHOS_TOOLCHAIN/bin/clang --target=aarch64-linux-ohos"
export CXX="$OHOS_TOOLCHAIN/bin/clang++ --target=aarch64-linux-ohos"

# PE 侧编译器（i386/x86_64-windows，llvm-mingw clang，带 MinGW CRT）
export ac_cv_prog_x86_64_CC="$LLVM_MINGW/bin/clang"
export ac_cv_prog_i386_CC="$LLVM_MINGW/bin/clang"
export ac_cv_prog_x86_64_CXX="$LLVM_MINGW/bin/clang++"
export ac_cv_prog_i386_CXX="$LLVM_MINGW/bin/clang++"

# PE 工具链（llvm-mingw bin，供 build_dxvk.sh 等复用，避免各脚本重复定义）
PE_CLANG="$LLVM_MINGW/bin/clang"
PE_CXX="$LLVM_MINGW/bin/clang++"
PE_LLDLINK="$LLVM_MINGW/bin/lld-link"
PE_AR="$LLVM_MINGW/bin/llvm-ar"
PE_STRIP="$LLVM_MINGW/bin/llvm-strip"
PE_READOBJ="$LLVM_MINGW/bin/llvm-readobj"

# arm64ec sysroot + compiler-rt builtins（DXVK arm64ec 用；arm64ec-w64-mingw32 不存在则 arm64ec 不可用）
EC_SYSROOT="$LLVM_MINGW/arm64ec-w64-mingw32"
EC_BUILTINS_LIB="$(find "$LLVM_MINGW/lib/clang" -path '*/lib/windows/libclang_rt.builtins-aarch64.a' 2>/dev/null | head -1)"
[ -n "$EC_BUILTINS_LIB" ] && EC_RESOURCE_DIR="$(dirname "$(dirname "$(dirname "$EC_BUILTINS_LIB")")")"
export PE_CLANG PE_CXX PE_LLDLINK PE_AR PE_STRIP PE_READOBJ EC_SYSROOT EC_BUILTINS_LIB EC_RESOURCE_DIR

# DXVK / vkd3d-proton 路径（供 build 脚本复用，避免重复定义）
DXVK_DIR="${OHOS_BUILD_ROOT}/third_party/dxvk"
DXVK_BUILD_DIR="${OHOS_BUILD_ROOT}/build/dxvk"
DXVK_LEGACY_DIR="${OHOS_BUILD_ROOT}/third_party/dxvk-legacy"
DXVK_LEGACY_BUILD_DIR="${OHOS_BUILD_ROOT}/build/dxvk-legacy"
VKD3D_PROTON_DIR="${OHOS_BUILD_ROOT}/third_party/vkd3d-proton"
VKD3D_BUILD_DIR="${OHOS_BUILD_ROOT}/build/vkd3d-proton"
export DXVK_DIR DXVK_BUILD_DIR DXVK_LEGACY_DIR DXVK_LEGACY_BUILD_DIR VKD3D_PROTON_DIR VKD3D_BUILD_DIR

# arm64ec sysroot 校验（仅 --with-dxvk + --build-arm64ec 时）
if [ "${COMPILE_DXVK:-0}" = "1" ] && [ "${BUILD_ARM64EC:-0}" = "1" ]; then
    if [ -z "${EC_SYSROOT:-}" ] || [ ! -d "${EC_SYSROOT}" ]; then
        _ohos_die "arm64ec sysroot 未就绪: ${EC_SYSROOT:-<未设置>}
  LLVM_MINGW 须含 arm64ec-w64-mingw32/ (sysroot)、lib/clang (compiler-rt)
  export LLVM_MINGW=/path/to/llvm-mingw-<tag>-ucrt-ubuntu-22.04-x86_64
  下载: https://github.com/mstorsjo/llvm-mingw/releases"
    fi
    for lib in d3d9 d3d11 dxgi gdi32 version; do
        ls "${EC_SYSROOT}/lib/lib${lib}.a" >/dev/null 2>&1 || _ohos_die "arm64ec 导入库 ${lib} missing in ${EC_SYSROOT}/lib"
    done
    [ -d "${EC_SYSROOT}/include/c++/v1" ] || _ohos_die "arm64ec libc++ headers missing"
    ls "${EC_SYSROOT}/lib/libc++"*.a >/dev/null 2>&1 || _ohos_die "arm64ec libc++ static lib missing"
    if [ -n "${EC_BUILTINS_LIB}" ]; then _ohos_log "OK: builtins ${EC_BUILTINS_LIB}"; else _ohos_warn "compiler-rt builtins (aarch64) not found, 链接期若出现 undefined __multi3/__ashlti3 需补齐"; fi
    _ohos_log "arm64ec sysroot ready: ${EC_SYSROOT}"
fi

# PATH：OHOS SDK command-line-tools bin（hvigorw/ohpm）+ node + LLVM bin
# TOOL_HOME（command-line-tools 根，含 bin/hvigorw、tool/node、sdk/）从 OHOS_SDK 推导
#   （OHOS_SDK = $TOOL_HOME/sdk/default/openharmony → TOOL_HOME = OHOS_SDK/../.. 的上两级）
TOOL_HOME="${TOOL_HOME:-$(cd "$OHOS_SDK/../../.." 2>/dev/null && pwd || true)}"
export TOOL_HOME
export PATH="$TOOL_HOME/bin:$TOOL_HOME/tool/node/bin:$LLVM_MINGW/bin:$PATH"

# HAP 打包用（hvigor 读 DEVECO_SDK_HOME/DEVECO_NODE_HOME）
export DEVECO_SDK_HOME="$TOOL_HOME/sdk"
export DEVECO_NODE_HOME="$TOOL_HOME/tool/node"

# libxml2 兼容性（仅警告，不阻断——用户可能已用系统符号链接处理）
_ohos_check_libxml2 || true

_ohos_log "OHOS_SDK        = $OHOS_SDK"
_ohos_log "LLVM_MINGW = $LLVM_MINGW (PE, llvm-mingw)"
_ohos_log "Unix target    = aarch64-linux-ohos (OHOS SDK clang)"
_ohos_log "PE archs       = aarch64/i386/x86_64-windows (llvm-mingw clang)"
_ohos_log "TOOL_HOME      = $TOOL_HOME (hvigorw/node: $DEVECO_SDK_HOME / $DEVECO_NODE_HOME)"

#!/bin/bash

# Build DXVK 1.10.3 (legacy) for OHOS (cross-compile to Windows PE DLLs)
#
# 64-bit: ARM64X (0xA64E, hybrid ARM64 + arm64ec), 机制同 build_dxvk.sh
#   (编两套 obj, lld-link -machine:arm64x 合并)。
# 32-bit: i386, llvm-mingw 自带 i686 工具链 (wow64 下 32 位游戏由鸿蒙
#   内核 ubt 翻译器执行)。
#
# DXVK 1.10.3 与 2.6.2 的差异 (影响构建): meson.build 对非 x86_64 架构
# 固定从源码树 lib32 找 vulkan-1/d3dcompiler_43, 但 lib32 是 x86 库且与
# i386 构建共用, 故为 arm64ec/aarch64 单独生成 lib-arm64ec/ 导入库:
#   - d3dcompiler_43: 用 dxvk-legacy 自带的 def (def 与架构无关)
#   - vulkan-1: dxvk 只引用 vkGetInstanceProcAddr 一个导入符号
#     (src/vulkan/vulkan_loader.cpp), 自写最小 def
# meson.build 的 dxvk_library_path 已加 aarch64 分支指向 lib-arm64ec。
#
# 公共步骤 (glslang 发现 / cross-file 生成 / ARM64X 合并等) 见 pe_common.sh。
# DXVK_LEGACY_DIR / DXVK_LEGACY_BUILD_DIR 由 env.sh 派生。

set -ex
source "$(dirname "${BASH_SOURCE[0]}")/pe_common.sh"

# DXVK 复用 wine 内置 BC 解码头：源码内写
# #include "dlls/d3dx9_36/bcdec.h"（标明来源），wine 源码根
# (OHOS_BUILD_ROOT) 经 crossfile 以 -I 注入，消除相对路径硬编码耦合。


pe_find_glslang

mkdir -p ${DXVK_LEGACY_BUILD_DIR}

# ============================================================
# 1. Build 64-bit DXVK legacy (arm64ec + aarch64 -> ARM64X hybrid)
# ============================================================
if [ "${BUILD_ARM64EC}" -eq 1 ]; then
    pe_require_ec_sysroot
    AARCH64_SYSROOT="${LLVM_MINGW}/aarch64-w64-mingw32"
    if [ ! -d "${AARCH64_SYSROOT}" ]; then
        echo "ERROR: aarch64 sysroot missing: ${AARCH64_SYSROOT}"
        exit 1
    fi

    # ------------------------------------------------------------
    # 1.1 生成 arm64ec/aarch64 导入库 (vulkan-1, d3dcompiler_43)
    # meson 按当前构建架构从 lib-arm64ec/ 读无版本号的 .lib,
    # 故每种库编 EC 与 A64 两个版本, 构建前 cp 成无版本号名。
    # ------------------------------------------------------------
    EC_IMPLIB_DIR="${DXVK_LEGACY_DIR}/lib-arm64ec"
    mkdir -p ${EC_IMPLIB_DIR}

    cat > ${DXVK_LEGACY_BUILD_DIR}/vulkan-1.def << 'EOF'
LIBRARY vulkan-1.dll
EXPORTS
    vkGetInstanceProcAddr
EOF

    if [ ! -f "${EC_IMPLIB_DIR}/vulkan-1-ec.lib" ]; then
        ${PE_LLDLINK} /MACHINE:ARM64EC /NOLOGO /DEF:${DXVK_LEGACY_BUILD_DIR}/vulkan-1.def \
            /OUT:${EC_IMPLIB_DIR}/vulkan-1-ec.lib
    fi
    if [ ! -f "${EC_IMPLIB_DIR}/d3dcompiler_43-ec.lib" ]; then
        ${PE_LLDLINK} /MACHINE:ARM64EC /NOLOGO /DEF:${DXVK_LEGACY_DIR}/lib/libd3dcompiler_43.def \
            /OUT:${EC_IMPLIB_DIR}/d3dcompiler_43-ec.lib
    fi
    if [ ! -f "${EC_IMPLIB_DIR}/vulkan-1-a64.lib" ]; then
        ${PE_LLDLINK} /MACHINE:ARM64 /NOLOGO /DEF:${DXVK_LEGACY_BUILD_DIR}/vulkan-1.def \
            /OUT:${EC_IMPLIB_DIR}/vulkan-1-a64.lib
    fi
    if [ ! -f "${EC_IMPLIB_DIR}/d3dcompiler_43-a64.lib" ]; then
        ${PE_LLDLINK} /MACHINE:ARM64 /NOLOGO /DEF:${DXVK_LEGACY_DIR}/lib/libd3dcompiler_43.def \
            /OUT:${EC_IMPLIB_DIR}/d3dcompiler_43-a64.lib
    fi

    pe_make_windres_shim "${DXVK_LEGACY_BUILD_DIR}/windres-ec" arm64ec-w64-mingw32 "${EC_SYSROOT}"
    pe_make_windres_shim "${DXVK_LEGACY_BUILD_DIR}/windres-a64" aarch64-w64-mingw32 \
        "${AARCH64_SYSROOT}"
    pe_make_lld_wrapper "${DXVK_LEGACY_BUILD_DIR}/ld.lld"
    pe_make_lld_wrapper "${DXVK_LEGACY_BUILD_DIR}/ld.lld-a64"

    pe_write_mingw_crossfile "${DXVK_LEGACY_BUILD_DIR}/build-arm64ec.txt" arm64ec "${EC_SYSROOT}" \
        "${DXVK_LEGACY_BUILD_DIR}/windres-ec" "${DXVK_LEGACY_BUILD_DIR}/ld.lld" \
        "-D__OHOS__=1 -I${OHOS_BUILD_ROOT}" "" ""
    pe_write_mingw_crossfile "${DXVK_LEGACY_BUILD_DIR}/build-aarch64.txt" aarch64 \
        "${AARCH64_SYSROOT}" \
        "${DXVK_LEGACY_BUILD_DIR}/windres-a64" "${DXVK_LEGACY_BUILD_DIR}/ld.lld-a64" \
        "-D__OHOS__=1 -I${OHOS_BUILD_ROOT}" "" ""

    echo "--- Building DXVK legacy arm64ec ---"
    # arm64ec 构建用 EC 版导入库
    cp -f ${EC_IMPLIB_DIR}/vulkan-1-ec.lib ${EC_IMPLIB_DIR}/vulkan-1.lib
    cp -f ${EC_IMPLIB_DIR}/d3dcompiler_43-ec.lib ${EC_IMPLIB_DIR}/d3dcompiler_43.lib
    pe_meson_build "${DXVK_LEGACY_BUILD_DIR}/buildec" "${DXVK_LEGACY_DIR}" \
        "${DXVK_LEGACY_BUILD_DIR}/build-arm64ec.txt" "${DXVK_LEGACY_BUILD_DIR}/installec" \
        "${DXVK_LEGACY_BUILD_DIR}:${GLSLANG_DIR}" 1

    # 中途校验: machine type 必须是 ARM64EC (0xA641), 防止 target 配错
    # 时静默编成纯 aarch64 (wow64 下无法加载)
    echo "--- Verifying DXVK legacy arm64ec artifacts ---"
    for dll in d3d11.dll dxgi.dll; do
        f="${DXVK_LEGACY_BUILD_DIR}/installec/bin/${dll}"
        [ -f "${f}" ] || f="${DXVK_LEGACY_BUILD_DIR}/installec/lib/${dll}"
        pe_verify_machine "${f}" "ARM64EC" || exit 1
    done

    echo "--- Building DXVK legacy aarch64 (native ARM64 obj for ARM64X) ---"
    # aarch64 构建换用 ARM64 版导入库
    cp -f ${EC_IMPLIB_DIR}/vulkan-1-a64.lib ${EC_IMPLIB_DIR}/vulkan-1.lib
    cp -f ${EC_IMPLIB_DIR}/d3dcompiler_43-a64.lib ${EC_IMPLIB_DIR}/d3dcompiler_43.lib
    pe_meson_build "${DXVK_LEGACY_BUILD_DIR}/buildarm64" "${DXVK_LEGACY_DIR}" \
        "${DXVK_LEGACY_BUILD_DIR}/build-aarch64.txt" "${DXVK_LEGACY_BUILD_DIR}/installarm64" \
        "${DXVK_LEGACY_BUILD_DIR}:${GLSLANG_DIR}" 1

    pe_make_ctors_stubs "${DXVK_LEGACY_BUILD_DIR}"

    # 附加导入库: 1.10.3 用全路径链接 vulkan-1/d3dcompiler_43 (不走 -l),
    # 合并函数抓不到, 需每个 DLL 显式带上 (EC 与 A64 各一份)。
    LEGACY_IMPLIBS="${EC_IMPLIB_DIR}/vulkan-1-ec.lib ${EC_IMPLIB_DIR}/vulkan-1-a64.lib"
    LEGACY_IMPLIBS="${LEGACY_IMPLIBS} ${EC_IMPLIB_DIR}/d3dcompiler_43-ec.lib"
    LEGACY_IMPLIBS="${LEGACY_IMPLIBS} ${EC_IMPLIB_DIR}/d3dcompiler_43-a64.lib"

    # (ninja target, def 相对 DXVK_LEGACY_DIR, 输出 dll)
    echo "--- Merging DXVK legacy arm64ec + aarch64 -> ARM64X (0xA64E) ---"
    mkdir -p ${DXVK_LEGACY_BUILD_DIR}/installec/bin
    pe_merge_arm64x_dll ${DXVK_LEGACY_BUILD_DIR} src/d3d11/d3d11.dll \
        ${DXVK_LEGACY_DIR}/src/d3d11/d3d11.def \
        ${DXVK_LEGACY_BUILD_DIR}/installec/bin/d3d11.dll ${LEGACY_IMPLIBS}
    pe_merge_arm64x_dll ${DXVK_LEGACY_BUILD_DIR} src/dxgi/dxgi.dll \
        ${DXVK_LEGACY_DIR}/src/dxgi/dxgi.def \
        ${DXVK_LEGACY_BUILD_DIR}/installec/bin/dxgi.dll ${LEGACY_IMPLIBS}
    pe_merge_arm64x_dll ${DXVK_LEGACY_BUILD_DIR} src/d3d9/d3d9.dll \
        ${DXVK_LEGACY_DIR}/src/d3d9/d3d9.def \
        ${DXVK_LEGACY_BUILD_DIR}/installec/bin/d3d9.dll ${LEGACY_IMPLIBS}
    pe_merge_arm64x_dll ${DXVK_LEGACY_BUILD_DIR} src/d3d10/d3d10core.dll \
        ${DXVK_LEGACY_DIR}/src/d3d10/d3d10core.def \
        ${DXVK_LEGACY_BUILD_DIR}/installec/bin/d3d10core.dll ${LEGACY_IMPLIBS}

    # 重写 .ctors 为只含原生 flavor 构造函数的有效列表 (同 build_dxvk.sh,
    # 见 patch_arm64x_ctors.py 头注释)。
    python3 ${SCRIPT_DIR}/patch_arm64x_ctors.py \
        ${DXVK_LEGACY_BUILD_DIR}/installec/bin/d3d11.dll \
        ${DXVK_LEGACY_BUILD_DIR}/installec/bin/dxgi.dll \
        ${DXVK_LEGACY_BUILD_DIR}/installec/bin/d3d9.dll \
        ${DXVK_LEGACY_BUILD_DIR}/installec/bin/d3d10core.dll || exit 1

    for dll in d3d11.dll dxgi.dll d3d9.dll d3d10core.dll; do
        f="${DXVK_LEGACY_BUILD_DIR}/installec/bin/${dll}"
        [ -f "${f}" ] && ${PE_STRIP} -x "${f}" 2>/dev/null || true
    done

    echo "--- Verifying DXVK legacy ARM64X artifacts ---"
    for dll in d3d11.dll dxgi.dll; do
        pe_verify_machine "${DXVK_LEGACY_BUILD_DIR}/installec/bin/${dll}" "ARM64X" || exit 1
    done
else
    echo "SKIP: DXVK legacy arm64ec build requires --build-arm64ec"
fi

# ============================================================
# 2. Build 32-bit DXVK legacy (i386, llvm-mingw i686 工具链)
# ============================================================
pe_require_i386_sysroot

pe_write_i386_crossfile "${DXVK_LEGACY_BUILD_DIR}/build-ohos32.txt" "-D__OHOS__=1 -I${OHOS_BUILD_ROOT}" "" ""

echo "--- Building DXVK legacy x86 ---"
pe_meson_build "${DXVK_LEGACY_BUILD_DIR}/build32" "${DXVK_LEGACY_DIR}" \
    "${DXVK_LEGACY_BUILD_DIR}/build-ohos32.txt" "${DXVK_LEGACY_BUILD_DIR}/install32" \
    "${DXVK_LEGACY_BUILD_DIR}:${GLSLANG_DIR}" 0

echo "--- Verifying DXVK legacy x86 artifacts ---"
for dll in d3d11.dll dxgi.dll; do
    if [ ! -f "${DXVK_LEGACY_BUILD_DIR}/install32/bin/${dll}" ]; then
        echo "MISSING: ${DXVK_LEGACY_BUILD_DIR}/install32/bin/${dll}"
        exit 1
    fi
    echo "OK: ${DXVK_LEGACY_BUILD_DIR}/install32/bin/${dll}"
done

echo "DXVK legacy build completed successfully!"

#!/bin/bash

# Build DXVK 2.6.2 for OHOS (cross-compile to Windows PE DLLs)
#
# 64-bit: ARM64X (0xA64E, hybrid ARM64 + arm64ec)。ARM64X 让 ARM64 进程
#   (如 wine_d3d11_test.exe, 0xAA64) 能加载 d3d11.dll/dxgi.dll —
#   ntdll/loader.c is_valid_binary 对 ARM64X 的 info->Machine=ARM64 放行,
#   纯 ARM64EC (0xA641) 在 ARM64 进程被拒。机制: 编两套 obj (arm64ec +
#   aarch64), lld-link -machine:arm64x 合并; libmingwex 提供
#   __chpe_metadata + dispatch + _load_config_used (参考 Wine builtin,
#   winebuild -marm64x)。工具链: 环境变量 LLVM_MINGW。
# 32-bit: i386, 系统 mingw-w64 工具链 (wow64 下 32 位游戏走 xtajit)。
#
# DXVK 2.6.2 与 1.10.3 的差异 (影响构建): meson.build 用
# check_header('vulkan/vulkan.h') 而非 find_library('vulkan-1'), 且不再
# 引用 d3dcompiler_43 — 无需预生成任何 arm64ec 导入库 (对比
# build_dxvk_legacy.sh 的 lib-arm64ec 一节)。
#
# 公共步骤 (glslang 发现 / cross-file 生成 / ARM64X 合并等) 见 pe_common.sh。
# DXVK_DIR / DXVK_BUILD_DIR 由 env.sh 派生。

set -ex
source "$(dirname "${BASH_SOURCE[0]}")/pe_common.sh"

pe_find_glslang

mkdir -p ${DXVK_BUILD_DIR}

# ============================================================
# 1. Build 64-bit DXVK (arm64ec + aarch64 -> ARM64X hybrid)
# ============================================================
if [ "${BUILD_ARM64EC}" -eq 1 ]; then
    pe_require_ec_sysroot
    AARCH64_SYSROOT="${LLVM_MINGW}/aarch64-w64-mingw32"
    if [ ! -d "${AARCH64_SYSROOT}" ]; then
        echo "ERROR: aarch64 sysroot missing: ${AARCH64_SYSROOT}"
        exit 1
    fi

    pe_make_windres_shim "${DXVK_BUILD_DIR}/windres-ec" arm64ec-w64-mingw32 "${EC_SYSROOT}"
    pe_make_windres_shim "${DXVK_BUILD_DIR}/windres-a64" aarch64-w64-mingw32 "${AARCH64_SYSROOT}"
    pe_make_lld_wrapper "${DXVK_BUILD_DIR}/ld.lld"
    pe_make_lld_wrapper "${DXVK_BUILD_DIR}/ld.lld-a64"

    pe_write_mingw_crossfile "${DXVK_BUILD_DIR}/build-arm64ec.txt" arm64ec "${EC_SYSROOT}" \
        "${DXVK_BUILD_DIR}/windres-ec" "${DXVK_BUILD_DIR}/ld.lld" \
        "-D__OHOS__=1" "" "-include algorithm"
    pe_write_mingw_crossfile "${DXVK_BUILD_DIR}/build-aarch64.txt" aarch64 "${AARCH64_SYSROOT}" \
        "${DXVK_BUILD_DIR}/windres-a64" "${DXVK_BUILD_DIR}/ld.lld-a64" \
        "-D__OHOS__=1" "" "-include algorithm"

    echo "--- Building DXVK arm64ec ---"
    pe_meson_build "${DXVK_BUILD_DIR}/buildec" "${DXVK_DIR}" \
        "${DXVK_BUILD_DIR}/build-arm64ec.txt" "${DXVK_BUILD_DIR}/installec" \
        "${DXVK_BUILD_DIR}:${GLSLANG_DIR}" 1

    echo "--- Building DXVK aarch64 (native ARM64 obj for ARM64X) ---"
    pe_meson_build "${DXVK_BUILD_DIR}/buildarm64" "${DXVK_DIR}" \
        "${DXVK_BUILD_DIR}/build-aarch64.txt" "${DXVK_BUILD_DIR}/installarm64" \
        "${DXVK_BUILD_DIR}:${GLSLANG_DIR}" 1

    pe_make_ctors_stubs "${DXVK_BUILD_DIR}"

    # (ninja target, def 相对 DXVK_DIR, 输出 dll)
    echo "--- Merging DXVK arm64ec + aarch64 -> ARM64X (0xA64E) ---"
    mkdir -p ${DXVK_BUILD_DIR}/installec/bin
    pe_merge_arm64x_dll ${DXVK_BUILD_DIR} src/d3d11/d3d11.dll \
        ${DXVK_DIR}/src/d3d11/d3d11.def ${DXVK_BUILD_DIR}/installec/bin/d3d11.dll
    pe_merge_arm64x_dll ${DXVK_BUILD_DIR} src/dxgi/dxgi.dll \
        ${DXVK_DIR}/src/dxgi/dxgi.def ${DXVK_BUILD_DIR}/installec/bin/dxgi.dll
    pe_merge_arm64x_dll ${DXVK_BUILD_DIR} src/d3d9/d3d9.dll \
        ${DXVK_DIR}/src/d3d9/d3d9.def ${DXVK_BUILD_DIR}/installec/bin/d3d9.dll
    pe_merge_arm64x_dll ${DXVK_BUILD_DIR} src/d3d10/d3d10core.dll \
        ${DXVK_DIR}/src/d3d10/d3d10core.def ${DXVK_BUILD_DIR}/installec/bin/d3d10core.dll
    pe_merge_arm64x_dll ${DXVK_BUILD_DIR} src/d3d8/d3d8.dll \
        ${DXVK_DIR}/src/d3d8/d3d8.def ${DXVK_BUILD_DIR}/installec/bin/d3d8.dll

    # 修复合并产物: 空 __CTOR_LIST__ stub 导致两套 CRT 的真实构造函数
    # (libc++ iostream/locale/guard 状态等) 从不执行 → 原生 (ARM64) 进程
    # 内首次使用 C++ iostream 即 SIGSEGV。重写 .ctors 为仅含原生 flavor
    # 构造函数的有效列表 (EC 构造函数依赖未初始化的 EC dispatch, 原生
    # 进程内调用即 x16=NULL SIGSEGV, 故按 RVA 聚类剔除)。
    # 详见 patch_arm64x_ctors.py 头注释。
    python3 ${SCRIPT_DIR}/patch_arm64x_ctors.py \
        ${DXVK_BUILD_DIR}/installec/bin/d3d11.dll \
        ${DXVK_BUILD_DIR}/installec/bin/dxgi.dll \
        ${DXVK_BUILD_DIR}/installec/bin/d3d9.dll \
        ${DXVK_BUILD_DIR}/installec/bin/d3d10core.dll \
        ${DXVK_BUILD_DIR}/installec/bin/d3d8.dll || exit 1

    for dll in d3d11.dll dxgi.dll d3d9.dll d3d10core.dll d3d8.dll; do
        f="${DXVK_BUILD_DIR}/installec/bin/${dll}"
        [ -f "${f}" ] && ${PE_STRIP} -x "${f}" 2>/dev/null || true
    done

    echo "--- Verifying DXVK ARM64X artifacts ---"
    for dll in d3d11.dll dxgi.dll; do
        pe_verify_machine "${DXVK_BUILD_DIR}/installec/bin/${dll}" \
            "IMAGE_FILE_MACHINE_ARM64X (0xA64E)" || exit 1
    done
else
    echo "SKIP: DXVK ARM64X build requires --build-arm64ec"
fi

# ============================================================
# 2. Build 32-bit DXVK (i386, 系统 mingw-w64 工具链)
# ============================================================
pe_require_mingw_w64

pe_write_i386_crossfile "${DXVK_BUILD_DIR}/build-ohos32.txt" "-D__OHOS__=1"

echo "--- Building DXVK x86 ---"
pe_meson_build "${DXVK_BUILD_DIR}/build32" "${DXVK_DIR}" \
    "${DXVK_BUILD_DIR}/build-ohos32.txt" "${DXVK_BUILD_DIR}/install32" \
    "${GLSLANG_DIR}" 0

echo "--- Verifying DXVK x86 artifacts ---"
for dll in d3d11.dll dxgi.dll; do
    if [ ! -f "${DXVK_BUILD_DIR}/install32/bin/${dll}" ]; then
        echo "MISSING: ${DXVK_BUILD_DIR}/install32/bin/${dll}"
        exit 1
    fi
    echo "OK: ${DXVK_BUILD_DIR}/install32/bin/${dll}"
done

echo "DXVK build completed successfully!"

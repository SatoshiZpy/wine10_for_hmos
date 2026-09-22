#!/bin/bash

# Build vkd3d-proton 3.1.0 for OHOS (cross-compile to Windows PE DLLs)
#
# 64-bit: 纯 arm64ec (wow64 mode), 不做 ARM64X 合并 — d3d12.dll 仅由
#   64 位 EC 进程加载, 无 ARM64 原生进程直载需求 (对比 build_dxvk.sh)。
# 32-bit: i386, llvm-mingw 自带 i686 工具链 (wow64 下 32 位游戏由鸿蒙
#   内核 ubt 翻译器执行)。
#
# vkd3d-proton meson.build 要点:
#   - khronos/Vulkan-Headers + SPIRV-Headers (源码树内置 include)
#   - subprojects/dxil-spirv (meson subproject, 跟随 cross-file 编译)
#   - find_program widl (cross-file [binaries] 显式指定, PATH 命令级注入)
#   - find_program glslang/glslangValidator (编译着色器, 硬依赖)
#   - windows 平台 find_library dxgi/gdi32/d3d12 (sysroot 内)
#
# OHOS 适配补丁 (500K profile / query-no-int64 / device10 等) 位于
# ohos/script/patches/vkd3d-proton/, 构建前应用到隔离副本。
# 公共步骤 (glslang 发现 / cross-file 生成等) 见 pe_common.sh。
# VKD3D_PROTON_DIR / VKD3D_BUILD_DIR 由 env.sh 派生。

set -ex
source "$(dirname "${BASH_SOURCE[0]}")/pe_common.sh"

pe_find_glslang

# wine 自带 widl (build-tools 阶段已编好), 供 vkd3d-proton 的 IDL 编译
WIDL_DIR="${OHOS_BUILD_ROOT}/build-tools/tools/widl"

mkdir -p ${VKD3D_BUILD_DIR}

# ============================================================
# 0. Apply OHOS patches (500K profile, query-no-int64, etc.)
# ============================================================
PATCH_DIR="${OHOS_BUILD_ROOT}/ohos/script/patches/vkd3d-proton"
PATCHED_SRC="${VKD3D_BUILD_DIR}/src"
STAMP="${PATCHED_SRC}/.ohos-patched"

if [ ! -f "${STAMP}" ]; then
    echo "Applying OHOS patches to vkd3d-proton..."
    rm -rf "${PATCHED_SRC}"
    mkdir -p "${PATCHED_SRC}"
    # 拷贝源码到隔离目录 (排除 .git), 避免污染 third_party 原树
    cp -a "${VKD3D_PROTON_DIR}/." "${PATCHED_SRC}/"
    rm -rf "${PATCHED_SRC}/.git"
    for patch in "${PATCH_DIR}"/*.patch; do
        echo "  applying $(basename "${patch}")"
        (cd "${PATCHED_SRC}" && patch -p1 --forward --batch < "${patch}") || {
            echo "WARNING: patch $(basename "${patch}") failed, continuing"
        }
    done
    touch "${STAMP}"
fi

VKD3D_PROTON_SRC="${PATCHED_SRC}"

# ============================================================
# 1. Build 64-bit vkd3d-proton (arm64ec)
# ============================================================
if [ "${BUILD_ARM64EC}" -eq 1 ]; then
    pe_require_ec_sysroot

    pe_make_windres_shim "${VKD3D_BUILD_DIR}/windres-ec" arm64ec-w64-mingw32 "${EC_SYSROOT}"
    pe_make_lld_wrapper "${VKD3D_BUILD_DIR}/ld.lld"

    pe_write_mingw_crossfile "${VKD3D_BUILD_DIR}/build-arm64ec.txt" arm64ec "${EC_SYSROOT}" \
        "${VKD3D_BUILD_DIR}/windres-ec" "${VKD3D_BUILD_DIR}/ld.lld" \
        "" "-Wno-incompatible-pointer-types" "-include algorithm" \
        "${WIDL_DIR}/widl"

    echo "--- Building vkd3d-proton arm64ec ---"
    pe_meson_build "${VKD3D_BUILD_DIR}/buildec" "${VKD3D_PROTON_SRC}" \
        "${VKD3D_BUILD_DIR}/build-arm64ec.txt" "${VKD3D_BUILD_DIR}/installec" \
        "${VKD3D_BUILD_DIR}:${GLSLANG_DIR}:${WIDL_DIR}" 1

    echo "--- Verifying vkd3d-proton arm64ec artifacts ---"
    for dll in d3d12.dll; do
        f="${VKD3D_BUILD_DIR}/installec/bin/${dll}"
        [ -f "${f}" ] || f="${VKD3D_BUILD_DIR}/installec/lib/${dll}"
        pe_verify_machine "${f}" "ARM64EC" || exit 1
    done
else
    echo "SKIP: vkd3d-proton arm64ec build requires --build-arm64ec"
fi

# ============================================================
# 2. Build 32-bit vkd3d-proton (i386, llvm-mingw i686 工具链)
# ============================================================
pe_require_i386_sysroot

pe_write_i386_crossfile "${VKD3D_BUILD_DIR}/build-ohos32.txt" "" "" ""

echo "--- Building vkd3d-proton x86 ---"
pe_meson_build "${VKD3D_BUILD_DIR}/build32" "${VKD3D_PROTON_SRC}" \
    "${VKD3D_BUILD_DIR}/build-ohos32.txt" "${VKD3D_BUILD_DIR}/install32" \
    "${VKD3D_BUILD_DIR}:${GLSLANG_DIR}:${WIDL_DIR}" 0

echo "--- Verifying vkd3d-proton x86 artifacts ---"
for dll in d3d12.dll; do
    if [ ! -f "${VKD3D_BUILD_DIR}/install32/bin/${dll}" ]; then
        echo "MISSING: ${VKD3D_BUILD_DIR}/install32/bin/${dll}"
        exit 1
    fi
    echo "OK: ${VKD3D_BUILD_DIR}/install32/bin/${dll}"
done

echo "vkd3d-proton build completed successfully!"

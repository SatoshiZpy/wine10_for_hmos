#!/bin/bash

# Build vkd3d-proton 3.1.0 for OHOS (cross-compile to Windows PE DLLs)
#
# 64-bit: arm64ec (wow64 mode), 用环境变量 LLVM_MINGW 指定的
#         clang + llvm-mingw 的 arm64ec sysroot (见 env.sh)
# 32-bit: i386, 用系统 mingw-w64 工具链 (wow64 下 32 位游戏走 xtajit)
#
# vkd3d-proton meson.build 要点:
#   - khronos/Vulkan-Headers + SPIRV-Headers (源码树内置 include)
#   - subprojects/dxil-spirv (meson subproject, 跟随 cross-file 编译)
#   - find_program widl (required:false; 缺则 meson 用 fallback, 不阻断)
#     —— 本脚本不强制 widl: 若 meson 报缺再在 cross-file [binaries]
#        加 widl, 保持简单。
#   - find_program glslang/glslangValidator (编译着色器, 硬依赖)
#   - windows 平台 find_library dxgi/gdi32/d3d12 (sysroot 内)
#   - linux/android 才 find_library dl/atomic/m/xcb (本脚本只编 windows)

set -ex
# VKD3D_PROTON_DIR / VKD3D_BUILD_DIR 由 env.sh 派生
#   (${OHOS_BUILD_ROOT}/third_party/vkd3d-proton, ${OHOS_BUILD_ROOT}/build/vkd3d-proton)

# vkd3d-proton 的 meson.build 硬依赖 glslangValidator 编译着色器
# 查找顺序: PATH > Vulkan SDK (VULKAN_SDK 或默认 ~/1.3.290.0)
GLSLANG_BIN="$(command -v glslangValidator || true)"
if [ -z "${GLSLANG_BIN}" ] && [ -n "${VULKAN_SDK}" ] && [ -x "${VULKAN_SDK}/bin/glslangValidator" ]; then
    GLSLANG_BIN="${VULKAN_SDK}/bin/glslangValidator"
fi
if [ -z "${GLSLANG_BIN}" ] && [ -x "${HOME}/1.3.290.0/x86_64/bin/glslangValidator" ]; then
    GLSLANG_BIN="${HOME}/1.3.290.0/x86_64/bin/glslangValidator"
fi
if [ -z "${GLSLANG_BIN}" ]; then
    echo "ERROR: glslangValidator not found."
    echo "  方式1: sudo apt install glslang-tools"
    echo "  方式2: export VULKAN_SDK=<SDK路径>/x86_64 (source <SDK>/setup-env.sh)"
    exit 1
fi
echo "Using glslangValidator: ${GLSLANG_BIN}"
# 注意: 本脚本被 build.sh source, 绝不能 export 修改 PATH ——
# 曾因把 glslang 所在目录 (系统里是 /usr/bin) 前置到 PATH,
# 导致后续 wine 构建阶段 winebuild 按 PATH 顺序命中系统的
# /usr/bin/llvm-dlltool (LLVM 18, 无 arm64ec) 而失败。
# 改为命令级注入: 只对下面的 meson/ninja 调用生效, 零泄漏。
GLSLANG_DIR="$(dirname "${GLSLANG_BIN}")"

# Create build directories
mkdir -p ${VKD3D_BUILD_DIR}

# ============================================================
# 1. Build 64-bit vkd3d-proton (arm64ec)
# ============================================================
if [ "${BUILD_ARM64EC}" -eq 1 ]; then
    # 依赖 env.sh 导出的: EC_SYSROOT, EC_BUILTINS_LIB
    if [ -z "${EC_SYSROOT}" ]; then
        echo "ERROR: EC_SYSROOT not set, source ohos/script/env.sh first"
        echo "  (须先 export LLVM_MINGW=/path/to/llvm-mingw-<tag>-ucrt-...-x86_64)"
        exit 1
    fi

    # PE 工具由 env.sh 统一派生: PE_CLANG/PE_CXX/PE_LLDLINK/PE_AR/PE_STRIP/PE_READOBJ

    # windres: 优先 LLVM_MINGW 的 llvm-windres, 缺失则回退
    # llvm-mingw 的带前缀 windres; 包一层 shim 以固定 target
    if [ -f "${LLVM_MINGW}/bin/llvm-windres" ]; then
        EC_WINDRES_BASE="${LLVM_MINGW}/bin/llvm-windres"
    else
        EC_WINDRES_BASE="${EC_SYSROOT}/../bin/arm64ec-w64-mingw32-windres"
    fi
    if [ ! -f "${EC_WINDRES_BASE}" ]; then
        echo "ERROR: llvm-windres not found (LLVM_MINGW=${LLVM_MINGW})"
        exit 1
    fi
    cat > ${VKD3D_BUILD_DIR}/windres-ec << EOF
#!/bin/sh
# --sysroot/include: mingw-w64 头 (version.rc 需要 windows.h)
exec ${EC_WINDRES_BASE} --target=arm64ec-w64-mingw32 \
    -I${EC_SYSROOT}/include \$@
EOF
    chmod +x ${VKD3D_BUILD_DIR}/windres-ec

    # resource-dir: 让 clang 的 -rtlib=compiler-rt 能解析到 arm64ec
    # 可用的 builtins (llvm-mingw 的 clang resource 目录, 通用发行包没有)
    EC_RESOURCE_ELEM=""
    if [ -n "${EC_RESOURCE_DIR}" ]; then
        EC_RESOURCE_ELEM=", '-resource-dir=${EC_RESOURCE_DIR}'"
    fi

    # builtins 库显式加入链接参数 (兜底: resource-dir 之外再点名一次)
    EC_BUILTINS_ELEM=""
    if [ -n "${EC_BUILTINS_LIB}" ]; then
        EC_BUILTINS_ELEM=", '${EC_BUILTINS_LIB}'"
    fi

    # 注意参数分区:
    #   c_args/cpp_args 只放纯编译期参数 (--target/--sysroot/-rtlib/
    #   -resource-dir)。
    #   链接期参数 (-unwindlib/-fuse-ld=lld/-L/builtins.a/-resource-dir)
    #   必须放 *_link_args, 否则 meson 的编译探测 (-c + -Werror=unused-
    #   command-line-argument) 会报 argument unused, 误杀所有
    #   compiler_args 探测。
    #   -resource-dir 两边都要有: 链接期没有它, clang 会去自己的 resource
    #   目录找 arm64ec builtins (不存在) 而链接失败。
    cat > ${VKD3D_BUILD_DIR}/build-arm64ec.txt << EOF
[binaries]
c = '${PE_CLANG}'
cpp = '${PE_CXX}'
ar = '${PE_AR}'
strip = '${PE_STRIP}'
windres = '${VKD3D_BUILD_DIR}/windres-ec'
widl = '${OHOS_BUILD_ROOT}/build-tools/tools/widl/widl'

[properties]
needs_exe_wrapper = true
sys_root = '${EC_SYSROOT}'
c_args = ['--target=arm64ec-w64-mingw32', '--sysroot=${EC_SYSROOT}',
          '-rtlib=compiler-rt'${EC_RESOURCE_ELEM}]
cpp_args = ['--target=arm64ec-w64-mingw32', '--sysroot=${EC_SYSROOT}',
            '-rtlib=compiler-rt', '-stdlib=libc++', '-include', 'algorithm'${EC_RESOURCE_ELEM}]
c_link_args = ['--target=arm64ec-w64-mingw32', '--sysroot=${EC_SYSROOT}',
               '-rtlib=compiler-rt', '-unwindlib=libunwind', '-fuse-ld=lld', '--ld-path=${VKD3D_BUILD_DIR}/ld.lld'${EC_RESOURCE_ELEM},
               '-L${EC_SYSROOT}/lib'${EC_BUILTINS_ELEM}]
cpp_link_args = ['--target=arm64ec-w64-mingw32', '--sysroot=${EC_SYSROOT}',
                 '-rtlib=compiler-rt', '-stdlib=libc++', '-unwindlib=libunwind',
                 '-fuse-ld=lld', '--ld-path=${VKD3D_BUILD_DIR}/ld.lld'${EC_RESOURCE_ELEM},
                 '-L${EC_SYSROOT}/lib'${EC_BUILTINS_ELEM}]

[host_machine]
system = 'windows'
cpu_family = 'aarch64'
cpu = 'arm64ec'
endian = 'little'
EOF

    cat > ${VKD3D_BUILD_DIR}/ld.lld << EOF
#!/bin/bash
# 过滤 meson find_library link test 加的 --allow-shlib-undefined (lld 不认)
args=()
for a in "\$@"; do
  [ "\$a" = "--allow-shlib-undefined" ] && continue
  args+=("\$a")
done
exec ${LLVM_MINGW}/bin/ld.lld "\${args[@]}"
EOF
    chmod +x ${VKD3D_BUILD_DIR}/ld.lld
    echo "--- Building vkd3d-proton arm64ec ---"
    if [ ! -f "${VKD3D_BUILD_DIR}/buildec/build.ninja" ]; then
        PATH="${VKD3D_BUILD_DIR}:${GLSLANG_DIR}:${OHOS_BUILD_ROOT}/build-tools/tools/widl:${PATH}" meson setup ${VKD3D_BUILD_DIR}/buildec ${VKD3D_PROTON_DIR} \
            --cross-file ${VKD3D_BUILD_DIR}/build-arm64ec.txt \
            --prefix ${VKD3D_BUILD_DIR}/installec \
            -Dbuildtype=release -Db_lundef=false
    fi

    # Use limited parallelism to avoid OOM
    # ninja 编译着色器时也要调 glslangValidator, 同样命令级注入
    PATH="${VKD3D_BUILD_DIR}:${GLSLANG_DIR}:${OHOS_BUILD_ROOT}/build-tools/tools/widl:${PATH}" ninja -C ${VKD3D_BUILD_DIR}/buildec -j4 install

    # Verify artifacts: machine type 必须是 ARM64EC (0xA641),
    # 防止 target 配错时静默编成纯 aarch64 (wow64 下无法加载)
    echo "--- Verifying vkd3d-proton arm64ec artifacts ---"
    for dll in d3d12.dll; do
        f="${VKD3D_BUILD_DIR}/installec/bin/${dll}"
        if [ ! -f "${f}" ]; then
            f="${VKD3D_BUILD_DIR}/installec/lib/${dll}"
        fi
        if [ ! -f "${f}" ]; then
            echo "MISSING: ${VKD3D_BUILD_DIR}/installec/{bin,lib}/${dll}"
            exit 1
        fi
        if ! ${PE_READOBJ} --file-headers "${f}" | grep -q "ARM64EC"; then
            echo "ERROR: ${f} is not ARM64EC machine type"
            ${PE_READOBJ} --file-headers "${f}" | grep -i "machine"
            exit 1
        fi
        echo "OK: ${f}"
    done
else
    echo "SKIP: vkd3d-proton arm64ec build requires --build-arm64ec"
fi

# ============================================================
# 2. Build 32-bit vkd3d-proton (i386, 系统 mingw-w64 工具链)
# ============================================================
# Use system-installed mingw-w64 toolchain
# These have the dxgi/gdi32/d3d12 import libraries needed by vkd3d-proton
if ! command -v x86_64-w64-mingw32-gcc &> /dev/null; then
    echo "ERROR: mingw-w64 (x86_64) not found. Install with: sudo apt install gcc-mingw-w64 g++-mingw-w64"
    exit 1
fi
if ! command -v i686-w64-mingw32-gcc &> /dev/null; then
    echo "ERROR: mingw-w64 (i686) not found. Install with: sudo apt install gcc-mingw-w64 g++-mingw-w64"
    exit 1
fi

cat > ${VKD3D_BUILD_DIR}/build-ohos32.txt << 'EOF'
[binaries]
c = 'i686-w64-mingw32-gcc'
cpp = 'i686-w64-mingw32-g++'
ar = 'i686-w64-mingw32-ar'
strip = 'i686-w64-mingw32-strip'
windres = 'i686-w64-mingw32-windres'

[properties]
needs_exe_wrapper = true

[host_machine]
system = 'windows'
cpu_family = 'x86'
cpu = 'x86'
endian = 'little'
EOF

echo "--- Building vkd3d-proton x86 ---"
if [ ! -f "${VKD3D_BUILD_DIR}/build32/build.ninja" ]; then
    PATH="${GLSLANG_DIR}:${OHOS_BUILD_ROOT}/build-tools/tools/widl:${PATH}" meson setup ${VKD3D_BUILD_DIR}/build32 ${VKD3D_PROTON_DIR} \
        --cross-file ${VKD3D_BUILD_DIR}/build-ohos32.txt \
        --prefix ${VKD3D_BUILD_DIR}/install32 \
        -Dbuildtype=release
fi

# Use limited parallelism to avoid OOM
PATH="${GLSLANG_DIR}:${OHOS_BUILD_ROOT}/build-tools/tools/widl:${PATH}" ninja -C ${VKD3D_BUILD_DIR}/build32 -j4 install

# Verify artifacts
echo "--- Verifying vkd3d-proton artifacts ---"
for dll in d3d12.dll; do
    if [ -f "${VKD3D_BUILD_DIR}/install32/bin/${dll}" ]; then
        echo "OK: ${VKD3D_BUILD_DIR}/install32/bin/${dll}"
    else
        echo "MISSING: ${VKD3D_BUILD_DIR}/install32/bin/${dll}"
        exit 1
    fi
done

echo "vkd3d-proton build completed successfully!"

#!/bin/bash

# Build DXVK for OHOS (cross-compile to Windows PE DLLs)
#
# 64-bit: arm64ec (wow64 mode), 用 third_party/llvm 的 clang +
#         llvm-mingw 的 arm64ec sysroot (见 build_llvm_mingw_ec.sh)
# 32-bit: i386, 用系统 mingw-w64 工具链 (wow64 下 32 位游戏走 xtajit)

set -ex

DXVK_DIR="${ROOT_DIR}/third_party/dxvk"
DXVK_BUILD_DIR="${BUILD_DIR}/dxvk"

# DXVK (v1.10.3) 的 meson.build 硬依赖 glslangValidator 编译着色器
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
mkdir -p ${DXVK_BUILD_DIR}

# ============================================================
# 1. Build 64-bit DXVK (arm64ec)
# ============================================================
if [ "${BUILD_ARM64EC}" -eq 1 ]; then
    # 依赖 build_llvm_mingw_ec.sh 导出的: EC_SYSROOT, EC_BUILTINS_LIB
    if [ -z "${EC_SYSROOT}" ]; then
        echo "ERROR: EC_SYSROOT not set, source ohos/script/build_llvm_mingw_ec.sh first"
        exit 1
    fi

    CLANG="${LLVM_TOOLCHAIN}/bin/clang"
    CLANGXX="${LLVM_TOOLCHAIN}/bin/clang++"
    LLDLINK="${LLVM_TOOLCHAIN}/bin/lld-link"
    for tool in "${CLANG}" "${CLANGXX}" \
                "${LLVM_TOOLCHAIN}/bin/llvm-ar" \
                "${LLVM_TOOLCHAIN}/bin/llvm-strip" \
                "${LLVM_TOOLCHAIN}/bin/llvm-readobj" \
                "${LLDLINK}"; do
        if [ ! -f "${tool}" ]; then
            echo "ERROR: LLVM toolchain missing ${tool} (third_party/llvm)"
            exit 1
        fi
    done

    # ------------------------------------------------------------
    # 1.1 生成 arm64ec 导入库 (vulkan-1, d3dcompiler_43)
    #
    # dxvk meson.build 对非 x86_64 架构固定从源码树 lib32 找
    # vulkan-1/d3dcompiler_43, 但 lib32 是 x86 库且与 i386 构建共用,
    # 故为 arm64ec 单独生成 lib-arm64ec/ 目录:
    #   - d3dcompiler_43: 用 dxvk 自带的 def (def 与架构无关)
    #   - vulkan-1: dxvk 只引用 vkGetInstanceProcAddr 一个导入符号
    #     (src/vulkan/vulkan_loader.cpp), 自写最小 def
    # dxvk meson.build 的 dxvk_library_path 已加 aarch64 分支指向此目录
    # ------------------------------------------------------------
    EC_IMPLIB_DIR="${DXVK_DIR}/lib-arm64ec"
    mkdir -p ${EC_IMPLIB_DIR}

    if [ ! -f "${EC_IMPLIB_DIR}/vulkan-1.lib" ]; then
        cat > ${DXVK_BUILD_DIR}/vulkan-1.def << 'EOF'
LIBRARY vulkan-1.dll
EXPORTS
    vkGetInstanceProcAddr
EOF
        ${LLDLINK} /MACHINE:ARM64EC /NOLOGO /DEF:${DXVK_BUILD_DIR}/vulkan-1.def \
            /OUT:${EC_IMPLIB_DIR}/vulkan-1.lib
    fi

    if [ ! -f "${EC_IMPLIB_DIR}/d3dcompiler_43.lib" ]; then
        ${LLDLINK} /MACHINE:ARM64EC /NOLOGO /DEF:${DXVK_DIR}/lib/libd3dcompiler_43.def \
            /OUT:${EC_IMPLIB_DIR}/d3dcompiler_43.lib
    fi

    # windres: 优先 third_party/llvm 的 llvm-windres, 缺失则回退
    # llvm-mingw 的带前缀 windres; 包一层 shim 以固定 target
    if [ -f "${LLVM_TOOLCHAIN}/bin/llvm-windres" ]; then
        EC_WINDRES_BASE="${LLVM_TOOLCHAIN}/bin/llvm-windres"
    else
        EC_WINDRES_BASE="${EC_SYSROOT}/../bin/arm64ec-w64-mingw32-windres"
    fi
    if [ ! -f "${EC_WINDRES_BASE}" ]; then
        echo "ERROR: llvm-windres not found (neither third_party/llvm nor llvm-mingw)"
        exit 1
    fi
    cat > ${DXVK_BUILD_DIR}/windres-ec << EOF
#!/bin/sh
# --sysroot/include: mingw-w64 头 (version.rc 需要 windows.h)
exec ${EC_WINDRES_BASE} --target=arm64ec-w64-mingw32 \
    -I${EC_SYSROOT}/include \$@
EOF
    chmod +x ${DXVK_BUILD_DIR}/windres-ec

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
    #   -stdlib/-resource-dir)。
    #   链接期参数 (-unwindlib/-fuse-ld=lld/-L/builtins.a/-resource-dir)
    #   必须放 *_link_args, 否则 meson 的编译探测 (-c + -Werror=unused-
    #   command-line-argument) 会报 argument unused, 误杀所有
    #   compiler_args 探测 (连 -DNOMINMAX 都会 NO)。
    #   -resource-dir 两边都要有: 链接期没有它, clang 会去自己的 resource
    #   目录找 arm64ec builtins (不存在) 而链接失败。
    cat > ${DXVK_BUILD_DIR}/build-arm64ec.txt << EOF
[binaries]
c = '${CLANG}'
cpp = '${CLANGXX}'
ar = '${LLVM_TOOLCHAIN}/bin/llvm-ar'
strip = '${LLVM_TOOLCHAIN}/bin/llvm-strip'
windres = '${DXVK_BUILD_DIR}/windres-ec'

[properties]
needs_exe_wrapper = true
sys_root = '${EC_SYSROOT}'
c_args = ['--target=arm64ec-w64-mingw32', '--sysroot=${EC_SYSROOT}',
          '-rtlib=compiler-rt'${EC_RESOURCE_ELEM}]
cpp_args = ['--target=arm64ec-w64-mingw32', '--sysroot=${EC_SYSROOT}',
            '-rtlib=compiler-rt', '-stdlib=libc++'${EC_RESOURCE_ELEM}]
c_link_args = ['--target=arm64ec-w64-mingw32', '--sysroot=${EC_SYSROOT}',
               '-rtlib=compiler-rt', '-unwindlib=libunwind', '-fuse-ld=lld'${EC_RESOURCE_ELEM},
               '-L${EC_SYSROOT}/lib'${EC_BUILTINS_ELEM}]
cpp_link_args = ['--target=arm64ec-w64-mingw32', '--sysroot=${EC_SYSROOT}',
                 '-rtlib=compiler-rt', '-stdlib=libc++', '-unwindlib=libunwind',
                 '-fuse-ld=lld'${EC_RESOURCE_ELEM},
                 '-L${EC_SYSROOT}/lib'${EC_BUILTINS_ELEM}]

[host_machine]
system = 'windows'
cpu_family = 'aarch64'
cpu = 'arm64ec'
endian = 'little'
EOF

    echo "--- Building DXVK arm64ec ---"
    if [ ! -f "${DXVK_BUILD_DIR}/buildec/build.ninja" ]; then
        PATH="${GLSLANG_DIR}:${PATH}" meson setup ${DXVK_BUILD_DIR}/buildec ${DXVK_DIR} \
            --cross-file ${DXVK_BUILD_DIR}/build-arm64ec.txt \
            --prefix ${DXVK_BUILD_DIR}/installec \
            -Dbuildtype=release
    fi

    # Use limited parallelism to avoid OOM
    # ninja 编译着色器时也要调 glslangValidator, 同样命令级注入
    PATH="${GLSLANG_DIR}:${PATH}" ninja -C ${DXVK_BUILD_DIR}/buildec -j4 install

    # Verify artifacts: machine type 必须是 ARM64EC (0xA641),
    # 防止 target 配错时静默编成纯 aarch64 (wow64 下无法加载)
    echo "--- Verifying DXVK arm64ec artifacts ---"
    for dll in d3d11.dll dxgi.dll; do
        f="${DXVK_BUILD_DIR}/installec/bin/${dll}"
        if [ ! -f "${f}" ]; then
            f="${DXVK_BUILD_DIR}/installec/lib/${dll}"
        fi
        if [ ! -f "${f}" ]; then
            echo "MISSING: ${DXVK_BUILD_DIR}/installec/{bin,lib}/${dll}"
            exit 1
        fi
        if ! ${LLVM_TOOLCHAIN}/bin/llvm-readobj --file-headers "${f}" | grep -q "ARM64EC"; then
            echo "ERROR: ${f} is not ARM64EC machine type"
            ${LLVM_TOOLCHAIN}/bin/llvm-readobj --file-headers "${f}" | grep -i "machine"
            exit 1
        fi
        echo "OK: ${f}"
    done
else
    echo "SKIP: DXVK arm64ec build requires --build-arm64ec"
fi

# ============================================================
# 2. Build 32-bit DXVK (i386, 系统 mingw-w64 工具链)
# ============================================================
# Use system-installed mingw-w64 toolchain
# These have the d3d9/d3d11/dxgi import libraries needed by DXVK
if ! command -v x86_64-w64-mingw32-gcc &> /dev/null; then
    echo "ERROR: mingw-w64 (x86_64) not found. Install with: sudo apt install gcc-mingw-w64 g++-mingw-w64"
    exit 1
fi
if ! command -v i686-w64-mingw32-gcc &> /dev/null; then
    echo "ERROR: mingw-w64 (i686) not found. Install with: sudo apt install gcc-mingw-w64 g++-mingw-w64"
    exit 1
fi

cat > ${DXVK_BUILD_DIR}/build-ohos32.txt << 'EOF'
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

echo "--- Building DXVK x86 ---"
if [ ! -f "${DXVK_BUILD_DIR}/build32/build.ninja" ]; then
    PATH="${GLSLANG_DIR}:${PATH}" meson setup ${DXVK_BUILD_DIR}/build32 ${DXVK_DIR} \
        --cross-file ${DXVK_BUILD_DIR}/build-ohos32.txt \
        --prefix ${DXVK_BUILD_DIR}/install32 \
        -Dbuildtype=release
fi

# Use limited parallelism to avoid OOM
PATH="${GLSLANG_DIR}:${PATH}" ninja -C ${DXVK_BUILD_DIR}/build32 -j4 install

# Verify artifacts
echo "--- Verifying DXVK artifacts ---"
for dll in d3d11.dll dxgi.dll; do
    if [ -f "${DXVK_BUILD_DIR}/install32/bin/${dll}" ]; then
        echo "OK: ${DXVK_BUILD_DIR}/install32/bin/${dll}"
    else
        echo "MISSING: ${DXVK_BUILD_DIR}/install32/bin/${dll}"
        exit 1
    fi
done

echo "DXVK build completed successfully!"

#!/bin/bash

# Build DXVK 1.10.3 (legacy) for OHOS (cross-compile to Windows PE DLLs)
#
# 64-bit: ARM64X (0xA64E, hybrid ARM64 + arm64ec). ARM64X 让 ARM64 进程
#         (如 wine_d3d11_test.exe, 0xAA64) 能直接加载, 避免 c000007b。
#         机制: 编两套 obj (arm64ec + aarch64), lld-link -machine:arm64x 合并。
# 32-bit: i386, 用系统 mingw-w64 工具链 (wow64 下 32 位游戏走 xtajit)
#
# DXVK 1.10.3 与 2.6.2 的差异 (影响本脚本):
#   - meson.build 对非 x86_64 架构固定从源码树 lib32 找
#     vulkan-1/d3dcompiler_43 (lib32 是 x86 库, 与 i386 构建共用),
#     故为 arm64ec 单独生成 lib-arm64ec/ 目录:
#       - d3dcompiler_43: 用 dxvk-legacy 自带的 def (def 与架构无关)
#         (源码树 lib/libd3dcompiler_43.def 预置)
#       - vulkan-1: dxvk 只引用 vkGetInstanceProcAddr 一个导入符号
#         (src/vulkan/vulkan_loader.cpp), 自写最小 def
#   - meson.build 的 dxvk_library_path 有 aarch64 分支指向 lib-arm64ec
#
# env.sh 只派生 DXVK_DIR/DXVK_BUILD_DIR (2.6.2) 与 VKD3D_* ;
# 1.10.3 路径 (DXVK_LEGACY_DIR/DXVK_LEGACY_BUILD_DIR) 在本脚本内定义。

set -ex
# env.sh 派生 OHOS_BUILD_ROOT / PE_* / EC_* / LLVM_MINGW
DXVK_LEGACY_DIR="${OHOS_BUILD_ROOT}/third_party/dxvk-legacy"
DXVK_LEGACY_BUILD_DIR="${OHOS_BUILD_ROOT}/build/dxvk-legacy"

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
mkdir -p ${DXVK_LEGACY_BUILD_DIR}

# ============================================================
# 1. Build 64-bit DXVK legacy (arm64ec)
# ============================================================
if [ "${BUILD_ARM64EC}" -eq 1 ]; then
    # 依赖 env.sh 导出的: EC_SYSROOT, EC_BUILTINS_LIB
    if [ -z "${EC_SYSROOT}" ]; then
        echo "ERROR: EC_SYSROOT not set, source ohos/script/env.sh first"
        echo "  (须先 export LLVM_MINGW=/path/to/llvm-mingw-<tag>-ucrt-...-x86_64)"
        exit 1
    fi

    # PE 工具由 env.sh 统一派生: PE_CLANG/PE_CXX/PE_LLDLINK/PE_AR/PE_STRIP/PE_READOBJ

    # ------------------------------------------------------------
    # 1.1 生成 arm64ec 导入库 (vulkan-1, d3dcompiler_43)
    #
    # dxvk-legacy meson.build 对非 x86_64 架构固定从源码树 lib32 找
    # vulkan-1/d3dcompiler_43, 但 lib32 是 x86 库且与 i386 构建共用,
    # 故为 arm64ec 单独生成 lib-arm64ec/ 目录:
    #   - d3dcompiler_43: 用 dxvk-legacy 自带的 def (def 与架构无关)
    #   - vulkan-1: dxvk 只引用 vkGetInstanceProcAddr 一个导入符号
    #     (src/vulkan/vulkan_loader.cpp), 自写最小 def
    # dxvk-legacy meson.build 的 dxvk_library_path 已加 aarch64 分支指向此目录
    # ------------------------------------------------------------
    EC_IMPLIB_DIR="${DXVK_LEGACY_DIR}/lib-arm64ec"
    mkdir -p ${EC_IMPLIB_DIR}

    cat > ${DXVK_LEGACY_BUILD_DIR}/vulkan-1.def << 'EOF'
LIBRARY vulkan-1.dll
EXPORTS
    vkGetInstanceProcAddr
EOF

    # 生成两种架构的导入库 (EC 和 native 各一份, .lib 格式不兼容)
    # ARM64EC 版本
    if [ ! -f "${EC_IMPLIB_DIR}/vulkan-1-ec.lib" ]; then
        ${PE_LLDLINK} /MACHINE:ARM64EC /NOLOGO /DEF:${DXVK_LEGACY_BUILD_DIR}/vulkan-1.def \
            /OUT:${EC_IMPLIB_DIR}/vulkan-1-ec.lib
    fi
    if [ ! -f "${EC_IMPLIB_DIR}/d3dcompiler_43-ec.lib" ]; then
        ${PE_LLDLINK} /MACHINE:ARM64EC /NOLOGO /DEF:${DXVK_LEGACY_DIR}/lib/libd3dcompiler_43.def \
            /OUT:${EC_IMPLIB_DIR}/d3dcompiler_43-ec.lib
    fi
    # ARM64 版本
    if [ ! -f "${EC_IMPLIB_DIR}/vulkan-1-a64.lib" ]; then
        ${PE_LLDLINK} /MACHINE:ARM64 /NOLOGO /DEF:${DXVK_LEGACY_BUILD_DIR}/vulkan-1.def \
            /OUT:${EC_IMPLIB_DIR}/vulkan-1-a64.lib
    fi
    if [ ! -f "${EC_IMPLIB_DIR}/d3dcompiler_43-a64.lib" ]; then
        ${PE_LLDLINK} /MACHINE:ARM64 /NOLOGO /DEF:${DXVK_LEGACY_DIR}/lib/libd3dcompiler_43.def \
            /OUT:${EC_IMPLIB_DIR}/d3dcompiler_43-a64.lib
    fi

    # 为 arm64ec 构建准备: 用 EC 版本
    cp -f ${EC_IMPLIB_DIR}/vulkan-1-ec.lib ${EC_IMPLIB_DIR}/vulkan-1.lib
    cp -f ${EC_IMPLIB_DIR}/d3dcompiler_43-ec.lib ${EC_IMPLIB_DIR}/d3dcompiler_43.lib

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
    cat > ${DXVK_LEGACY_BUILD_DIR}/windres-ec << EOF
#!/bin/sh
# --sysroot/include: mingw-w64 头 (version.rc 需要 windows.h)
exec ${EC_WINDRES_BASE} --target=arm64ec-w64-mingw32 \
    -I${EC_SYSROOT}/include \$@
EOF
    chmod +x ${DXVK_LEGACY_BUILD_DIR}/windres-ec

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
    cat > ${DXVK_LEGACY_BUILD_DIR}/build-arm64ec.txt << EOF
[binaries]
c = '${PE_CLANG}'
cpp = '${PE_CXX}'
ar = '${PE_AR}'
strip = '${PE_STRIP}'
windres = '${DXVK_LEGACY_BUILD_DIR}/windres-ec'

[properties]
needs_exe_wrapper = true
sys_root = '${EC_SYSROOT}'
c_args = ['--target=arm64ec-w64-mingw32', '--sysroot=${EC_SYSROOT}',
          '-rtlib=compiler-rt', '-D__OHOS__=1'${EC_RESOURCE_ELEM}]
cpp_args = ['--target=arm64ec-w64-mingw32', '--sysroot=${EC_SYSROOT}',
            '-rtlib=compiler-rt', '-stdlib=libc++', '-D__OHOS__=1'${EC_RESOURCE_ELEM}]
c_link_args = ['--target=arm64ec-w64-mingw32', '--sysroot=${EC_SYSROOT}',
               '-rtlib=compiler-rt', '-unwindlib=libunwind', '-fuse-ld=lld', '--ld-path=${DXVK_LEGACY_BUILD_DIR}/ld.lld'${EC_RESOURCE_ELEM},
               '-L${EC_SYSROOT}/lib'${EC_BUILTINS_ELEM}]
cpp_link_args = ['--target=arm64ec-w64-mingw32', '--sysroot=${EC_SYSROOT}',
                 '-rtlib=compiler-rt', '-stdlib=libc++', '-unwindlib=libunwind',
                 '-fuse-ld=lld', '--ld-path=${DXVK_LEGACY_BUILD_DIR}/ld.lld'${EC_RESOURCE_ELEM},
                 '-L${EC_SYSROOT}/lib'${EC_BUILTINS_ELEM}]

[host_machine]
system = 'windows'
cpu_family = 'aarch64'
cpu = 'arm64ec'
endian = 'little'
EOF

    cat > ${DXVK_LEGACY_BUILD_DIR}/ld.lld << EOF
#!/bin/bash
# 过滤 meson find_library link test 加的 --allow-shlib-undefined (lld 不认)
args=()
for a in "\$@"; do
  [ "\$a" = "--allow-shlib-undefined" ] && continue
  args+=("\$a")
done
exec ${LLVM_MINGW}/bin/ld.lld "\${args[@]}"
EOF
    chmod +x ${DXVK_LEGACY_BUILD_DIR}/ld.lld
    echo "--- Building DXVK legacy arm64ec ---"
    if [ ! -f "${DXVK_LEGACY_BUILD_DIR}/buildec/build.ninja" ]; then
        PATH="${DXVK_LEGACY_BUILD_DIR}:${GLSLANG_DIR}:${PATH}" meson setup ${DXVK_LEGACY_BUILD_DIR}/buildec ${DXVK_LEGACY_DIR} \
            --cross-file ${DXVK_LEGACY_BUILD_DIR}/build-arm64ec.txt \
            --prefix ${DXVK_LEGACY_BUILD_DIR}/installec \
            -Dbuildtype=release -Db_lundef=false
    fi

    # Use limited parallelism to avoid OOM
    # ninja 编译着色器时也要调 glslangValidator, 同样命令级注入
    PATH="${DXVK_LEGACY_BUILD_DIR}:${GLSLANG_DIR}:${PATH}" ninja -C ${DXVK_LEGACY_BUILD_DIR}/buildec -j4 install

    # Verify artifacts: machine type 必须是 ARM64EC (0xA641),
    # 防止 target 配错时静默编成纯 aarch64 (wow64 下无法加载)
    echo "--- Verifying DXVK legacy arm64ec artifacts ---"
    for dll in d3d11.dll dxgi.dll; do
        f="${DXVK_LEGACY_BUILD_DIR}/installec/bin/${dll}"
        if [ ! -f "${f}" ]; then
            f="${DXVK_LEGACY_BUILD_DIR}/installec/lib/${dll}"
        fi
        if [ ! -f "${f}" ]; then
            echo "MISSING: ${DXVK_LEGACY_BUILD_DIR}/installec/{bin,lib}/${dll}"
            exit 1
        fi
        if ! ${PE_READOBJ} --file-headers "${f}" | grep -q "ARM64EC"; then
            echo "ERROR: ${f} is not ARM64EC machine type"
            ${PE_READOBJ} --file-headers "${f}" | grep -i "machine"
            exit 1
        fi
        echo "OK: ${f}"
    done

    # ---- aarch64 (native ARM64) 第二套 obj, 供 ARM64X 合并 ----
    AARCH64_SYSROOT="${LLVM_MINGW}/aarch64-w64-mingw32"
    [ -d "${AARCH64_SYSROOT}" ] || { echo "ERROR: aarch64 sysroot missing: ${AARCH64_SYSROOT}"; exit 1; }

    # windres / ld.lld wrappers for aarch64
    A64_WINDRES_BASE="${AARCH64_SYSROOT}/../bin/aarch64-w64-mingw32-windres"
    [ -f "${A64_WINDRES_BASE}" ] || A64_WINDRES_BASE="${LLVM_MINGW}/bin/llvm-windres"
    cat > ${DXVK_LEGACY_BUILD_DIR}/windres-a64 << EOF
#!/bin/sh
exec ${A64_WINDRES_BASE} --target=aarch64-w64-mingw32 -I${AARCH64_SYSROOT}/include \$@
EOF
    chmod +x ${DXVK_LEGACY_BUILD_DIR}/windres-a64
    cat > ${DXVK_LEGACY_BUILD_DIR}/ld.lld-a64 << EOF
#!/bin/bash
args=()
for a in "\$@"; do
  [ "\$a" = "--allow-shlib-undefined" ] && continue
  args+=("\$a")
done
exec ${LLVM_MINGW}/bin/ld.lld "\${args[@]}"
EOF
    chmod +x ${DXVK_LEGACY_BUILD_DIR}/ld.lld-a64

    A64_RESOURCE_ELEM=""
    [ -n "${EC_RESOURCE_DIR}" ] && A64_RESOURCE_ELEM=", '-resource-dir=${EC_RESOURCE_DIR}'"
    A64_BUILTINS_ELEM=""
    [ -n "${EC_BUILTINS_LIB}" ] && A64_BUILTINS_ELEM=", '${EC_BUILTINS_LIB}'"

    cat > ${DXVK_LEGACY_BUILD_DIR}/build-aarch64.txt << EOF
[binaries]
c = '${PE_CLANG}'
cpp = '${PE_CXX}'
ar = '${PE_AR}'
strip = '${PE_STRIP}'
windres = '${DXVK_LEGACY_BUILD_DIR}/windres-a64'

[properties]
needs_exe_wrapper = true
sys_root = '${AARCH64_SYSROOT}'
c_args = ['--target=aarch64-w64-mingw32', '--sysroot=${AARCH64_SYSROOT}',
          '-rtlib=compiler-rt', '-D__OHOS__=1'${A64_RESOURCE_ELEM}]
cpp_args = ['--target=aarch64-w64-mingw32', '--sysroot=${AARCH64_SYSROOT}',
            '-rtlib=compiler-rt', '-stdlib=libc++', '-D__OHOS__=1'${A64_RESOURCE_ELEM}]
c_link_args = ['--target=aarch64-w64-mingw32', '--sysroot=${AARCH64_SYSROOT}',
               '-rtlib=compiler-rt', '-unwindlib=libunwind', '-fuse-ld=lld', '--ld-path=${DXVK_LEGACY_BUILD_DIR}/ld.lld-a64'${A64_RESOURCE_ELEM},
               '-L${AARCH64_SYSROOT}/lib'${A64_BUILTINS_ELEM}]
cpp_link_args = ['--target=aarch64-w64-mingw32', '--sysroot=${AARCH64_SYSROOT}',
                 '-rtlib=compiler-rt', '-stdlib=libc++', '-unwindlib=libunwind',
                 '-fuse-ld=lld', '--ld-path=${DXVK_LEGACY_BUILD_DIR}/ld.lld-a64'${A64_RESOURCE_ELEM},
                 '-L${AARCH64_SYSROOT}/lib'${A64_BUILTINS_ELEM}]

[host_machine]
system = 'windows'
cpu_family = 'aarch64'
cpu = 'aarch64'
endian = 'little'
EOF

    # 为 aarch64 构建准备: 用 ARM64 版本导入库
    cp -f ${EC_IMPLIB_DIR}/vulkan-1-a64.lib ${EC_IMPLIB_DIR}/vulkan-1.lib
    cp -f ${EC_IMPLIB_DIR}/d3dcompiler_43-a64.lib ${EC_IMPLIB_DIR}/d3dcompiler_43.lib

    echo "--- Building DXVK legacy aarch64 (native ARM64) ---"
    if [ ! -f "${DXVK_LEGACY_BUILD_DIR}/buildarm64/build.ninja" ]; then
        PATH="${DXVK_LEGACY_BUILD_DIR}:${GLSLANG_DIR}:${PATH}" meson setup ${DXVK_LEGACY_BUILD_DIR}/buildarm64 ${DXVK_LEGACY_DIR} \
            --cross-file ${DXVK_LEGACY_BUILD_DIR}/build-aarch64.txt \
            --prefix ${DXVK_LEGACY_BUILD_DIR}/installarm64 \
            -Dbuildtype=release -Db_lundef=false
    fi
    PATH="${DXVK_LEGACY_BUILD_DIR}:${GLSLANG_DIR}:${PATH}" ninja -C ${DXVK_LEGACY_BUILD_DIR}/buildarm64 -j4 install

    # ---- ARM64X 合并: lld-link -machine:arm64x ----
    EC_SYSROOT_LIB="${EC_SYSROOT}/lib"
    A64_SYSROOT_LIB="${AARCH64_SYSROOT}/lib"

    cat > ${DXVK_LEGACY_BUILD_DIR}/arm64x_ctors_stub.c << 'EOF'
asm( ".section .ctors,\"dw\"\n"
     ".balign 8\n"
     ".globl __CTOR_LIST__\n"
     "__CTOR_LIST__:\n"
     "\t.quad -1\n"
     "\t.quad 0\n"
     ".section .dtors,\"dw\"\n"
     ".balign 8\n"
     ".globl __DTOR_LIST__\n"
     "__DTOR_LIST__:\n"
     "\t.quad -1\n"
     "\t.quad 0\n" );
EOF
    ${PE_CLANG} --target=arm64ec-w64-mingw32 -c -o ${DXVK_LEGACY_BUILD_DIR}/ctors_stub_ec.o ${DXVK_LEGACY_BUILD_DIR}/arm64x_ctors_stub.c
    ${PE_CLANG} --target=aarch64-w64-mingw32 -c -o ${DXVK_LEGACY_BUILD_DIR}/ctors_stub_native.o ${DXVK_LEGACY_BUILD_DIR}/arm64x_ctors_stub.c

    merge_arm64x_dll() {
        local target="$1" deffile="$2" out="$3"
        local ec_cmd ec_obj static_lib sys_lib builtins
        ec_cmd=$(cd ${DXVK_LEGACY_BUILD_DIR}/buildec && ninja -t commands "${target}" 2>/dev/null | tail -1)
        [ -z "${ec_cmd}" ] && { echo "ERROR: cannot get ninja command for ${target}"; return 1; }

        local args=("${PE_LLDLINK}" -machine:arm64x -dll -out:"${out}" -subsystem:windows
                    -entry:DllMainCRTStartup
                    -libpath:"${EC_SYSROOT_LIB}" -libpath:"${A64_SYSROOT_LIB}")

        for o in $(printf '%s\n' "${ec_cmd}" | tr ' ' '\n' | grep -E '\.(o|obj)$'); do
            args+=("${DXVK_LEGACY_BUILD_DIR}/buildec/${o}")
        done
        for o in $(printf '%s\n' "${ec_cmd}" | tr ' ' '\n' | grep -E '\.(o|obj)$'); do
            case "$o" in *version*.o) continue;; esac
            args+=("${DXVK_LEGACY_BUILD_DIR}/buildarm64/${o}")
        done
        args+=("${DXVK_LEGACY_BUILD_DIR}/ctors_stub_ec.o" "${DXVK_LEGACY_BUILD_DIR}/ctors_stub_native.o")
        for d in "${EC_SYSROOT_LIB}" "${A64_SYSROOT_LIB}"; do
            [ -f "$d/dllcrt2.o" ] && args+=("$d/dllcrt2.o")
            [ -f "$d/crtbegin.o" ] && args+=("$d/crtbegin.o")
            [ -f "$d/crtend.o" ] && args+=("$d/crtend.o")
        done
        for lib in $(printf '%s\n' "${ec_cmd}" | tr ' ' '\n' | grep -E '(^src/|^subprojects/).*\.a$'); do
            args+=("${DXVK_LEGACY_BUILD_DIR}/buildec/${lib}" "${DXVK_LEGACY_BUILD_DIR}/buildarm64/${lib}")
        done
        for d in "${EC_SYSROOT_LIB}" "${A64_SYSROOT_LIB}"; do
            for lib in libc++.a libc++abi.a libunwind.a libmsvcrt.a libmsvcrt-os.a libmoldname.a libmingwex.a libmingw32.a libucrt.a; do
                [ -f "$d/$lib" ] && args+=("$d/$lib")
            done
        done
        builtins=$(printf '%s\n' "${ec_cmd}" | tr ' ' '\n' | grep -E 'libclang_rt\.builtins-aarch64\.a$' | head -1)
        [ -n "${builtins}" ] && args+=("${builtins}")
        args+=("-def:${deffile}" "-defarm64native:${deffile}")
        for lib in $(printf '%s\n' "${ec_cmd}" | tr ' ' '\n' | grep -E '^-l' | sed 's/^-l//'); do
            [ -f "${EC_SYSROOT_LIB}/lib${lib}.a" ] && args+=("${EC_SYSROOT_LIB}/lib${lib}.a")
            [ -f "${A64_SYSROOT_LIB}/lib${lib}.a" ] && args+=("${A64_SYSROOT_LIB}/lib${lib}.a")
            [ "$lib" = "dxgi" ] && args+=("${DXVK_LEGACY_BUILD_DIR}/buildec/src/dxgi/dxgi.dll.a")
        done

        # DXVK 1.10.3 links vulkan-1.lib and d3dcompiler_43.lib via full path
        # (not -l flag), so merge function doesn't capture them. Add explicitly.
        args+=("${EC_IMPLIB_DIR}/vulkan-1-ec.lib" "${EC_IMPLIB_DIR}/vulkan-1-a64.lib"
               "${EC_IMPLIB_DIR}/d3dcompiler_43-ec.lib" "${EC_IMPLIB_DIR}/d3dcompiler_43-a64.lib")

        echo "  lld-link -machine:arm64x -> ${out} ($(echo ${args[@]} | wc -w) args)"
        "${args[@]}" || { echo "ERROR: lld-link failed for ${target}"; return 1; }
    }

    echo "--- Merging DXVK legacy arm64ec + aarch64 -> ARM64X (0xA64E) ---"
    mkdir -p ${DXVK_LEGACY_BUILD_DIR}/installec/bin
    merge_arm64x_dll src/d3d11/d3d11.dll  ${DXVK_LEGACY_DIR}/src/d3d11/d3d11.def  ${DXVK_LEGACY_BUILD_DIR}/installec/bin/d3d11.dll
    merge_arm64x_dll src/dxgi/dxgi.dll    ${DXVK_LEGACY_DIR}/src/dxgi/dxgi.def    ${DXVK_LEGACY_BUILD_DIR}/installec/bin/dxgi.dll
    merge_arm64x_dll src/d3d9/d3d9.dll    ${DXVK_LEGACY_DIR}/src/d3d9/d3d9.def    ${DXVK_LEGACY_BUILD_DIR}/installec/bin/d3d9.dll
    merge_arm64x_dll src/d3d10/d3d10core.dll ${DXVK_LEGACY_DIR}/src/d3d10/d3d10core.def ${DXVK_LEGACY_BUILD_DIR}/installec/bin/d3d10core.dll

    for dll in d3d11.dll dxgi.dll d3d9.dll d3d10core.dll d3d8.dll; do
        f="${DXVK_LEGACY_BUILD_DIR}/installec/bin/${dll}"
        [ -f "${f}" ] && ${PE_STRIP} -x "${f}" 2>/dev/null || true
    done

    # Verify ARM64X machine type
    echo "--- Verifying DXVK legacy ARM64X artifacts ---"
    for dll in d3d11.dll dxgi.dll; do
        f="${DXVK_LEGACY_BUILD_DIR}/installec/bin/${dll}"
        if [ ! -f "${f}" ]; then
            echo "MISSING: ${f}"
            exit 1
        fi
        if ! ${PE_READOBJ} --file-headers "${f}" | grep -q "ARM64X"; then
            echo "ERROR: ${f} is not ARM64X machine type"
            ${PE_READOBJ} --file-headers "${f}" | grep -i "machine"
            exit 1
        fi
        echo "OK: ${f}"
    done
else
    echo "SKIP: DXVK legacy arm64ec build requires --build-arm64ec"
fi

# ============================================================
# 2. Build 32-bit DXVK legacy (i386, 系统 mingw-w64 工具链)
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

cat > ${DXVK_LEGACY_BUILD_DIR}/build-ohos32.txt << 'EOF'
[binaries]
c = 'i686-w64-mingw32-gcc'
cpp = 'i686-w64-mingw32-g++'
ar = 'i686-w64-mingw32-ar'
strip = 'i686-w64-mingw32-strip'
windres = 'i686-w64-mingw32-windres'

[properties]
needs_exe_wrapper = true
c_args = ['-D__OHOS__=1']
cpp_args = ['-D__OHOS__=1']

[host_machine]
system = 'windows'
cpu_family = 'x86'
cpu = 'x86'
endian = 'little'
EOF

echo "--- Building DXVK legacy x86 ---"
if [ ! -f "${DXVK_LEGACY_BUILD_DIR}/build32/build.ninja" ]; then
    PATH="${GLSLANG_DIR}:${PATH}" meson setup ${DXVK_LEGACY_BUILD_DIR}/build32 ${DXVK_LEGACY_DIR} \
        --cross-file ${DXVK_LEGACY_BUILD_DIR}/build-ohos32.txt \
        --prefix ${DXVK_LEGACY_BUILD_DIR}/install32 \
        -Dbuildtype=release
fi

# Use limited parallelism to avoid OOM
PATH="${GLSLANG_DIR}:${PATH}" ninja -C ${DXVK_LEGACY_BUILD_DIR}/build32 -j4 install

# Verify artifacts
echo "--- Verifying DXVK legacy artifacts ---"
for dll in d3d11.dll dxgi.dll; do
    if [ -f "${DXVK_LEGACY_BUILD_DIR}/install32/bin/${dll}" ]; then
        echo "OK: ${DXVK_LEGACY_BUILD_DIR}/install32/bin/${dll}"
    else
        echo "MISSING: ${DXVK_LEGACY_BUILD_DIR}/install32/bin/${dll}"
        exit 1
    fi
done

echo "DXVK legacy build completed successfully!"

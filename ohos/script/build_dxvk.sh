#!/bin/bash

# Build DXVK 2.6.2 for OHOS (cross-compile to Windows PE DLLs)
#
# 64-bit: ARM64X (0xA64E, hybrid ARM64 + arm64ec). ARM64X 让 ARM64 进程
#         (如 wine_d3d11_test.exe, 0xAA64) 能加载 d3d11.dll/dxgi.dll
#         (ntdll/loader.c is_valid_binary: ARM64X 的 info->Machine=ARM64,
#         在第 2493 行 current_machine 命中放行; 纯 ARM64EC 0xA641 在 ARM64
#         进程被拒)。机制: 编两套 obj (arm64ec + aarch64), lld-link
#         -machine:arm64x 合并; libmingwex 提供 __chpe_metadata + dispatch +
#         _load_config_used, 参考 Wine builtin (winebuild -marm64x)。
#         工具链: 环境变量 LLVM_MINGW (clang + arm64ec/aarch64 sysroot)
# 32-bit: i386, 用系统 mingw-w64 工具链 (wow64 下 32 位游戏走 xtajit)
#
# DXVK 2.6.2 与 1.10.3 的差异 (影响本脚本):
#   - meson.build 用 check_header('vulkan/vulkan.h') 而非
#     find_library('vulkan-1'), 不再需要预生成 vulkan-1.lib
#   - 不再引用 d3dcompiler_43 (无 lib/libd3dcompiler_43.def 依赖)
#   - 不再有 dxvk_library_path / lib-arm64ec 分支
#   故本脚本无需生成任何 arm64ec 导入库, 只需 windres shim /
#   cross-file / ld.lld wrapper (过滤 meson 的 --allow-shlib-undefined)。

set -ex
# DXVK_DIR / DXVK_BUILD_DIR 由 env.sh 派生
#   (${OHOS_BUILD_ROOT}/third_party/dxvk, ${OHOS_BUILD_ROOT}/build/dxvk)

# DXVK (2.6.2) 的 meson.build 硬依赖 glslangValidator 编译着色器
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
# 1. Build 64-bit DXVK (arm64ec + aarch64 -> ARM64X hybrid)
# ============================================================
# ARM64X = hybrid PE (0xA64E) 含 ARM64 (0xAA64) 主 + arm64ec (0xA641)。
# 需两套 obj: arm64ec (buildec) + aarch64 (buildarm64), 然后 lld-link
# -machine:arm64x 合并。libmingwex 提供 __chpe_metadata/dispatch/_load_config。
# 参考: Wine builtin d3d11.dll 也是 ARM64X (tools/winebuild/main.c:382
#   -marm64x -> native_arch=CPU_ARM64; winegcc 1384 对 arm64x 编两套 spec.o;
#   Makefile 链接命令 -b arm64ec-windows -marm64x + aarch64/arm64ec 两套 .o)。
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
c = '${PE_CLANG}'
cpp = '${PE_CXX}'
ar = '${PE_AR}'
strip = '${PE_STRIP}'
windres = '${DXVK_BUILD_DIR}/windres-ec'

[properties]
needs_exe_wrapper = true
sys_root = '${EC_SYSROOT}'
c_args = ['--target=arm64ec-w64-mingw32', '--sysroot=${EC_SYSROOT}',
          '-rtlib=compiler-rt', '-D__OHOS__=1'${EC_RESOURCE_ELEM}]
cpp_args = ['--target=arm64ec-w64-mingw32', '--sysroot=${EC_SYSROOT}',
            '-rtlib=compiler-rt', '-stdlib=libc++', '-include', 'algorithm', '-D__OHOS__=1'${EC_RESOURCE_ELEM}]
c_link_args = ['--target=arm64ec-w64-mingw32', '--sysroot=${EC_SYSROOT}',
               '-rtlib=compiler-rt', '-unwindlib=libunwind', '-fuse-ld=lld', '--ld-path=${DXVK_BUILD_DIR}/ld.lld'${EC_RESOURCE_ELEM},
               '-L${EC_SYSROOT}/lib'${EC_BUILTINS_ELEM}]
cpp_link_args = ['--target=arm64ec-w64-mingw32', '--sysroot=${EC_SYSROOT}',
                 '-rtlib=compiler-rt', '-stdlib=libc++', '-unwindlib=libunwind',
                 '-fuse-ld=lld', '--ld-path=${DXVK_BUILD_DIR}/ld.lld'${EC_RESOURCE_ELEM},
                 '-L${EC_SYSROOT}/lib'${EC_BUILTINS_ELEM}]

[host_machine]
system = 'windows'
cpu_family = 'aarch64'
cpu = 'arm64ec'
endian = 'little'
EOF

    cat > ${DXVK_BUILD_DIR}/ld.lld << EOF
#!/bin/bash
# 过滤 meson find_library link test 加的 --allow-shlib-undefined (lld 不认)
args=()
for a in "\$@"; do
  [ "\$a" = "--allow-shlib-undefined" ] && continue
  args+=("\$a")
done
exec ${LLVM_MINGW}/bin/ld.lld "\${args[@]}"
EOF
    chmod +x ${DXVK_BUILD_DIR}/ld.lld
    echo "--- Building DXVK arm64ec ---"
    if [ ! -f "${DXVK_BUILD_DIR}/buildec/build.ninja" ]; then
        PATH="${DXVK_BUILD_DIR}:${GLSLANG_DIR}:${PATH}" meson setup ${DXVK_BUILD_DIR}/buildec ${DXVK_DIR} \
            --cross-file ${DXVK_BUILD_DIR}/build-arm64ec.txt \
            --prefix ${DXVK_BUILD_DIR}/installec \
            -Dbuildtype=release -Db_lundef=false
    fi

    # Use limited parallelism to avoid OOM
    # ninja 编译着色器时也要调 glslangValidator, 同样命令级注入
    PATH="${DXVK_BUILD_DIR}:${GLSLANG_DIR}:${PATH}" ninja -C ${DXVK_BUILD_DIR}/buildec -j4 install

    # ---- aarch64 (native ARM64) 第二套 obj, 供 ARM64X 合并 ----
    # arm64ec-w64-mingw32 与 aarch64-w64-mingw32 共享同一 llvm-mingw 发行包。
    AARCH64_SYSROOT="${LLVM_MINGW}/aarch64-w64-mingw32"
    [ -d "${AARCH64_SYSROOT}" ] || { echo "ERROR: aarch64 sysroot missing: ${AARCH64_SYSROOT}"; exit 1; }

    # windres for aarch64 (固定 target)
    if [ -f "${LLVM_MINGW}/bin/llvm-windres" ]; then
        A64_WINDRES_BASE="${LLVM_MINGW}/bin/llvm-windres"
    else
        A64_WINDRES_BASE="${AARCH64_SYSROOT}/../bin/aarch64-w64-mingw32-windres"
    fi
    [ -f "${A64_WINDRES_BASE}" ] || { echo "ERROR: llvm-windres (aarch64) not found"; exit 1; }
    cat > ${DXVK_BUILD_DIR}/windres-a64 << EOF
#!/bin/sh
exec ${A64_WINDRES_BASE} --target=aarch64-w64-mingw32 -I${AARCH64_SYSROOT}/include \$@
EOF
    chmod +x ${DXVK_BUILD_DIR}/windres-a64

    # ld.lld wrapper for aarch64 (同样过滤 --allow-shlib-undefined)
    cat > ${DXVK_BUILD_DIR}/ld.lld-a64 << EOF
#!/bin/bash
args=()
for a in "\$@"; do
  [ "\$a" = "--allow-shlib-undefined" ] && continue
  args+=("\$a")
done
exec ${LLVM_MINGW}/bin/ld.lld "\${args[@]}"
EOF
    chmod +x ${DXVK_BUILD_DIR}/ld.lld-a64

    # 注意: aarch64 cross-file 复用 EC_RESOURCE_DIR/EC_BUILTINS_LIB
    # (compiler-rt builtins-aarch64.a 对 arm64ec 与 aarch64 通用)。
    A64_RESOURCE_ELEM=""
    if [ -n "${EC_RESOURCE_DIR}" ]; then
        A64_RESOURCE_ELEM=", '-resource-dir=${EC_RESOURCE_DIR}'"
    fi
    A64_BUILTINS_ELEM=""
    if [ -n "${EC_BUILTINS_LIB}" ]; then
        A64_BUILTINS_ELEM=", '${EC_BUILTINS_LIB}'"
    fi
    cat > ${DXVK_BUILD_DIR}/build-aarch64.txt << EOF
[binaries]
c = '${PE_CLANG}'
cpp = '${PE_CXX}'
ar = '${PE_AR}'
strip = '${PE_STRIP}'
windres = '${DXVK_BUILD_DIR}/windres-a64'

[properties]
needs_exe_wrapper = true
sys_root = '${AARCH64_SYSROOT}'
c_args = ['--target=aarch64-w64-mingw32', '--sysroot=${AARCH64_SYSROOT}',
          '-rtlib=compiler-rt', '-D__OHOS__=1'${A64_RESOURCE_ELEM}]
cpp_args = ['--target=aarch64-w64-mingw32', '--sysroot=${AARCH64_SYSROOT}',
            '-rtlib=compiler-rt', '-stdlib=libc++', '-include', 'algorithm', '-D__OHOS__=1'${A64_RESOURCE_ELEM}]
c_link_args = ['--target=aarch64-w64-mingw32', '--sysroot=${AARCH64_SYSROOT}',
               '-rtlib=compiler-rt', '-unwindlib=libunwind', '-fuse-ld=lld', '--ld-path=${DXVK_BUILD_DIR}/ld.lld-a64'${A64_RESOURCE_ELEM},
               '-L${AARCH64_SYSROOT}/lib'${A64_BUILTINS_ELEM}]
cpp_link_args = ['--target=aarch64-w64-mingw32', '--sysroot=${AARCH64_SYSROOT}',
                 '-rtlib=compiler-rt', '-stdlib=libc++', '-unwindlib=libunwind',
                 '-fuse-ld=lld', '--ld-path=${DXVK_BUILD_DIR}/ld.lld-a64'${A64_RESOURCE_ELEM},
                 '-L${AARCH64_SYSROOT}/lib'${A64_BUILTINS_ELEM}]

[host_machine]
system = 'windows'
cpu_family = 'aarch64'
cpu = 'aarch64'
endian = 'little'
EOF

    echo "--- Building DXVK aarch64 (native ARM64 obj for ARM64X) ---"
    if [ ! -f "${DXVK_BUILD_DIR}/buildarm64/build.ninja" ]; then
        PATH="${DXVK_BUILD_DIR}:${GLSLANG_DIR}:${PATH}" meson setup ${DXVK_BUILD_DIR}/buildarm64 ${DXVK_DIR} \
            --cross-file ${DXVK_BUILD_DIR}/build-aarch64.txt \
            --prefix ${DXVK_BUILD_DIR}/installarm64 \
            -Dbuildtype=release -Db_lundef=false
    fi
    PATH="${DXVK_BUILD_DIR}:${GLSLANG_DIR}:${PATH}" ninja -C ${DXVK_BUILD_DIR}/buildarm64 -j4 install

    # ---- ARM64X 合并: lld-link -machine:arm64x ----
    # 合并 buildec (arm64ec) + buildarm64 (aarch64) 的 obj 为单个 0xA64E PE。
    # libmingwex (两套) 提供 __chpe_metadata/__os_arm64x_dispatch_*/_load_config_used。
    # 唯一需补的: 空 __CTOR_LIST__/__DTOR_LIST__ (gccmain.o 用; clang 走 _initterm
    # 路径, ctor 列表为空, 但 lld-link MSVC 模式不像 ld.lld mingw 自动合成它)。
    cat > ${DXVK_BUILD_DIR}/arm64x_ctors_stub.c << 'EOF'
/* Empty __CTOR_LIST__/__DTOR_LIST__ for ARM64X link via lld-link (MSVC mode).
 * clang/mingw registers C++ ctors via .CRT$XCU + _initterm (dllcrt2.o
 * DllMainCRTStartup), not via __CTOR_LIST__; gccmain.o's __do_global_ctors
 * still iterates the list, so an empty (-1, 0) list satisfies it. */
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
    ${PE_CLANG} --target=arm64ec-w64-mingw32 -c -o ${DXVK_BUILD_DIR}/ctors_stub_ec.o ${DXVK_BUILD_DIR}/arm64x_ctors_stub.c
    ${PE_CLANG} --target=aarch64-w64-mingw32 -c -o ${DXVK_BUILD_DIR}/ctors_stub_native.o ${DXVK_BUILD_DIR}/arm64x_ctors_stub.c

    EC_SYSROOT_LIB="${EC_SYSROOT}/lib"
    A64_SYSROOT_LIB="${AARCH64_SYSROOT}/lib"

    # merge_arm64x_dll <ninja_target> <def_file> <out_dll>
    # 用 ninja -t commands 解析 buildec 链接命令, 抽取 obj/静态库/-l 系统库,
    # 对每个 EC obj 补 buildarm64 同路径 native obj (version.o 资源 obj 跳过,
    # lld-link 只允许一个资源 obj)。
    merge_arm64x_dll() {
        local target="$1" deffile="$2" out="$3"
        local ec_cmd ec_obj static_lib sys_lib builtins
        ec_cmd=$(cd ${DXVK_BUILD_DIR}/buildec && ninja -t commands "${target}" 2>/dev/null | tail -1)
        [ -z "${ec_cmd}" ] && { echo "ERROR: cannot get ninja command for ${target}"; return 1; }

        local args=("${PE_LLDLINK}" -machine:arm64x -dll -out:"${out}" -subsystem:windows
                    -entry:DllMainCRTStartup
                    -libpath:"${EC_SYSROOT_LIB}" -libpath:"${A64_SYSROOT_LIB}")

        # EC obj
        for o in $(printf '%s\n' "${ec_cmd}" | tr ' ' '\n' | grep -E '\.(o|obj)$'); do
            args+=("${DXVK_BUILD_DIR}/buildec/${o}")
        done
        # aarch64 native obj (skip windres resource obj — lld-link 只允许一个
        # 资源 obj, 且资源是 arch-agnostic; 资源 obj 名为 version*.o /
        # version10_core.o 等, 统一按 *version*.o 跳过)
        for o in $(printf '%s\n' "${ec_cmd}" | tr ' ' '\n' | grep -E '\.(o|obj)$'); do
            case "$o" in *version*.o) continue;; esac
            args+=("${DXVK_BUILD_DIR}/buildarm64/${o}")
        done
        # 空 ctor list stub (两套)
        args+=("${DXVK_BUILD_DIR}/ctors_stub_ec.o" "${DXVK_BUILD_DIR}/ctors_stub_native.o")
        # CRT startup obj (两套: dllcrt2 提供 DllMainCRTStartup)
        for d in "${EC_SYSROOT_LIB}" "${A64_SYSROOT_LIB}"; do
            [ -f "$d/dllcrt2.o" ] && args+=("$d/dllcrt2.o")
            [ -f "$d/crtbegin.o" ] && args+=("$d/crtbegin.o")
            [ -f "$d/crtend.o" ] && args+=("$d/crtend.o")
        done
        # DXVK 内部静态库 (两套)
        for lib in $(printf '%s\n' "${ec_cmd}" | tr ' ' '\n' | grep -E '(^src/|^subprojects/).*\.a$'); do
            args+=("${DXVK_BUILD_DIR}/buildec/${lib}" "${DXVK_BUILD_DIR}/buildarm64/${lib}")
        done
        # C/C++ runtime + mingw CRT (两套; libmingwex 提供 chpe/dispatch/loadcfg)
        for d in "${EC_SYSROOT_LIB}" "${A64_SYSROOT_LIB}"; do
            for lib in libc++.a libc++abi.a libunwind.a libmsvcrt.a libmsvcrt-os.a libmoldname.a libmingwex.a libmingw32.a libucrt.a; do
                [ -f "$d/$lib" ] && args+=("$d/$lib")
            done
        done
        # compiler-rt builtins (aarch64, EC 与 native 通用)
        builtins=$(printf '%s\n' "${ec_cmd}" | tr ' ' '\n' | grep -E 'libclang_rt\.builtins-aarch64\.a$' | head -1)
        [ -n "${builtins}" ] && args+=("${builtins}")
        # def (导出表): 必须同时给 EC 视图与 ARM64 原生视图各一份。
        # ARM64X PE 含两份可选头: 主(ARM64X) 头是 ARM64 原生视图, 混合(ARM64EC)
        # 头是 EC 视图。lld-link -machine:arm64x + 单 -def 只把导出表写进
        # hybrid(EC) 头 (RVA 指向 .hexpthk 的 EC thunk), 主头的 ExportTableRVA
        # 留 0。ARM64 原生进程 (如 wine_d3d11_test.exe, 0xAA64) 加载此 DLL 时,
        # ntdll RtlImageDirectoryEntryToData 读主头 → 无导出表 → 解析失败 → 回退
        # Wine builtin stub → "unimplemented function d3d11.dll.D3D11Create..."。
        # -defarm64native 让 lld-link 镜像同一份 def 到主头的导出表, RVA 指向
        # buildarm64 *.obj 的真实实现 (非 .hexpthk thunk / forwarder)。
        args+=("-def:${deffile}" "-defarm64native:${deffile}")
        # 系统导入库 (-lNAME, 两套按全路径; d3d11 依赖 dxgi 用 EC 自建 dxgi.dll.a)
        for lib in $(printf '%s\n' "${ec_cmd}" | tr ' ' '\n' | grep -E '^-l' | sed 's/^-l//'); do
            [ -f "${EC_SYSROOT_LIB}/lib${lib}.a" ] && args+=("${EC_SYSROOT_LIB}/lib${lib}.a")
            [ -f "${A64_SYSROOT_LIB}/lib${lib}.a" ] && args+=("${A64_SYSROOT_LIB}/lib${lib}.a")
            [ "$lib" = "dxgi" ] && args+=("${DXVK_BUILD_DIR}/buildec/src/dxgi/dxgi.dll.a")
        done

        echo "  lld-link -machine:arm64x -> ${out} ($(echo ${args[@]} | wc -w) args)"
        "${args[@]}" || { echo "ERROR: lld-link failed for ${target}"; return 1; }
    }

    echo "--- Merging DXVK arm64ec + aarch64 -> ARM64X (0xA64E) ---"
    mkdir -p ${DXVK_BUILD_DIR}/installec/bin
    # (ninja target, def 相对 DXVK_DIR, 输出 dll) 列表
    merge_arm64x_dll src/d3d11/d3d11.dll  ${DXVK_DIR}/src/d3d11/d3d11.def  ${DXVK_BUILD_DIR}/installec/bin/d3d11.dll
    merge_arm64x_dll src/dxgi/dxgi.dll    ${DXVK_DIR}/src/dxgi/dxgi.def    ${DXVK_BUILD_DIR}/installec/bin/dxgi.dll
    merge_arm64x_dll src/d3d9/d3d9.dll    ${DXVK_DIR}/src/d3d9/d3d9.def    ${DXVK_BUILD_DIR}/installec/bin/d3d9.dll
    merge_arm64x_dll src/d3d10/d3d10core.dll ${DXVK_DIR}/src/d3d10/d3d10core.def ${DXVK_BUILD_DIR}/installec/bin/d3d10core.dll
    merge_arm64x_dll src/d3d8/d3d8.dll    ${DXVK_DIR}/src/d3d8/d3d8.def    ${DXVK_BUILD_DIR}/installec/bin/d3d8.dll

    # strip ARM64X 产物 (保持与原 arm64ec install 一致)
    for dll in d3d11.dll dxgi.dll d3d9.dll d3d10core.dll d3d8.dll; do
        f="${DXVK_BUILD_DIR}/installec/bin/${dll}"
        [ -f "${f}" ] && ${PE_STRIP} -x "${f}" 2>/dev/null || true
    done

    # Verify artifacts: machine type 必须是 ARM64X (0xA64E), 这样 ARM64 进程
    # (wine_d3d11_test.exe) 经 is_valid_binary 第 2493 行才能放行加载。
    echo "--- Verifying DXVK ARM64X artifacts ---"
    for dll in d3d11.dll dxgi.dll; do
        f="${DXVK_BUILD_DIR}/installec/bin/${dll}"
        if [ ! -f "${f}" ]; then
            echo "MISSING: ${f}"
            exit 1
        fi
        if ! ${PE_READOBJ} --file-headers "${f}" | grep -q "IMAGE_FILE_MACHINE_ARM64X (0xA64E)"; then
            echo "ERROR: ${f} is not ARM64X (0xA64E)"
            ${PE_READOBJ} --file-headers "${f}" | grep -i "machine"
            exit 1
        fi
        echo "OK: ${f}"
    done
else
    echo "SKIP: DXVK ARM64X build requires --build-arm64ec"
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

#!/bin/bash
# ohos/script/pe_common.sh — DXVK / vkd3d-proton PE 交叉编译共用工具集
#
# 被 build_dxvk.sh / build_dxvk_legacy.sh / build_vkd3d_proton.sh source
# (它们又被 build.sh source), 因此本文件只定义函数, 不修改 PATH /
# 不 export 环境变量。教训: 曾把 glslang 所在目录 (系统里是 /usr/bin)
# 前置 export 到 PATH, 导致后续 wine 构建阶段 winebuild 命中系统的
# /usr/bin/llvm-dlltool (LLVM 18, 无 arm64ec) 而失败。所有 PATH 变更
# 一律命令级注入 (见 pe_meson_build)。
#
# 依赖 env.sh 派生的变量: LLVM_MINGW, PE_CLANG/PE_CXX/PE_LLDLINK/
# PE_AR/PE_STRIP/PE_READOBJ, EC_SYSROOT/EC_RESOURCE_DIR/EC_BUILTINS_LIB。

# 本文件所在目录 (调用方与其同目录, 可复用作资源寻址)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# ------------------------------------------------------------
# glslangValidator 发现 (DXVK / vkd3d-proton 的 meson 着色器编译硬依赖)
# 查找顺序: PATH > VULKAN_SDK/bin > ~/1.3.290.0 (Vulkan SDK 默认装路径)
# 成功时设置 GLSLANG_BIN / GLSLANG_DIR, 失败时打印指引并返回 1。
# ------------------------------------------------------------
pe_find_glslang() {
    GLSLANG_BIN="$(command -v glslangValidator || true)"
    if [ -z "${GLSLANG_BIN}" ] && [ -n "${VULKAN_SDK:-}" ] \
       && [ -x "${VULKAN_SDK}/bin/glslangValidator" ]; then
        GLSLANG_BIN="${VULKAN_SDK}/bin/glslangValidator"
    fi
    if [ -z "${GLSLANG_BIN}" ] && [ -x "${HOME}/1.3.290.0/x86_64/bin/glslangValidator" ]; then
        GLSLANG_BIN="${HOME}/1.3.290.0/x86_64/bin/glslangValidator"
    fi
    if [ -z "${GLSLANG_BIN}" ]; then
        echo "ERROR: glslangValidator not found."
        echo "  方式1: sudo apt install glslang-tools"
        echo "  方式2: export VULKAN_SDK=<SDK路径>/x86_64 (source <SDK>/setup-env.sh)"
        return 1
    fi
    GLSLANG_DIR="$(dirname "${GLSLANG_BIN}")"
    echo "Using glslangValidator: ${GLSLANG_BIN}"
}

# ------------------------------------------------------------
# arm64ec 环境校验 (依赖 env.sh 导出的 EC_SYSROOT)
# ------------------------------------------------------------
pe_require_ec_sysroot() {
    if [ -z "${EC_SYSROOT}" ]; then
        echo "ERROR: EC_SYSROOT not set, source ohos/script/env.sh first"
        echo "  (须先 export LLVM_MINGW=/path/to/llvm-mingw-<tag>-ucrt-...-x86_64)"
        return 1
    fi
}

# ------------------------------------------------------------
# i386 工具链校验 (32 位 DLL 用 llvm-mingw 自带的 i686 工具链)
# ------------------------------------------------------------
pe_require_i386_sysroot() {
    local hint="export LLVM_MINGW=/path/to/llvm-mingw-<tag>-ucrt-ubuntu-22.04-x86_64"
    if [ -z "${LLVM_MINGW:-}" ] || [ ! -x "${LLVM_MINGW}/bin/clang" ]; then
        echo "ERROR: llvm-mingw not found. ${hint}"
        return 1
    fi
    if [ ! -d "${LLVM_MINGW}/i686-w64-mingw32" ]; then
        echo "ERROR: i686 sysroot missing: ${LLVM_MINGW}/i686-w64-mingw32. ${hint}"
        return 1
    fi
}

# ------------------------------------------------------------
# windres shim: 优先 LLVM_MINGW 的 llvm-windres, 缺失则回退
# 发行包内带前缀的 windres; 包一层以固定 target。
# -I<sysroot>/include: mingw-w64 头 (version.rc 需要 windows.h)
# 用法: pe_make_windres_shim <输出脚本> <target三元组> <sysroot>
# ------------------------------------------------------------
pe_make_windres_shim() {
    local out="$1" target="$2" sysroot="$3" base
    if [ -f "${LLVM_MINGW}/bin/llvm-windres" ]; then
        base="${LLVM_MINGW}/bin/llvm-windres"
    else
        base="${sysroot}/../bin/${target}-windres"
    fi
    if [ ! -f "${base}" ]; then
        echo "ERROR: windres for ${target} not found (LLVM_MINGW=${LLVM_MINGW})"
        return 1
    fi
    cat > "${out}" << EOF
#!/bin/sh
exec ${base} --target=${target} -I${sysroot}/include \$@
EOF
    chmod +x "${out}"
}

# ------------------------------------------------------------
# ld.lld wrapper: 过滤 meson find_library 链接探测注入的
# --allow-shlib-undefined (lld 的 MinGW PE 前端不认, ELF 专属语义)。
# 用法: pe_make_lld_wrapper <输出脚本>
# ------------------------------------------------------------
pe_make_lld_wrapper() {
    cat > "$1" << EOF
#!/bin/bash
args=()
for a in "\$@"; do
  [ "\$a" = "--allow-shlib-undefined" ] && continue
  args+=("\$a")
done
exec ${LLVM_MINGW}/bin/ld.lld "\${args[@]}"
EOF
    chmod +x "$1"
}

# 把空格分隔的 flag 串转成 meson 列表元素后缀。
# 例: "-include algorithm" -> ", '-include', 'algorithm'"
__pe_meson_elems() {
    local e out="" first=1
    for e in $1; do
        if [ "${first}" -eq 1 ]; then out=", '${e}'"; first=0; else out="${out}, '${e}'"; fi
    done
    printf '%s' "${out}"
}

# ------------------------------------------------------------
# 生成 llvm-mingw 交叉编译 cross-file (arm64ec 或 aarch64)。
# 参数分区铁律: c_args/cpp_args 只放纯编译期参数 (--target/--sysroot/
# -rtlib/-stdlib/附加参数); 链接期参数 (-unwindlib/-fuse-ld=lld/
# --ld-path/-L/builtins.a) 必须放 *_link_args, 否则 meson 的编译探测
# (-c + -Werror=unused-command-line-argument) 会报 argument unused,
# 误杀所有 compiler_args 探测 (连 -DNOMINMAX 都会 NO)。
# -resource-dir 两边都要有: 链接期没有它, clang 会去自己的 resource
# 目录找 arm64ec builtins (不存在) 而链接失败。
#
# 用法: pe_write_mingw_crossfile <输出> <cpu:arm64ec|aarch64> <sysroot> \
#           <windres> <ld.lld wrapper> <公共编译参数> <c独有> <cpp独有> [widl]
# 公共编译参数同时进 c_args 与 cpp_args (如 -D__OHOS__=1 只需写一次);
# c独有/cpp独有 可为空串; widl 省略则不写入 [binaries]。
# ------------------------------------------------------------
pe_write_mingw_crossfile() {
    local out="$1" cpu="$2" sysroot="$3" windres="$4" lldw="$5"
    local common="$6" c_only="$7" cpp_only="$8" widl="${9:-}"
    local target="${cpu}-w64-mingw32"
    local res_elem="" builtins_elem=""

    # resource-dir: 让 clang 的 -rtlib=compiler-rt 解析到 llvm-mingw 的
    # clang resource 目录里的 arm64ec builtins (通用发行包没有);
    # builtins 库再显式点名一次兜底。两者对 aarch64 系 target 通用。
    [ -n "${EC_RESOURCE_DIR}" ] && res_elem=", '-resource-dir=${EC_RESOURCE_DIR}'"
    [ -n "${EC_BUILTINS_LIB}" ] && builtins_elem=", '${EC_BUILTINS_LIB}'"

    common="$(__pe_meson_elems "${common}")"
    c_only="$(__pe_meson_elems "${c_only}")"
    cpp_only="$(__pe_meson_elems "${cpp_only}")"

    {
        echo "[binaries]"
        echo "c = '${PE_CLANG}'"
        echo "cpp = '${PE_CXX}'"
        echo "ar = '${PE_AR}'"
        echo "strip = '${PE_STRIP}'"
        echo "windres = '${windres}'"
        [ -n "${widl}" ] && echo "widl = '${widl}'"
        echo ""
        echo "[properties]"
        echo "needs_exe_wrapper = true"
        echo "sys_root = '${sysroot}'"
        echo "c_args = ['--target=${target}', '--sysroot=${sysroot}',"
        echo "          '-rtlib=compiler-rt'${common}${c_only}${res_elem}]"
        echo "cpp_args = ['--target=${target}', '--sysroot=${sysroot}',"
        echo "            '-rtlib=compiler-rt', '-stdlib=libc++'${common}${cpp_only}${res_elem}]"
        echo "c_link_args = ['--target=${target}', '--sysroot=${sysroot}',"
        echo "               '-rtlib=compiler-rt', '-unwindlib=libunwind', '-fuse-ld=lld',"
        echo "               '--ld-path=${lldw}'${res_elem},"
        echo "               '-L${sysroot}/lib'${builtins_elem}]"
        echo "cpp_link_args = ['--target=${target}', '--sysroot=${sysroot}',"
        echo "                 '-rtlib=compiler-rt', '-stdlib=libc++', '-unwindlib=libunwind',"
        echo "                 '-fuse-ld=lld', '--ld-path=${lldw}'${res_elem},"
        echo "                 '-L${sysroot}/lib'${builtins_elem}]"
        echo ""
        echo "[host_machine]"
        echo "system = 'windows'"
        echo "cpu_family = 'aarch64'"
        echo "cpu = '${cpu}'"
        echo "endian = 'little'"
    } > "${out}"
}

# ------------------------------------------------------------
# 生成 i386 cross-file (llvm-mingw 工具链)
# 使用 LLVM_MINGW 自带的 i686 sysroot/clang/lld, 不依赖系统 mingw-w64;
# windres shim 与 ld.lld wrapper 生成在 cross-file 同目录下。
# 参数分区铁律同 pe_write_mingw_crossfile (编译期参数进 *_args,
# 链接期参数进 *_link_args)。
# 用法: pe_write_i386_crossfile <输出> <公共编译参数> <c独有> <cpp独有>
#   公共编译参数同时进 c_args 与 cpp_args (如 "-D__OHOS__=1");
#   c独有/cpp独有 可为空串 (DXVK 2.6 配 libc++ 时 cpp 需 "-include algorithm")。
# ------------------------------------------------------------
pe_write_i386_crossfile() {
    local out="$1" common="$2" c_only="$3" cpp_only="$4"
    local sysroot="${LLVM_MINGW}/i686-w64-mingw32"
    local dir="$(dirname "${out}")"
    local windres="${dir}/windres-i386" lldw="${dir}/ld.lld-i386"

    pe_make_windres_shim "${windres}" i686-w64-mingw32 "${sysroot}"
    pe_make_lld_wrapper "${lldw}"

    common="$(__pe_meson_elems "${common}")"
    c_only="$(__pe_meson_elems "${c_only}")"
    cpp_only="$(__pe_meson_elems "${cpp_only}")"

    {
        echo "[binaries]"
        echo "c = '${PE_CLANG}'"
        echo "cpp = '${PE_CXX}'"
        echo "ar = '${PE_AR}'"
        echo "strip = '${PE_STRIP}'"
        echo "windres = '${windres}'"
        echo ""
        echo "[properties]"
        echo "needs_exe_wrapper = true"
        echo "sys_root = '${sysroot}'"
        echo "c_args = ['--target=i686-w64-mingw32', '--sysroot=${sysroot}',"
        echo "          '-rtlib=compiler-rt'${common}${c_only}]"
        echo "cpp_args = ['--target=i686-w64-mingw32', '--sysroot=${sysroot}',"
        echo "            '-rtlib=compiler-rt', '-stdlib=libc++'${common}${cpp_only}]"
        echo "c_link_args = ['--target=i686-w64-mingw32', '--sysroot=${sysroot}',"
        echo "               '-rtlib=compiler-rt', '-fuse-ld=lld',"
        echo "               '--ld-path=${lldw}', '-L${sysroot}/lib']"
        echo "cpp_link_args = ['--target=i686-w64-mingw32', '--sysroot=${sysroot}',"
        echo "                 '-rtlib=compiler-rt', '-stdlib=libc++', '-fuse-ld=lld',"
        echo "                 '--ld-path=${lldw}', '-L${sysroot}/lib']"
        echo ""
        echo "[host_machine]"
        echo "system = 'windows'"
        echo "cpu_family = 'x86'"
        echo "cpu = 'x86'"
        echo "endian = 'little'"
    } > "${out}"
}

# ------------------------------------------------------------
# meson setup (一次性) + ninja install。
# PATH 命令级注入 <path_prefix> (wrapper 目录/glslang 目录/widl 目录),
# 零泄漏到外层 shell。 -j4 限制并行度避免 OOM。
# meson setup 时会快照 cross-file 内容, 后续 ninja/regenerate 不会重读
# 变更后的 cross-file — 故记录其 sha256, 变更即强制整目录重配重编,
# 保证编译脚本改动的编译参数 (如 -I 注入路径) 必定生效。
# 用法: pe_meson_build <build目录> <源码目录> <cross-file> <prefix> \
#         <PATH前缀> <b_lundef: 0|1>
# ------------------------------------------------------------
pe_meson_build() {
    local bdir="$1" src="$2" cross="$3" prefix="$4" ppath="$5" lundef="$6"
    local undef_flag=""
    [ "${lundef}" -eq 1 ] && undef_flag="-Db_lundef=false"
    local stamp="${bdir}/.ohos-cross.sha256"
    local want_sha=""
    [ -f "${cross}" ] && want_sha="$(sha256sum "${cross}" | cut -d' ' -f1)"
    if [ ! -f "${bdir}/build.ninja" ] || [ ! -f "${stamp}" ] || \
       [ "$(cat "${stamp}" 2>/dev/null)" != "${want_sha}" ]; then
        rm -rf "${bdir}"
        PATH="${ppath}:${PATH}" meson setup "${bdir}" "${src}" \
            --cross-file "${cross}" \
            --prefix "${prefix}" \
            -Dbuildtype=release ${undef_flag}
        printf '%s\n' "${want_sha}" > "${stamp}"
    fi
    PATH="${ppath}:${PATH}" ninja -C "${bdir}" -j4 install
}

# ------------------------------------------------------------
# 校验 PE 产物 machine type, 防止 target 配错时静默编成别的架构。
# <pattern> 传 llvm-readobj 输出的匹配串, 如 "ARM64X (0xA64E)"。
# 用法: pe_verify_machine <文件> <pattern>
# ------------------------------------------------------------
pe_verify_machine() {
    local f="$1" pattern="$2" headers
    if [ ! -f "${f}" ]; then
        echo "MISSING: ${f}"
        return 1
    fi
    if ! headers="$(${PE_READOBJ} --file-headers "${f}")"; then
        echo "ERROR: invalid PE structure: ${f}"
        return 1
    fi
    if ! grep -q "${pattern}" <<< "${headers}"; then
        echo "ERROR: ${f} is not ${pattern} machine type"
        grep -i "machine" <<< "${headers}"
        return 1
    fi
    echo "OK: ${f}"
}

# ------------------------------------------------------------
# 编译空 __CTOR_LIST__/__DTOR_LIST__ stub (arm64ec + aarch64 各一份)。
# gccmain.o 的 __do_global_ctors 迭代该列表; clang/mingw 经 .CRT$XCU +
# _initterm (dllcrt2.o DllMainCRTStartup) 注册构造, 列表为空 (-1, 0)
# 即可满足符号。ARM64X 合并后由 patch_arm64x_ctors.py 重写为只含原生
# flavor 构造函数的有效列表 (详见该脚本头注释)。
# 用法: pe_make_ctors_stubs <build目录>
# ------------------------------------------------------------
pe_make_ctors_stubs() {
    local bdir="$1"
    cat > "${bdir}/arm64x_ctors_stub.c" << 'EOF'
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
    ${PE_CLANG} --target=arm64ec-w64-mingw32 -c -o "${bdir}/ctors_stub_ec.o" \
        "${bdir}/arm64x_ctors_stub.c"
    ${PE_CLANG} --target=aarch64-w64-mingw32 -c -o "${bdir}/ctors_stub_native.o" \
        "${bdir}/arm64x_ctors_stub.c"
}

# ------------------------------------------------------------
# ARM64X 合并: 把 buildec (arm64ec) 与 buildarm64 (aarch64) 两套 obj
# 用 lld-link -machine:arm64x 合并为单一 0xA64E PE (ARM64 原生进程可直接
# 加载; 纯 ARM64EC 0xA641 在 ARM64 进程被拒, 见 ntdll/loader.c
# is_valid_binary)。
# 机制: 用 ninja -t commands 解析 buildec 的链接命令, 抽取 obj/静态库/
# -l 系统库, 对每个 EC obj 补 buildarm64 同路径 native obj。
#   - version*.o 资源 obj 跳过 (lld-link 只允许一个资源 obj, 资源与
#     架构无关)
#   - libmingwex (两套) 提供 __chpe_metadata/__os_arm64x_dispatch_*/
#     _load_config_used
#   - def 双写: -def (EC 视图) + -defarm64native (ARM64 原生视图)。
#     单 -def 只把导出表写进 hybrid(EC) 头, 原生进程加载会找不到导出
#     而回退 Wine builtin stub
#   - d3d11 依赖 dxgi 时用 EC 自建的 dxgi.dll.a
# 用法: pe_merge_arm64x_dll <build目录> <ninja target> <def文件> \
#         <输出dll> [附加导入库...]
# (附加导入库: 全路径链接不进 -l 的库, 如 legacy 的 vulkan-1/d3dcompiler_43)
# ------------------------------------------------------------
pe_merge_arm64x_dll() {
    local bdir="$1" target="$2" deffile="$3" out="$4"
    shift 4
    local ec_cmd builtins o lib d
    ec_cmd=$(cd "${bdir}/buildec" && ninja -t commands "${target}" 2>/dev/null | tail -1)
    [ -z "${ec_cmd}" ] && { echo "ERROR: cannot get ninja command for ${target}"; return 1; }

    local ec_sysroot_lib="${EC_SYSROOT}/lib"
    local a64_sysroot_lib="${LLVM_MINGW}/aarch64-w64-mingw32/lib"
    local args=("${PE_LLDLINK}" -machine:arm64x -dll -out:"${out}" -subsystem:windows
                -entry:DllMainCRTStartup
                -libpath:"${ec_sysroot_lib}" -libpath:"${a64_sysroot_lib}")

    # EC obj
    for o in $(printf '%s\n' "${ec_cmd}" | tr ' ' '\n' | grep -E '\.(o|obj)$'); do
        args+=("${bdir}/buildec/${o}")
    done
    # aarch64 native obj
    for o in $(printf '%s\n' "${ec_cmd}" | tr ' ' '\n' | grep -E '\.(o|obj)$'); do
        case "$o" in *version*.o) continue;; esac
        args+=("${bdir}/buildarm64/${o}")
    done
    # 空 ctor 列表 stub (两套)
    args+=("${bdir}/ctors_stub_ec.o" "${bdir}/ctors_stub_native.o")
    # CRT startup obj (两套: dllcrt2 提供 DllMainCRTStartup)
    for d in "${ec_sysroot_lib}" "${a64_sysroot_lib}"; do
        [ -f "$d/dllcrt2.o" ] && args+=("$d/dllcrt2.o")
        [ -f "$d/crtbegin.o" ] && args+=("$d/crtbegin.o")
        [ -f "$d/crtend.o" ] && args+=("$d/crtend.o")
    done
    # 项目内部静态库 (两套)
    for lib in $(printf '%s\n' "${ec_cmd}" | tr ' ' '\n' \
                | grep -E '(^src/|^subprojects/).*\.a$'); do
        args+=("${bdir}/buildec/${lib}" "${bdir}/buildarm64/${lib}")
    done
    # C/C++ runtime + mingw CRT (两套)
    for d in "${ec_sysroot_lib}" "${a64_sysroot_lib}"; do
        for lib in libc++.a libc++abi.a libunwind.a libmsvcrt.a libmsvcrt-os.a \
                   libmoldname.a libmingwex.a libmingw32.a libucrt.a; do
            [ -f "$d/$lib" ] && args+=("$d/$lib")
        done
    done
    # compiler-rt builtins (aarch64, EC 与 native 通用)
    builtins=$(printf '%s\n' "${ec_cmd}" | tr ' ' '\n' \
              | grep -E 'libclang_rt\.builtins-aarch64\.a$' | head -1)
    [ -n "${builtins}" ] && args+=("${builtins}")
    # def 双视图导出表
    args+=("-def:${deffile}" "-defarm64native:${deffile}")
    # 系统导入库 (-lNAME -> 两套全路径)
    for lib in $(printf '%s\n' "${ec_cmd}" | tr ' ' '\n' | grep -E '^-l' | sed 's/^-l//'); do
        [ -f "${ec_sysroot_lib}/lib${lib}.a" ] && args+=("${ec_sysroot_lib}/lib${lib}.a")
        [ -f "${a64_sysroot_lib}/lib${lib}.a" ] && args+=("${a64_sysroot_lib}/lib${lib}.a")
        [ "$lib" = "dxgi" ] && args+=("${bdir}/buildec/src/dxgi/dxgi.dll.a")
    done
    # 调用方附加导入库 (全路径链接的库)
    args+=("$@")

    echo "  lld-link -machine:arm64x -> ${out} ($(echo ${args[@]} | wc -w) args)"
    "${args[@]}" || { echo "ERROR: lld-link failed for ${target}"; return 1; }
}

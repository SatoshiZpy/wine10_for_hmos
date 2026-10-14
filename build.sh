#!/bin/bash

set -ex

# ============================================================
# 1. 定义路径和版本
# ============================================================

basedir=$(dirname "$0")
ROOT_DIR=$PWD

# --- Wine 源码/产物路径（工具链路径由 ohos/script/env.sh 统一发现，见 §4） ---
BUILD_DIR=${ROOT_DIR}/build
INSTALL_DIR=${ROOT_DIR}/out
ADAPTER_DIR=${ROOT_DIR}/ohos/adapter
APP_DIR=${ROOT_DIR}/ohos/app
UBT_DIR=${ROOT_DIR}/ohos/ubt
TOOLS_DIR=${ROOT_DIR}/build-tools

# ============================================================
# 2. 解析命令行参数
# ============================================================
NO_CACHE=0
BUILD_ARM64EC=1
COMPILE_DXVK=1
COMPILE_HAP=1
DEBUG_MODE=0
DXVK_LEGACY=0
WITH_MONO=0
WITH_GECKO=0
while [[ $# -gt 0 ]]; do
    case $1 in
        --no-cache)
            NO_CACHE=1
            shift
            ;;
        --build-arm64ec)
            BUILD_ARM64EC=1
            shift
            ;;
        --with-dxvk)
            COMPILE_DXVK=1
            shift
            ;;
        --compile-hap)
            COMPILE_HAP=1
            shift
            ;;
        --debug)
            DEBUG_MODE=1
            shift
            ;;
        --dxvk-legacy)
            DXVK_LEGACY=1
            shift
            ;;
        --with-mono)
            WITH_MONO=1
            shift
            ;;
        --with-gecko)
            WITH_GECKO=1
            shift
            ;;
        -h|--help)
            echo "用法: $0 [--no-cache] [--debug]"
            echo ""
            echo "选项 (默认已启用 --build-arm64ec --with-dxvk --compile-hap):"
            echo "  --no-cache        清理所有缓存后重新编译"
            echo "  --build-arm64ec   编译arm64ec/wow64 mode dll (默认启用)"
            echo "  --with-dxvk       编译DXVK子模块 (默认启用, 需配合 --build-arm64ec)"
            echo "  --compile-hap     编译HAP包 (默认启用)"
            echo "  --debug           启用调试模式"
            echo "  --dxvk-legacy     用 DXVK 1.10.3 (dxvk-legacy) 替代默认 2.6.2"
            echo "  --with-mono       打包 wine-mono (默认不打包)"
            echo "  --with-gecko      打包 wine-gecko (默认不打包)"
            echo "  -h, --help        显示此帮助信息"
            exit 0
            ;;
        *)
            echo "未知选项: $1"
            echo "使用 -h 或 --help 查看帮助"
            exit 1
            ;;
    esac
done

clean_all_cache() {
    echo "清理项目缓存..."
    # 注意: -x 会连 .gitignore 的文件一起删。工具链不再入仓库
    # (c2d0191 起由环境变量 OHOS_SDK/LLVM_MINGW
    # 定位, third_party/llvm、third_party/llvm-mingw 均已废弃);
    # ohos_sdk/command-line-tools 若本地仍解压有 SDK, 用 --exclude 保留。
    # tarball 本身是 git 追踪的, clean 不会动它们。
    git clean -xfd
    git submodule foreach --recursive 'git reset --hard HEAD' && git submodule foreach --recursive 'git clean -fdx'
    echo "所有缓存清理完成!"
}

if [ ${NO_CACHE} -eq 1 ]; then
    clean_all_cache
fi

# ============================================================
# 3. 准备 Wine 工具
# ============================================================
cd ${ROOT_DIR}
autoheader && autoconf

if [ ! -d "${TOOLS_DIR}" ]; then
    echo "build x86 wine in ${TOOLS_DIR}"
    mkdir -p ${TOOLS_DIR}
    cd ${TOOLS_DIR}

    ../configure --without-x
    make -j$(nproc)

    echo "build x86 wine done"
fi

# ============================================================
# 4. 准备 OHOS SDK + PE LLVM 工具链（env.sh 统一发现与校验，不再解压 bundle）
# ============================================================
cd ${ROOT_DIR}
source ${ROOT_DIR}/ohos/script/env.sh
# (legacy) 在 OHOS SDK llvm/bin 建 lld-link 软链（ld.lld 别名，兼容旧构建步骤）
[ -e "${OHOS_TOOLCHAIN}/bin/lld-link" ] || ln -sf ld.lld "${OHOS_TOOLCHAIN}/bin/lld-link"

# 防呆: arm64ec 模式下 llvm-dlltool 必须支持 arm64ec 目标。
# winebuild 生成 ARM64X 导入库时会调 llvm-dlltool -m arm64ec, 系统
# /usr/bin/llvm-dlltool (LLVM <= 20) 的 TARGETS 不含 arm64ec, 会报
# unknown target; 提前拦截避免编译两小时后才在 kernel32 死掉。
# 注意: 该检查也验证 PATH 顺序正确 (LLVM 21 在 /usr/bin 之前),
# 防止交互 shell 续跑 make 时 PATH 丢失导致命中系统旧版工具。
if [ "${BUILD_ARM64EC}" -eq 1 ]; then
    FOUND_DLLTOOL=$(command -v llvm-dlltool)
    if ! ${FOUND_DLLTOOL} --help 2>&1 | grep -q arm64ec; then
        echo "ERROR: llvm-dlltool (${FOUND_DLLTOOL}) 不支持 arm64ec 目标"
        echo "  PATH 里的 llvm-dlltool 不是 ${LLVM_MINGW} 的版本,"
        echo "  检查 PATH 顺序 (LLVM 21 须在 /usr/bin 之前)"
        exit 1
    fi
    echo "OK: ${FOUND_DLLTOOL} supports arm64ec"
fi

# Clear build and install dir
rm -rf ${INSTALL_DIR}

# ============================================================
# 6. 编译依赖组件
# ============================================================

if [ "${DEBUG_MODE}" -eq 1 ]; then
    export BUILD_TYPE=Debug
else
    export BUILD_TYPE=Release
fi
# Build adapter
source ${ROOT_DIR}/ohos/script/build_adapter.sh

# GStreamer support for OHOS
# GSTREAMER_INSTALL_DIR=${ROOT_DIR}/third_party/gstreamer/install_ohos
# export PKG_CONFIG_PATH="${GSTREAMER_INSTALL_DIR}/lib/pkgconfig:${PKG_CONFIG_PATH}"
# export GSTREAMER_CFLAGS="-I${GSTREAMER_INSTALL_DIR}/include/gstreamer-1.0 -I${GSTREAMER_INSTALL_DIR}/include/glib-2.0 -I${GSTREAMER_INSTALL_DIR}/lib/glib-2.0/include"
# export GSTREAMER_LIBS="-L${GSTREAMER_INSTALL_DIR}/lib -lgstreamer-1.0 -lgstbase-1.0 -lgstvideo-1.0 -lgstaudio-1.0 -lgsttag-1.0 -lgobject-2.0 -lglib-2.0"
# buildargs="${buildargs} --with-gstreamer"
# echo "GStreamer support enabled: ${GSTREAMER_INSTALL_DIR}"
# source ${ROOT_DIR}/ohos/script/build_gstreamer.sh

# Build third_party freetype
source ${ROOT_DIR}/ohos/script/build_freetype.sh

# Build third_party krb5
KRB5_INSTALL_DIR=${ROOT_DIR}/third_party/krb5/out
export KRB5_CONFIG=${KRB5_INSTALL_DIR}/bin
export KRB5_CFLAGS="-I${KRB5_INSTALL_DIR}/include/"
export KRB5_LIBS="-L${KRB5_INSTALL_DIR}/lib -lkrb5 -lk5crypto -lcom_err"
source ${ROOT_DIR}/ohos/script/build_krb5.sh

# Build third_party gnutls chain (gmp/nettle/libtasn1/libunistring/gnutls) for bcrypt/schannel
source ${ROOT_DIR}/ohos/script/build_gnutls.sh
GNUTLS_STAGING=${ROOT_DIR}/build/gnutls_staging

# Build DXVK (D3D11/DXGI to Vulkan translation layer)
# arm64ec sysroot + EC_SYSROOT/EC_BUILTINS_LIB 已由 env.sh 派生并校验（COMPILE_DXVK+BUILD_ARM64EC 时）
if [ "${COMPILE_DXVK}" -eq 1 ]; then
    if [ "${DXVK_LEGACY}" -eq 1 ]; then
        source ${ROOT_DIR}/ohos/script/build_dxvk_legacy.sh
    else
        source ${ROOT_DIR}/ohos/script/build_dxvk.sh
    fi
    source ${ROOT_DIR}/ohos/script/build_vkd3d_proton.sh
fi

# ============================================================
# 7. 运行 configure
# ============================================================

# CC/CXX（aarch64-linux-ohos Unix 侧）由 env.sh 统一 export

if [ "${DEBUG_MODE}" -eq 1 ]; then
    export CFLAGS="-fPIC -D__MUSL__=1 -O0 -fno-ident -fdata-sections -ffunction-sections -fno-omit-frame-pointer -g2 -ggnu-pubnames -funwind-tables -no-canonical-prefixes"
    export CXXFLAGS="-fPIC -D__MUSL__=1 -O0 -fno-ident -fdata-sections -ffunction-sections -fno-omit-frame-pointer -g2 -ggnu-pubnames -funwind-tables -no-canonical-prefixes"
else
    export CFLAGS="-fPIC -D__MUSL__=1 -O2"
    export CXXFLAGS="-fPIC -D__MUSL__=1 -O2"
fi

export PKG_CONFIG_PATH="${GNUTLS_STAGING}/lib/pkgconfig:${PKG_CONFIG_PATH}"
export CPPFLAGS="-I${ADAPTER_DIR}/include -I${FREETYPE_DIR}/include -I${GNUTLS_STAGING}/include ${KRB5_CFLAGS}"
export LDFLAGS="-L${ADAPTER_DIR}/lib -L${FREETYPE_DIR}/build/lib -L${GNUTLS_STAGING}/lib ${KRB5_LIBS}"

# ac_cv_prog_{x86_64,i386}_{CC,CXX}（PE 侧 LLVM clang）由 env.sh 统一 export

time_start_for_build=$(date +%s)

echo "build wine in: ${BUILD_DIR}"

if [ ! -d "${BUILD_DIR}" ]; then
    mkdir -p ${BUILD_DIR}
fi

if [ ! -d "${INSTALL_DIR}" ]; then
    mkdir -p ${INSTALL_DIR}
fi

# Build arm wine
cd ${BUILD_DIR}

# Global variables.
if [ "${BUILD_ARM64EC}" -eq 1 ]; then
    ../configure \
        --without-x \
        --without-dlltool \
        --host=aarch64-linux-ohos \
        --with-mingw=clang \
        --with-wine-tools=${TOOLS_DIR} \
        --enable-archs=aarch64,arm64ec,x86_64,i386 \
        --disable-tests \
        --prefix=${INSTALL_DIR} \
        --exec-prefix=${INSTALL_DIR} \
        CC="${CC}" \
        CXX="${CXX}"
else
    ../configure \
        --without-x \
        --without-dlltool \
        --host=aarch64-linux-ohos \
        --with-wine-tools=${TOOLS_DIR} \
        --enable-archs=aarch64,i386 \
        --disable-tests \
        --prefix=${INSTALL_DIR} \
        --exec-prefix=${INSTALL_DIR} \
        CC="${CC}" \
        CXX="${CXX}"
fi

# ============================================================
# 8. 编译 Wine
# ============================================================
# PE 瘦身：ARM64/ARM64EC PE 默认 64KB 文件对齐（winegcc 将 file_align 默认取
# section_align=0x10000），通过 make 命令行变量覆盖 <arch>_LDFLAGS 追加
# -Wl,--file-alignment,0x1000（winegcc 专属解析语法，最终转成 lld-link
# -filealign:0x1000），可回收 ~314 MiB 文件内 padding。SectionAlignment 保持
# 0x10000 不变（ARM64X .a64xrm 运行时重定向按 RVA 工作，不受文件对齐影响）。
# 注意：
#  - 基值从 configure 刚生成的 Makefile 读取，不硬编码探测结果；
#  - 不能用环境变量预设 arm64ec_LDFLAGS 实现——configure 的 PE 编译器探测会
#    把该变量拼进直调 clang 的探测命令，lld-link 不认识 GNU 语法会探测失败
#    （报 "PE cross-compiler not found"）；make 命令行覆盖不经过 configure。
#  - make 覆盖会被递归 make 自动继承到所有子 Makefile 的链接规则。
#  - 用户 -Wl,-filealign:0x1000（lld 语法）无效：winegcc 内置对齐参数在后，
#    lld-link 取后者。
ALIGN_FLAG="-Wl,--file-alignment,0x1000"
EC_LDFLAGS_BASE=$(grep -m1 '^arm64ec_LDFLAGS = ' ${BUILD_DIR}/Makefile | sed 's/^arm64ec_LDFLAGS = *//')
A64_LDFLAGS_BASE=$(grep -m1 '^aarch64_LDFLAGS = ' ${BUILD_DIR}/Makefile | sed 's/^aarch64_LDFLAGS = *//')
make -j$(nproc) \
    arm64ec_LDFLAGS="${EC_LDFLAGS_BASE} ${ALIGN_FLAG}" \
    aarch64_LDFLAGS="${A64_LDFLAGS_BASE} ${ALIGN_FLAG}"

echo "install wine in: ${INSTALL_DIR}"
make install-lib

# ============================================================
# 8.5 覆盖 Wine 内置 D3D11/DXGI 为 DXVK 版本
# make install-lib 会安装 Wine 内置的 d3d11.dll/dxgi.dll（导入 wined3d.dll），
# 必须在此之后用 DXVK 版本覆盖，否则 D3D11CreateDeviceAndSwapChain 会使用
# WineD3D 后端而非 Vulkan。
# ============================================================
if [ "${COMPILE_DXVK}" -eq 1 ]; then
    # DXVK 路径由 env.sh 统一派生 (legacy 用 DXVK_LEGACY_BUILD_DIR)
    if [ "${DXVK_LEGACY}" -eq 1 ]; then DXVK_BUILD_DIR=${DXVK_LEGACY_BUILD_DIR}; fi
    echo "overriding Wine built-in D3D11/DXGI with DXVK"
    # 优先使用 ARM64X (installec)，不拷贝 installarm64 (会覆盖 ARM64X)
    for dxvk_src in installec/bin installec/lib; do
        if [ -d "${DXVK_BUILD_DIR}/${dxvk_src}" ]; then
            echo "  copying DXVK DLLs from ${dxvk_src}"
            cp ${DXVK_BUILD_DIR}/${dxvk_src}/*.dll ${INSTALL_DIR}/lib/wine/aarch64-windows/ 2>/dev/null || true
        fi
    done
    if [ -d "${DXVK_BUILD_DIR}/install32/bin" ]; then
        cp ${DXVK_BUILD_DIR}/install32/bin/*.dll ${INSTALL_DIR}/lib/wine/i386-windows/ 2>/dev/null || true
    fi
    # vkd3d-proton (d3d12); VKD3D_BUILD_DIR 由 env.sh 派生, 此处无需重绑
    for vkd3d_src in installec/bin installec/lib installarm64/bin installarm64/lib; do
        if [ -d "${VKD3D_BUILD_DIR}/${vkd3d_src}" ]; then
            echo "  copying vkd3d-proton DLLs from ${vkd3d_src}"
            cp ${VKD3D_BUILD_DIR}/${vkd3d_src}/*.dll ${INSTALL_DIR}/lib/wine/aarch64-windows/ 2>/dev/null || true
        fi
    done
    if [ -d "${VKD3D_BUILD_DIR}/install32/bin" ]; then
        cp ${VKD3D_BUILD_DIR}/install32/bin/*.dll ${INSTALL_DIR}/lib/wine/i386-windows/ 2>/dev/null || true
    fi
fi

# ============================================================
# 9. 编译应用程序
# ============================================================
if [ "${BUILD_ARM64EC}" -eq 1 ]; then
    cp ${UBT_DIR}/ubt_x64a64_win.dll ${INSTALL_DIR}/lib/wine/aarch64-windows/
    cp ${UBT_DIR}/ubt_x32a64_win.dll ${INSTALL_DIR}/lib/wine/aarch64-windows/
fi
cp ${ADAPTER_DIR}/lib/libadapter.so ${INSTALL_DIR}/lib/wine/aarch64-unix/
cp ${INSTALL_DIR}/bin/libwineserver.so ${INSTALL_DIR}/lib/wine/aarch64-unix/
# ubt_x32a64_win.dll is required by wow64 (32->64 translation), always copy (not gated on BUILD_ARM64EC)
cp ${UBT_DIR}/ubt_x32a64_win.dll ${INSTALL_DIR}/lib/wine/aarch64-windows/ 2>/dev/null || true
# Copy gnutls chain libs (bcrypt/schannel runtime deps) to out/
# gmp/libtasn1/libunistring are statically absorbed into nettle/hogweed/gnutls.
cp -d ${GNUTLS_STAGING}/lib/libgnutls.so.30* ${INSTALL_DIR}/lib/wine/aarch64-unix/ 2>/dev/null || true
cp -d ${GNUTLS_STAGING}/lib/libnettle.so.8* ${INSTALL_DIR}/lib/wine/aarch64-unix/ 2>/dev/null || true
cp -d ${GNUTLS_STAGING}/lib/libhogweed.so.6* ${INSTALL_DIR}/lib/wine/aarch64-unix/ 2>/dev/null || true

echo "copy resources"
cd ${ROOT_DIR}/
rm -rf ${APP_DIR}/wine_engine/src/main/resources/resfile/
mkdir -p ${APP_DIR}/wine_engine/src/main/resources/resfile/lib/wine
mkdir -p ${APP_DIR}/wine_engine/src/main/resources/resfile/share
cp ${INSTALL_DIR}/lib/wine/aarch64-unix/*.so ${APP_DIR}/wine_engine/libs/arm64-v8a
cp -d ${FREETYPE_DIR}/build/lib/libfreetype.so ${APP_DIR}/wine_engine/libs/arm64-v8a
cp -d ${FREETYPE_DIR}/build/lib/libfreetype.so.6 ${APP_DIR}/wine_engine/libs/arm64-v8a
cp ${FREETYPE_DIR}/build/lib/libfreetype.so.6.20.6 ${APP_DIR}/wine_engine/libs/arm64-v8a
# Copy gnutls chain libs to HAP (dlopen by bcrypt.so/secur32.so at runtime)
cp -d ${GNUTLS_STAGING}/lib/libgnutls.so.30* ${APP_DIR}/wine_engine/libs/arm64-v8a 2>/dev/null || true
cp -d ${GNUTLS_STAGING}/lib/libnettle.so.8* ${APP_DIR}/wine_engine/libs/arm64-v8a 2>/dev/null || true
cp -d ${GNUTLS_STAGING}/lib/libhogweed.so.6* ${APP_DIR}/wine_engine/libs/arm64-v8a 2>/dev/null || true

cp -r ${INSTALL_DIR}/share/wine ${APP_DIR}/wine_engine/src/main/resources/resfile/share
cp -r ${INSTALL_DIR}/lib/wine/aarch64-windows ${APP_DIR}/wine_engine/src/main/resources/resfile/lib/wine/
cp -r ${INSTALL_DIR}/lib/wine/i386-windows ${APP_DIR}/wine_engine/src/main/resources/resfile/lib/wine/

# ============================================================
# 9.5 PE 瘦身：strip-all（DWARF 调试节 + COFF 符号表）
# 只 strip resfile 副本（入 HAP 的），out/ 与 build/ 保持未 strip 供崩溃符号化。
# 无参数 llvm-strip = strip-all（同 WineHua 做法）；--strip-debug 不删符号表。
# ubt_*.dll 为闭源翻译器输入，无标准 DWARF（仅自定义 .dbgsymt），排除不动。
# strip 不重排节布局——64KB 对齐 padding 的回收由上面的 file-alignment sed + relink 完成。
# ============================================================
STRIP="${PE_STRIP:-}"
[ -x "${STRIP}" ] || STRIP="${OHOS_SDK}/native/llvm/bin/llvm-strip"
if [ ! -x "${STRIP}" ]; then
    echo "ERROR: llvm-strip not found (PE_STRIP=${PE_STRIP:-unset}, OHOS_SDK=${OHOS_SDK:-unset})"
    exit 1
fi
RESFILE_PE="${APP_DIR}/wine_engine/src/main/resources/resfile/lib/wine"
find "${RESFILE_PE}" -type f \
    \( -name '*.dll' -o -name '*.exe' -o -name '*.drv' -o -name '*.sys' \
       -o -name '*.cpl' -o -name '*.ocx' -o -name '*.acm' \) \
    ! -name 'ubt_*.dll' \
    -exec "${STRIP}" {} \;
echo "PE strip-all done: resfile PE now $(du -sh --apparent-size ${RESFILE_PE} | cut -f1)"

# DXVK + vkd3d-proton DLLs 已在 step 8.5 中覆盖到 INSTALL_DIR，
# 上面的 cp -r 从 INSTALL_DIR 复制到 resfile 时自动包含 DXVK 版本。
# 无需再次拷贝。
if [ "${WITH_GECKO}" -eq 1 ]; then
    cp -r ${ROOT_DIR}/third_party/gecko ${APP_DIR}/wine_engine/src/main/resources/resfile/share/wine/
fi
if [ "${WITH_MONO}" -eq 1 ]; then
    cp -r ${ROOT_DIR}/third_party/mono ${APP_DIR}/wine_engine/src/main/resources/resfile/share/wine/
fi

# Copy GStreamer libraries if available
# if [ -d "${GSTREAMER_INSTALL_DIR}" ]; then
#     echo "copying GStreamer libraries"
#     mkdir -p ${APP_DIR}/wine_engine/libs/arm64-v8a/gstreamer-1.0
#     cp -r ${GSTREAMER_INSTALL_DIR}/lib/*.so* ${APP_DIR}/wine_engine/libs/arm64-v8a/
#     # Copy GStreamer plugins
#     echo "copying GStreamer plugins"
#     cp -r ${GSTREAMER_INSTALL_DIR}/lib/gstreamer-1.0/*.so* ${APP_DIR}/wine_engine/libs/arm64-v8a/gstreamer-1.0/ 2>/dev/null
# fi

# Copy krb5 lib
if [ -d "${KRB5_INSTALL_DIR}" ]; then
    echo "copying krb5 libraries"
    mkdir -p ${APP_DIR}/wine_engine/libs/arm64-v8a/krb5/
    cp -r ${KRB5_INSTALL_DIR}/lib/*.so* ${APP_DIR}/wine_engine/libs/arm64-v8a/
    # Copy krb5 plugins
    echo "copying krb5 plugins"
    cp -r ${KRB5_INSTALL_DIR}/lib/krb5/plugins ${APP_DIR}/wine_engine/libs/arm64-v8a/krb5/ 2>/dev/null
fi

if [ "${COMPILE_HAP}" -eq 1 ]; then
    echo "compile hap"
    # PATH/DEVECO_SDK_HOME/DEVECO_NODE_HOME 由 env.sh 统一 export
    cd ${APP_DIR}/
    rm -rf ~/.hvigor
    ohpm install --all
    hvigorw clean --daemon
    hvigorw --sync -p product=default --analyze=normal --parallel --incremental --daemon
    hvigorw --mode module -p product=default assembleHap --analyze=normal --parallel --incremental --daemon

    echo "copy hap"
    cd ${ROOT_DIR}/
    mkdir ${INSTALL_DIR}/wine_hap/
    cp ${APP_DIR}/entry/build/default/outputs/default/entry-default-signed.hap ${INSTALL_DIR}/wine_hap/ 2>/dev/null || \
    cp ${APP_DIR}/entry/build/default/outputs/default/entry-default-unsigned.hap ${INSTALL_DIR}/wine_hap/
fi

time_end_for_build=$(date +%s)

time_format() {
  hours=$((($1 - $2) / 3600))
  minutes=$((($1 - $2) % 3600 / 60))
  seconds=$((($1 - $2) % 60))
}

time_format $time_end_for_build $time_start_for_build
printf "\e[32mTime for Total : %dH:%dM:%dS \e[0m\n\n" $hours $minutes $seconds

echo "build done"

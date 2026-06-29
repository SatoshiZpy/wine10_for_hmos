#!/bin/bash

set -ex

# ============================================================
# 1. 定义路径和版本
# ============================================================

basedir=$(dirname "$0")
ROOT_DIR=$PWD

# --- LLVM 工具路径 ---
LLVM_ARCHIVE="${ROOT_DIR}/third_party/LLVM-21.1.0-Linux-X64.tar.xz"
LLVM_DIR=${ROOT_DIR}/third_party/llvm
LLVM_TOOLCHAIN=${LLVM_DIR}/LLVM-21.1.0-Linux-X64

# --- Wine 源码路径 ---
BUILD_DIR=${ROOT_DIR}/build
INSTALL_DIR=${ROOT_DIR}/out
ADAPTER_DIR=${ROOT_DIR}/ohos/adapter
APP_DIR=${ROOT_DIR}/ohos/app
UBT_DIR=${ROOT_DIR}/ohos/ubt

# --- OpenHarmony SDK 路径 ---
OHOS_SDK=${ROOT_DIR}/ohos_sdk/command-line-tools/sdk/default/openharmony
OHOS_SYSROOT=${OHOS_SDK}/native/sysroot
OHOS_TOOLCHAIN=${OHOS_SDK}/native/llvm
CMAKE_PATH=${OHOS_SDK}/native/build-tools/cmake/bin/cmake
NINJA_PATH=${OHOS_SDK}/native/build-tools/cmake/bin/ninja
CMAKE_FILE_PATH=${OHOS_SDK}/native/build/cmake/ohos.toolchain.cmake

# --- Wine 工具路径 ---
TOOLS_DIR=${ROOT_DIR}/build-tools

# ============================================================
# 2. 解析命令行参数
# ============================================================
NO_CACHE=0
BUILD_ARM64EC=0
COMPILE_HAP=0
DEBUG_MODE=0
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
        --compile-hap)
            COMPILE_HAP=1
            shift
            ;;
        --debug)
            DEBUG_MODE=1
            shift
            ;;
        -h|--help)
            echo "用法: $0 [--no-cache]"
            echo ""
            echo "选项:"
            echo "  --no-cache        清理所有缓存后重新编译"
            echo "  --build-arm64ec   编译arm64ec/wow64 mode dll"
            echo "  --compile-hap     编译HAP包"
            echo "  --debug           启用调试模式"
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
    git clean -xfd --exclude='ohos_sdk/command-line-tools/'
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
autoconf

if [ ! -d "${TOOLS_DIR}" ]; then
    echo "build x86 wine in ${TOOLS_DIR}"
    mkdir -p ${TOOLS_DIR}
    cd ${TOOLS_DIR}

    ../configure --without-x
    make -j$(nproc)

    echo "build x86 wine done"
fi

# ============================================================
# 4. 准备 OpenHarmony 工具链
# ============================================================
cd ${ROOT_DIR}

# Extract ohos_sdk.
COMMAND_LINE_TOOLS_TAR="ohos_sdk/commandline-tools-linux-x64-6.0.2.650.zip"
COMMAND_LINE_TOOLS_DIR="ohos_sdk/command-line-tools"
if [ ! -d "${COMMAND_LINE_TOOLS_DIR}" ]; then
    unzip -q "${COMMAND_LINE_TOOLS_TAR}" -d ohos_sdk

    file="${OHOS_TOOLCHAIN}/bin/lld-link"
    if [ -L "$file" ]; then
        echo "$file"
    else
        ln -sf ${OHOS_TOOLCHAIN}/bin/ld.lld ${OHOS_TOOLCHAIN}/bin/lld-link
    fi
fi
export PATH=$PATH:${ROOT_DIR}/ohos_sdk/command-line-tools/bin

# ============================================================
# 5. 准备 LLVM 工具链
# ============================================================
if [ ! -f "${LLVM_ARCHIVE}" ]; then
    echo "${LLVM_ARCHIVE} not exist"
    exit 1
fi

if [ ! -d "${LLVM_DIR}" ]; then
    mkdir -p ${LLVM_DIR}
fi

if [ ! -d "${LLVM_TOOLCHAIN}" ]; then
    tar -xvf "${LLVM_ARCHIVE}" -C "${LLVM_DIR}"
else
    echo "${LLVM_TOOLCHAIN} is not empty, skip extracting LLVM."
fi
export PATH=${LLVM_TOOLCHAIN}/bin/:$PATH

# Clear build and install dir
rm -rf ${INSTALL_DIR}

# ============================================================
# 6. 编译依赖组件
# ============================================================

# Build adapter
if [ "${DEBUG_MODE}" -eq 1 ]; then
    export ADAPTER_BUILD_TYPE=Debug
else
    export ADAPTER_BUILD_TYPE=Release
fi
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

# ============================================================
# 7. 运行 configure
# ============================================================

# --- 设置环境变量, 为 --host 指定 Clang 版本 ---
export CC="${OHOS_TOOLCHAIN}/bin/clang --target=aarch64-linux-ohos"
export CXX="${OHOS_TOOLCHAIN}/bin/clang++ --target=aarch64-linux-ohos"

if [ "${DEBUG_MODE}" -eq 1 ]; then
    export CFLAGS="-fPIC -D__MUSL__=1 -g"
    export CXXFLAGS="-fPIC -D__MUSL__=1 -g"
else
    export CFLAGS="-fPIC -D__MUSL__=1"
    export CXXFLAGS="-fPIC -D__MUSL__=1"
fi

export CPPFLAGS="-I${ADAPTER_DIR}/include -I${FREETYPE_DIR}/include ${KRB5_CFLAGS}"
export LDFLAGS="-L${ADAPTER_DIR}/lib -L${FREETYPE_DIR}/build/lib ${KRB5_LIBS}"

# --- 设置 MinGW 目标的 Clang 版本 ---
export ac_cv_prog_x86_64_CC="${LLVM_TOOLCHAIN}/bin/clang"
export ac_cv_prog_i386_CC="${LLVM_TOOLCHAIN}/bin/clang"
export ac_cv_prog_x86_64_CXX="${LLVM_TOOLCHAIN}/bin/clang++"
export ac_cv_prog_i386_CXX="${LLVM_TOOLCHAIN}/bin/clang++"

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
        --enable-archs=aarch64,arm64ec,i386 \
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
make -j$(nproc)

echo "install wine in: ${INSTALL_DIR}"
make install-lib

# ============================================================
# 9. 编译应用程序
# ============================================================
if [ "${BUILD_ARM64EC}" -eq 1 ]; then
    cp ${UBT_DIR}/ubt_x64a64_win.dll ${INSTALL_DIR}/lib/wine/aarch64-windows/
    cp ${UBT_DIR}/ubt_x32a64_win.dll ${INSTALL_DIR}/lib/wine/aarch64-windows/
fi
cp ${ADAPTER_DIR}/lib/libadapter.so ${INSTALL_DIR}/lib/wine/aarch64-unix/
cp ${INSTALL_DIR}/bin/libwineserver.so ${INSTALL_DIR}/lib/wine/aarch64-unix/

echo "copy resources"
cd ${ROOT_DIR}/
rm -rf ${APP_DIR}/wine_engine/src/main/resources/resfile/
mkdir -p ${APP_DIR}/wine_engine/src/main/resources/resfile/lib/wine
mkdir -p ${APP_DIR}/wine_engine/src/main/resources/resfile/share
cp ${INSTALL_DIR}/lib/wine/aarch64-unix/*.so ${APP_DIR}/wine_engine/libs/arm64-v8a
cp -d ${FREETYPE_DIR}/build/lib/libfreetype.so ${APP_DIR}/wine_engine/libs/arm64-v8a
cp -d ${FREETYPE_DIR}/build/lib/libfreetype.so.6 ${APP_DIR}/wine_engine/libs/arm64-v8a
cp ${FREETYPE_DIR}/build/lib/libfreetype.so.6.20.6 ${APP_DIR}/wine_engine/libs/arm64-v8a

cp -r ${INSTALL_DIR}/share/wine ${APP_DIR}/wine_engine/src/main/resources/resfile/share
cp -r ${INSTALL_DIR}/lib/wine/aarch64-windows ${APP_DIR}/wine_engine/src/main/resources/resfile/lib/wine/

if [ "${BUILD_ARM64EC}" -eq 1 ]; then
    cp -r ${ROOT_DIR}/third_party/gecko ${APP_DIR}/wine_engine/src/main/resources/resfile/share/wine/
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
    cd ${APP_DIR}/
    rm -rf ~/.hvigor
    ohpm install --all
    hvigorw clean --daemon
    hvigorw --sync -p product=default --analyze=normal --parallel --incremental --daemon
    hvigorw --mode module -p product=default assembleHap --analyze=normal --parallel --incremental --daemon

    echo "copy hap"
    cd ${ROOT_DIR}/
    mkdir ${INSTALL_DIR}/wine_hap/
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

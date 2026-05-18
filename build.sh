#!/bin/bash

set -ex
basedir=$(dirname "$0")
ROOT_DIR=$PWD
BUILD_TOOLS=${ROOT_DIR}/nativeBuild
BUILD_DIR=${ROOT_DIR}/build
INSTALL_DIR=${ROOT_DIR}/out
ADAPTER_DIR=${ROOT_DIR}/ohos/adapter
APP_DIR=${ROOT_DIR}/ohos/app
FEX_DIR=${ROOT_DIR}/ohos/fex
LLVM_DIR=${ROOT_DIR}/third_party/llvm
ARM64EC_BUILD_DIR=${ROOT_DIR}/arm64ecbuild
ARM64EC_INSTALL_DIR=${ROOT_DIR}/out-arm64ec

OHOS_SDK=${ROOT_DIR}/ohos_sdk/command-line-tools/sdk/default/openharmony
CMAKE_PATH=${OHOS_SDK}/native/build-tools/cmake/bin/cmake
NINJA_PATH=${OHOS_SDK}/native/build-tools/cmake/bin/ninja
CMAKE_FILE_PATH=${OHOS_SDK}/native/build/cmake/ohos.toolchain.cmake

# Processing parameters
NO_CACHE=0
BUILD_ARM64EC=0
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
        -h|--help)
            echo "用法: $0 [--no-cache]"
            echo ""
            echo "选项:"
            echo "  --no-cache        清理所有缓存后重新编译"
            echo "  --build-arm64ec   编译arm64ec/wow64 mode dll"
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

# Global variables.
buildargs="--without-x --without-dlltool --host=aarch64-linux-ohos --with-wine-tools=${BUILD_TOOLS}"

# Extract ohos_sdk.
COMMAND_LINE_TOOLS_TAR="ohos_sdk/commandline-tools-linux-x64-6.0.2.650.zip"
COMMAND_LINE_TOOLS_DIR="ohos_sdk/command-line-tools"
if [ ! -d "${COMMAND_LINE_TOOLS_DIR}" ]; then
    unzip -q "${COMMAND_LINE_TOOLS_TAR}" -d ohos_sdk

    file="${OHOS_SDK}/native/llvm/bin/lld-link"
    if [ -L "$file" ]; then
        echo "$file"
    else
        ln -sf ${OHOS_SDK}/native/llvm/bin/ld.lld ${OHOS_SDK}/native/llvm/bin/lld-link
    fi
fi
export PATH=$PATH:${ROOT_DIR}/ohos_sdk/command-line-tools/bin

# Clear out
rm -rf ${INSTALL_DIR}
rm -rf ${ARM64EC_INSTALL_DIR}

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

# Build wine dll for arm64ec and wow64 mode
if [ "${BUILD_ARM64EC}" -eq 1 ]; then
    source ${ROOT_DIR}/ohos/script/build_arm64ec.sh
fi

# Build x86 wine
cd ${ROOT_DIR}
autoconf

if [ ! -d "${BUILD_TOOLS}" ]; then
    echo "build x86 wine in ${BUILD_TOOLS}"
    mkdir -p ${BUILD_TOOLS}
    cd ${BUILD_TOOLS}

    ../configure --without-x
    make -j$(nproc)

    echo "build x86 wine done"
fi

export AS=${OHOS_SDK}/native/llvm/bin/llvm-as
export CC="${OHOS_SDK}/native/llvm/bin/clang --target=aarch64-linux-ohos"
export CXX="${OHOS_SDK}/native/llvm/bin/clang++ --target=aarch64-linux-ohos"
export LD=${OHOS_SDK}/native/llvm/bin/ld.lld
export STRIP=${OHOS_SDK}/native/llvm/bin/llvm-strip
export RANLIB=${OHOS_SDK}/native/llvm/bin/llvm-ranlib
export OBJDUMP=${OHOS_SDK}/native/llvm/bin/llvm-objdump
export OBJCOPY=${OHOS_SDK}/native/llvm/bin/llvm-objcopy
export NM=${OHOS_SDK}/native/llvm/bin/llvm-nm
export AR=${OHOS_SDK}/native/llvm/bin/llvm-ar
export CFLAGS="-fPIC -D__MUSL__=1"
export CXXFLAGS="-fPIC -D__MUSL__=1"
export CPPFLAGS="-I${ADAPTER_DIR}/include -I${FREETYPE_DIR}/include"
export LDFLAGS="-L${ADAPTER_DIR}/lib -L${FREETYPE_DIR}/build ${GSTREAMER_LIBS}"
export PATH=${OHOS_SDK}/native/llvm/bin:$PATH

time_start_for_build=$(date +%s)

echo "generating args list: $buildargs"
echo "build wine in: ${BUILD_DIR}"

if [ ! -d "${BUILD_DIR}" ]; then
    mkdir -p ${BUILD_DIR}
fi

if [ ! -d "${INSTALL_DIR}" ]; then
    mkdir -p ${INSTALL_DIR}
fi

# Build arm wine
cd ${BUILD_DIR}

../configure ${buildargs} --prefix=${INSTALL_DIR} --exec-prefix=${INSTALL_DIR}
make -j$(nproc)

echo "install wine in: ${INSTALL_DIR}"
make install-lib

if [ "${BUILD_ARM64EC}" -eq 1 ]; then
    cp ${INSTALL_DIR}/lib/wine/aarch64-windows/wineboot.exe ${ARM64EC_INSTALL_DIR}/lib/wine/aarch64-windows/
    cp ${FEX_DIR}/libarm64ecfex.dll ${ARM64EC_INSTALL_DIR}/lib/wine/aarch64-windows/
    cp ${FEX_DIR}/libwow64fex.dll ${ARM64EC_INSTALL_DIR}/lib/wine/aarch64-windows/
fi
cp ${FREETYPE_DIR}/build/libfreetype.so.6.20.6 ${INSTALL_DIR}/lib/wine/aarch64-unix/libfreetype.so
cp ${FREETYPE_DIR}/build/libfreetype.so.6.20.6 ${INSTALL_DIR}/lib/wine/aarch64-unix/libfreetype.so.6
cp ${ADAPTER_DIR}/lib/libadapter.so ${INSTALL_DIR}/lib/wine/aarch64-unix/
cp ${INSTALL_DIR}/bin/libwineserver.so ${INSTALL_DIR}/lib/wine/aarch64-unix/

echo "copy resources"
cd ${ROOT_DIR}/
rm -rf ${APP_DIR}/wine_engine/src/main/resources/resfile/
mkdir -p ${APP_DIR}/wine_engine/src/main/resources/resfile/lib/wine
mkdir -p ${APP_DIR}/wine_engine/src/main/resources/resfile/share
cp ${INSTALL_DIR}/lib/wine/aarch64-unix/*.so ${APP_DIR}/wine_engine/libs/arm64-v8a
if [ "${BUILD_ARM64EC}" -eq 1 ]; then
    cp -r ${ARM64EC_INSTALL_DIR}/share/wine ${APP_DIR}/wine_engine/src/main/resources/resfile/share
    cp -r ${ARM64EC_INSTALL_DIR}/lib/wine/aarch64-windows ${APP_DIR}/wine_engine/src/main/resources/resfile/lib/wine/
else
    cp -r ${INSTALL_DIR}/share/wine ${APP_DIR}/wine_engine/src/main/resources/resfile/share
    cp -r ${INSTALL_DIR}/lib/wine/aarch64-windows ${APP_DIR}/wine_engine/src/main/resources/resfile/lib/wine/
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

time_end_for_build=$(date +%s)

time_format() {
  hours=$((($1 - $2) / 3600))
  minutes=$((($1 - $2) % 3600 / 60))
  seconds=$((($1 - $2) % 60))
}

time_format $time_end_for_build $time_start_for_build
printf "\e[32mTime for Total : %dH:%dM:%dS \e[0m\n\n" $hours $minutes $seconds

echo "build done"

#!/bin/bash

set -ex

FREETYPE_DIR="${ROOT_DIR}/third_party/freetype"

if [ ! -d "${FREETYPE_DIR}/build" ]; then
    mkdir -p ${FREETYPE_DIR}/build
fi

# Build freetype
cd ${FREETYPE_DIR}/build
${CMAKE_PATH} \
    -DOHOS_SDK_NATIVE=${OHOS_SDK}/native \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=${OHOS_SDK}/native/llvm/bin/clang \
    -DCMAKE_CXX_COMPILER=${OHOS_SDK}/native/llvm/bin/clang++ \
    -DCMAKE_TOOLCHAIN_FILE=${CMAKE_FILE_PATH} \
    -DBUILD_SHARED_LIBS=ON \
    -DOHOS_STL=c++_shared \
    -DAIPIPE_FRAMEWORK_LIB_FORMAT=SHARED \
    -DOHOS_ARCH=arm64-v8a \
    -DOHOS_PLATFORM=OHOS \
    -DCMAKE_DISABLE_FIND_PACKAGE_ZLIB=TRUE \
    -DCMAKE_DISABLE_FIND_PACKAGE_BZip2=TRUE \
    -DCMAKE_DISABLE_FIND_PACKAGE_PNG=TRUE \
    -DCMAKE_DISABLE_FIND_PACKAGE_HarfBuzz=TRUE \
    -DCMAKE_DISABLE_FIND_PACKAGE_BrotliDec=TRUE \
    ../
 
make -j$(nproc)

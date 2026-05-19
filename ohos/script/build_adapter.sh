#!/bin/bash

set -ex

if [ ! -d "${ADAPTER_DIR}/build" ]; then
    mkdir -p ${ADAPTER_DIR}/build
fi

# Build adapter
cd ${ADAPTER_DIR}/build
${CMAKE_PATH} \
    -G "Ninja" \
    -DOHOS_ARCH=arm64-v8a \
    -DOHOS_STL=c++_shared \
    -DOHOS_PLATFORM=OHOS \
    -DCMAKE_TOOLCHAIN_FILE=${CMAKE_FILE_PATH} \
    -DCMAKE_MAKE_PROGRAM=${NINJA_PATH} \
    -DAKI_BUILDING_SHARED=OFF \
    -DCMAKE_BUILD_TYPE=${ADAPTER_BUILD_TYPE} \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    ../

${CMAKE_PATH} --build .

echo "cp libadapter.so: ${ADAPTER_DIR}/lib"
if [ ! -d "${ADAPTER_DIR}/lib" ]; then
    mkdir -p ${ADAPTER_DIR}/lib
fi
cp -rf libadapter.so ${ADAPTER_DIR}/lib
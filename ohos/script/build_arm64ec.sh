#!/bin/bash

set -ex

# Build wine dll for arm64ec and wow64 mode
LLVM_ARCHIVE="${ROOT_DIR}/third_party/LLVM-21.1.0-Linux-X64.tar.xz"

if [ ! -d "${ARM64EC_BUILD_DIR}" ]; then
    mkdir -p ${ARM64EC_BUILD_DIR}
fi

if [ ! -d "${LLVM_DIR}" ]; then
    mkdir -p ${LLVM_DIR}
fi

if [ ! -f "${LLVM_ARCHIVE}" ]; then
    echo "LLVM-21.1.0-Linux-X64.tar.xz not exsit"
    exit 1
fi
 
if [ -z "$(ls -A "${LLVM_DIR}")" ]; then
    tar -xvf "${LLVM_ARCHIVE}" -C "${LLVM_DIR}"
else
    echo "${LLVM_DIR} is not empty, skip extracting LLVM."
fi

export PATH=${LLVM_DIR}/LLVM-21.1.0-Linux-X64/bin/:$PATH
cd ${ARM64EC_BUILD_DIR}
../configure \
    --without-gstreamer \
    --without-x \
    --with-mingw=clang \
    --enable-archs=aarch64,arm64ec,i386 \
    --disable-tests \
    --prefix=${ARM64EC_INSTALL_DIR} \
    --exec-prefix=${ARM64EC_INSTALL_DIR}

make -j $(nproc) && make install-lib

../configure \
    --enable-wineohos-drv \
    --without-x \
    --with-mingw=clang \
    --enable-archs=aarch64,arm64ec,i386 \
    --disable-tests
 
make dlls/wineohos.drv/aarch64-windows/wineohos.drv dlls/wineohos.drv/i386-windows/wineohos.drv
cp dlls/wineohos.drv/aarch64-windows/wineohos.drv ${ARM64EC_INSTALL_DIR}/lib/wine/aarch64-windows/
cp dlls/wineohos.drv/i386-windows/wineohos.drv ${ARM64EC_INSTALL_DIR}/lib/wine/i386-windows/
 

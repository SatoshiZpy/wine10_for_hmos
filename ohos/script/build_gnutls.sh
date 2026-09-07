#!/bin/bash
# build_gnutls.sh — GnuTLS chain cross-compile for aarch64-linux-ohos
# Sources from third_party/ submodules (bootstrapped), uses build.sh env vars.
# Output -> ${ROOT_DIR}/build/gnutls_staging (for Wine bcrypt/schannel)
# Idempotent: skips if all products present.
set -euo pipefail

# Use build.sh env vars; fallback if sourced standalone
ROOT_DIR="${ROOT_DIR:-$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")/../.." && pwd)}"
OHOS_SDK="${OHOS_SDK:-${ROOT_DIR}/ohos_sdk/command-line-tools/sdk/default/openharmony}"
OHOS_TOOLCHAIN="${OHOS_TOOLCHAIN:-${OHOS_SDK}/native/llvm}"
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build}"

CLANG="${OHOS_TOOLCHAIN}/bin/clang"
SYSROOT="${OHOS_SDK}/native/sysroot"
TARGET=aarch64-linux-ohos
STAGING="${BUILD_DIR}/gnutls_staging"
SRC="${ROOT_DIR}/third_party"
JOBS="${JOBS:-$(nproc)}"

GNUTLS_STAGING="${STAGING}"  # exported name for build.sh consumer

idempotent_done() {
    [ -f "$STAGING/lib/libgnutls.so.30" ] \
        && [ -f "$STAGING/lib/libnettle.so.8" ] \
        && [ -f "$STAGING/lib/libhogweed.so.6" ] \
        && [ -f "$STAGING/lib/libgmp.so.10" ] \
        && [ -f "$STAGING/lib/libtasn1.so.6" ] \
        && [ -f "$STAGING/lib/libunistring.so.5" ] \
        && [ -f "$STAGING/include/gnutls/gnutls.h" ]
}

if idempotent_done; then
    echo "GnuTLS chain already built, skipping"
    exit 0
fi

mkdir -p "$STAGING/include" "$STAGING/lib" "$STAGING/lib/pkgconfig"

CROSS_CFLAGS="-O2 -fPIC -D__MUSL__"
CROSS_LDFLAGS="-fuse-ld=lld --sysroot=$SYSROOT --target=$TARGET"
export PKG_CONFIG_PATH="$STAGING/lib/pkgconfig"
export CFLAGS="-I$STAGING/include $CROSS_CFLAGS"
export LDFLAGS="-L$STAGING/lib $CROSS_LDFLAGS"
export CC="$CLANG --target=$TARGET --sysroot=$SYSROOT"
export MAKEINFO=true  # skip texinfo docs

build_one() {
    local name="$1" src="$2"; shift 2
    local build="$BUILD_DIR/${name}_build_aarch64"
    echo "=== build $name ==="
    rm -rf "$build"
    mkdir -p "$build"
    cd "$build"
    CC="$CC" CFLAGS="$CFLAGS" LDFLAGS="$LDFLAGS" \
    "$src/configure" --host=aarch64-linux-gnu --prefix="$STAGING" \
        --libdir="$STAGING/lib" --disable-static \
        "$@"
    case "$name" in
        libtasn1)
            sed -i 's/^SUBDIRS = .*/SUBDIRS = lib src/' "$build/Makefile"
            touch "$build/Makefile"
            ;;
        libunistring)
            sed -i 's/^SUBDIRS = .*/SUBDIRS = doc gnulib-local lib/' "$build/Makefile"
            # libunistring 的 tests/Makefile.gnulib 有语法错误, 防 automake regen src/Makefile.in
            touch "$build/Makefile" "$src/Makefile.in" "$src/aclocal.m4" "$src/Makefile.am" 2>/dev/null || true
            ;;
        gnutls)
            mkdir -p "$build/lib"
            for t in gnutls_asn1_tab.c pkix_asn1_tab.c; do
                if [ ! -f "$build/lib/$t" ] && [ -f "$src/lib/$t" ]; then
                    cp "$src/lib/$t" "$build/lib/$t"
                fi
                [ -f "$build/lib/$t" ] && touch "$build/lib/$t"
            done
            if [ -f "$src/lib/inih/ini.c" ]; then
                mkdir -p "$build/lib/inih"
                cp "$src/lib/inih/ini.c" "$build/lib/inih/ini.c"
                cp "$src/lib/inih/ini.h" "$build/lib/inih/ini.h"
                sed -i '1i #include "config.h"' "$build/lib/inih/ini.c"
                touch "$build/lib/inih/ini.c" "$build/lib/inih/ini.h"
            fi
            ;;
    esac
    make -o Makefile -j$JOBS
    make -o Makefile install
    # git-tree bootstrap leaves Version: UNKNOWN in libtasn1.pc; fix for downstream
    if [ "$name" = "libtasn1" ] && [ -f "$STAGING/lib/pkgconfig/libtasn1.pc" ]; then
        sed -i 's/^Version: UNKNOWN/Version: 4.20.0/' "$STAGING/lib/pkgconfig/libtasn1.pc"
    fi
    cd "$ROOT_DIR"
}

# gmp: 用 release tarball 6.2.1 (gmp 无官方 git, 只有 Mercurial hg 在 gmplib.org;
# 提交官方 release tarball 到 gnutls_tarballs/, git-lfs 跟踪)
GMP_SRC="$SRC/gmp"
GMP_TARBALL="${ROOT_DIR}/gnutls_tarballs/gmp-6.2.1.tar.xz"
GMP_TAR_DIR="${BUILD_DIR}/gnutls_tarballs/gmp"
if [ -f "$GMP_TARBALL" ] && [ ! -f "$GMP_TAR_DIR/configure" ]; then
    mkdir -p "${BUILD_DIR}/gnutls_tarballs"
    tar -xJf "$GMP_TARBALL" -C "${BUILD_DIR}/gnutls_tarballs/"
    [ -d "${BUILD_DIR}/gnutls_tarballs/gmp-6.2.1" ] && mv "${BUILD_DIR}/gnutls_tarballs/gmp-6.2.1" "$GMP_TAR_DIR"
fi
[ -f "$GMP_TAR_DIR/configure" ] && GMP_SRC="$GMP_TAR_DIR"
build_one gmp "$GMP_SRC" \
    --disable-assembly --enable-cxx=no --disable-dependency-tracking

build_one libtasn1 "$SRC/libtasn1" \
    --disable-doc --disable-dependency-tracking --disable-tests

build_one libunistring "$SRC/libunistring" \
    --disable-dependency-tracking --without-libiconv-prefix

# nettle: submodule master is 4.0 (API-incompatible with gnutls 3.8.3).
# Use release tarball 3.10.2 (version-matched) if available.
NETTLE_SRC="$SRC/nettle"
if [ -f "${ROOT_DIR}/build/gnutls_tarballs/nettle/configure" ]; then
    NETTLE_SRC="${ROOT_DIR}/build/gnutls_tarballs/nettle"
fi
build_one nettle "$NETTLE_SRC" \
    --disable-documentation --disable-openssl --disable-assembler \
    --disable-dependency-tracking

build_one gnutls "$SRC/gnutls" \
    --disable-doc --disable-tools --disable-tests --disable-full-test-suite --disable-gtk-doc --disable-cxx \
    --disable-guile --disable-valgrind-tests --disable-code-coverage \
    --without-p11-kit --without-tpm --without-brotli --without-zstd \
    --without-libpsl --without-idn --without-libidn2 --without-unbound \
    --without-libxml2 --disable-dependency-tracking

# gnutls src/gl/tests uses glibc macros; exclude tests from build
if [ -f "$BUILD_DIR/gnutls_build_aarch64/src/gl/Makefile" ]; then
    sed -i 's/^SUBDIRS = . tests/SUBDIRS = ./' "$BUILD_DIR/gnutls_build_aarch64/src/gl/Makefile"
fi

echo "=== GnuTLS chain ready: $STAGING ==="
export GNUTLS_STAGING

#!/bin/bash
# Build the Vulkan layer probe .so files with the OHOS SDK toolchain.
# Produces libVkLayer_codex_probe.so (the layer) and libvklayerprobe.so
# (the napi probe module) into OUT_DIR, which defaults to the entry
# module's prebuilt library directory of ohos/app.
set -e

SDK=${SDK:-/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/native}
CC="$SDK/llvm/bin/clang"
SYSROOT="$SDK/sysroot"
TARGET="--target=aarch64-linux-ohos"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SRC_DIR="$SCRIPT_DIR/src"
OUT_DIR=${OUT_DIR:-"$SCRIPT_DIR/../../ohos/app/entry/libs/arm64-v8a"}

mkdir -p "$OUT_DIR"

# The layer library: exports vkGetInstanceProcAddr / vkCreateInstance /
# vkEnumerateInstanceLayerProperties and links only hilog + libc.
"$CC" $TARGET --sysroot="$SYSROOT" -shared -fPIC -O2 -Wall \
    -I"$SRC_DIR" -I"$SYSROOT/usr/include/vulkan" \
    "$SRC_DIR/codex_layer.c" \
    -o "$OUT_DIR/libVkLayer_codex_probe.so" \
    -lhilog_ndk.z

# The napi probe module.
"$CC" $TARGET --sysroot="$SYSROOT" -shared -fPIC -O2 -Wall \
    -I"$SRC_DIR" \
    "$SRC_DIR/vklayerprobe.c" \
    -o "$OUT_DIR/libvklayerprobe.so" \
    -lvulkan -lace_napi.z -lhilog_ndk.z

ls -la "$OUT_DIR"

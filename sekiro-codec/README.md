# Experimental Sekiro ARM64EC resource decoder

Based on HMDeck commit 13b609fc1ccc356f7ad3d814caab8329c04d0fe6. The decoder and bridge retain their upstream licensing; see thirdparty/hmos_ooz/COPYING and NOTICE. The build script verifies the vendor file hashes and uses Wine's winebuild to mark the resulting ARM64EC PE as builtin.

The launcher enables this only for sekiro.exe when the adjacent oo2core_6_win64.dll has SHA-256 988e8226d129b03011a96f51b476e86d38c0c3e817266375a78a7cf3b85d93c4. It copies the original to the Wine prefix for fallback; it does not modify the game directory. A private launcher-session marker controls Wine's builtin override and is cleared on another launch. This experimental mechanism assumes only one launcher session at a time.

Runtime evidence, when the codec actually executes, is written to C:\\windows\\temp\\hmos-oodle-native.json. nativeCalls, nativeBytes, fallbackCalls and nativeFailures are counters, not a speed measurement. If the original hash is different, the ordinary game DLL is used.

The test HAP was built from the existing sigbus-batch app snapshot, not from a clean build of the launcher commit: that snapshot contains device-specific fixes already installed before this experiment. The package includes the decoder under resources/resfile/sekiro-codec/{aarch64-windows,x86_64-windows} and the license notices under resources/rawfile/sekiro-codec-notices. The source files under ohos/app reproduce the two changed ArkTS files from that snapshot; the other snapshot changes are not represented by the launcher commit.

# OHOS x64 trampoline allocation diagnostic

This diagnostic inspects the loaded ntdll x64 export addresses and uses MinHook
1.3.4 to **construct** (but never enable) trampolines for seven exports. It also
tests explicit instruction-cache invalidation on a private code allocation. It
does not modify game files or install system API hooks.

## Observed failure

The v1 and v2 device reports are in `evidence/`. Both were collected before the
server address-range fix.

- The loaded ntdll image is at `0x7fffe40000`. Export-table addresses and
  `GetProcAddress` agree. The entry points contain valid, executable x64 code.
- All seven trampoline constructions fail with `MH_ERROR_MEMORY_ALLOC` (9),
  Windows error `ERROR_INVALID_ADDRESS` (487).
- The image immediately below ntdll starts at `0x7fbc2c0000`, matching the UBT
  image size `0x43b72000`. It fills the backward search range.
- Wine reports memory starting at `0x7fffff0000` as free, but fixed allocations
  there and above fail. `GetSystemInfo` still advertises the 47-bit Windows limit.
- The private code test passes: return value 11 changes to 22 after
  `FlushInstructionCache`.
- The seven construction attempts make 114,604 failed allocation calls.

These observations locate a real compatibility failure in nearby allocation.
They do not yet prove that fixing it resolves every initialization step in the
game's `steam_api64.dll`.

## Comparison and change

VintagePomeloPro's ARM64 Wine fork probes the host-reachable high image range:
[winehua/wine server/mapping.c](https://github.com/winehua/wine/blob/dc5204ecb0c3c1bf6624542ecf42f931fcc1a3b0/server/mapping.c#L276).
Our baseline always registers the range `0x600000000000..0x700000000000`, which is
unreachable on the observed host. The client then falls back to tightly packing
images at the top of its host address space, bypassing the server's inter-image
gaps. With the large UBT image immediately below ntdll, this leaves no space for
nearby trampolines.

`server/mapping.c` now probes successively lower ranges on OHOS ARM64, using
non-fixed anonymous mapping hints. Probe mappings are immediately released;
existing host mappings are never overwritten. Unlike the reference helper, a
stale `errno` cannot classify a relocated mapping as success. Other platforms
retain the original initializer. This change does not alter UBT, the game DLL,
or the Windows-reported address-space limit.

## Checks and build

Run `python3 tools/ohos-thunk-probe/check-high-map.py`. It extracts the real server
initializer, demonstrates failure of the baseline on a simulated 39-bit host,
and checks the fix against 32/36/39/42/47/48-bit limits, including relocated hints
with stale `errno`. It also checks that the non-OHOS initializer is unchanged.
All these checks passed on 2026-11-11.

`build-server-replacement.py` compiles the modified mapping object and links it
with snapshots of the current server objects. It writes only to an isolated
container directory and this worktree's `.build-cache/thunk-fix/`; the active
build tree is read-only to the script. Build provenance is saved in
`build-manifest.json` there.

The signed test HAP was built from an isolated snapshot of the current app.
Comparison with the baseline HAP found only two changed entries:
`libs/arm64-v8a/libwineserver.so` and `ets/sourceMaps.map`. Every other native
library, Windows module, and application bytecode entry is byte-identical.
See `package-manifest.json` for hashes. Installation succeeded.

Device validation after installation (`evidence/probe-v2-after-range.txt`):
all seven constructions return `MH_OK`; there are now seven allocation calls
instead of 114,604. The ntdll image is at `0x6fffe40000`, with an allocatable
64-KiB gap below it. Silksong itself still reports initialization error 0x0A.

The separate v3 lifecycle test (`probe-enable.c`, built with
`-DPROBE_ENABLE_HOOKS`) temporarily installs passthrough hooks **in its own
process only**. All seven hooks enable, execute through the detour (hit counter
0 to 1), disable, and restore the original bytes. API results before/during/after
match. A worker thread is present to exercise thread enumeration and suspension.
See `evidence/probe-v3-after-range.txt`. `LastError=18` on successful calls is
stale state from thread enumeration, not evidence of a failed API call.

The v3 report also shows `PAGE_WRITECOPY` as the previous protection when
restoring a page that was requested as `PAGE_EXECUTE_READWRITE`. V4
(`probe-memory.c`) investigates this separately, including a read-only query of
the running game's DLL pages. It performs self-modification only on its own
diagnostic code and never enables the hooks it constructs.

V4 reproduced the permission problem (`evidence/probe-v4-after-range.txt`):
an image page changed to executable/read-write becomes `PAGE_READONLY` before
the next write, then `PAGE_WRITECOPY` after writing. A private executable/RW
allocation similarly becomes read-only after its first execution and read/write
after modification. Its code executes correctly (7, then 9 after modification),
but MinHook rejects these pages with `MH_ERROR_NOT_EXECUTABLE`. Restoring the
image page to executable/read-only makes construction pass again. No running
game was found (`GAME_MATCHES=0`), so this report proves an independent Wine/UBT
compatibility defect, not yet the remaining game's failure branch.

## Experimental guest/host protection separation

The installed UBT calls `NtProtectVirtualMemory` from translation-cache code to
protect and unprotect guest pages for SMC detection. Disassembly of its
`tc_Cache::addTrace` and `win_NotifyMemoryProtect` shows these internal calls
bracketed by stores of 1 and 0 to `ChpeV2CpuAreaInfo->EmulatorData[1]`.

The experimental `virtual.c` change recognizes this private ABI only when
`WINE_UBT_HOST_PROTECT=1` is set. For an ARM64EC process with equal host/guest page
sizes, it applies these internal RO/RW requests to the host while retaining
the guest metadata, and only when every affected guest page was executable and
writable already. Guarded pages, write-watch mappings, system mappings, other
protection modes, and ordinary guest requests use the original path. Host
`mprotect` errors remain errors. The launcher enables the option for the bundled
UBT; this opt-in must not be carried over to an unrelated emulator or an
unverified UBT private ABI.

`check-ubt-protection.py` passed the scope, mixed-permission, and failure-path
checks. The native build and HAP build passed (the native source still has the
pre-existing shared-data initialization warning). The isolated build applies
this patch on top of the existing tested `virtual.c` repairs. Package comparison
against the range-only build finds changes only in `ntdll.so`, launcher ArkTS
bytecode, and source maps. Previous native repairs are retained. The original
range-only signed HAP is saved for rollback in `.build-cache/thunk-fix/`.
The unchanged v4 binary passed after installation
(`evidence/probe-v4-after-protection.txt`): all 12 construction phases return
`MH_OK` (before: 8 of 12 returned `MH_ERROR_NOT_EXECUTABLE`). The image RWX
stages retain `PAGE_EXECUTE_WRITECOPY` (0x80), private RWX stages retain
`PAGE_EXECUTE_READWRITE` (0x40), and all four executed results match 7/9 as
expected. Restoring the image page reports the original executable permission.
This confirms the independent guest-permission regression is fixed on the
device. The subsequent Silksong launch remains to be verified.

A standalone native shell probe could not execute on this device (permission
denied), so it is not counted as a successful hardware check.

## Rebuild the Windows diagnostic

From this directory:

```sh
x86_64-w64-mingw32-gcc -O2 -Wall -Wextra -static -mwindows probe.c \
  vendor/minhook/src/hook.c vendor/minhook/src/buffer.c \
  vendor/minhook/src/trampoline.c vendor/minhook/src/hde/hde64.c \
  -o wine-thunk-probe2.exe
```

The v2 report is written to `C:\wine-thunk-probe.log`; v3/v4 use
`C:\wine-thunk-probe3.log` and `C:\wine-thunk-probe4.log`. The vendored MinHook
files retain their original BSD licenses. `buffer.c` has diagnostic allocation
wrappers; `hook.c` has opt-in protection/flush wrappers for the v3 test.

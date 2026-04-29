# Wine OHOS Codebase Guide for AI Agents

This document provides guidance for AI coding agents working in the Wine OHOS codebase.

## Project Overview

Wine is a compatibility layer for running Microsoft Windows programs on Unix-like systems. It implements Windows API calls using Unix/X11/Mac equivalents. This is a C codebase with autoconf/automake build system.

**Fork Context**: This is an OHOS (OpenHarmony) port of Wine 10.12, targeting aarch64-linux-ohos platform. The project adds an OHOS adapter layer that translates Windows API calls to OHOS native APIs.

**Key Statistics**:
- 717 DLL implementations (dlls/)
- 115 Windows programs (programs/)
- 53 OHOS adapter modules (ohos/adapter/)
- 743 compiled Windows DLLs
- 30 compiled Unix shared libraries
- Total output: 814MB

## Build Commands

### OHOS Build (Primary)
```bash
./build.sh            # Full build for OHOS
./build.sh --no-cache # Clean rebuild (removes all cache)
./build.sh --help     # Show help
```

**Build Flow**:
1. Extract OHOS SDK (if not already)
2. Build FreeType font library
3. Build x86 Wine tools (nativeBuild/, only first time)
4. Build OHOS adapter layer (libadapter.so)
5. Cross-compile Wine for ARM64 (build/)
6. Install to out/ directory
7. Copy products to ohos/app/wine_engine/

**Build Time** (16-core CPU):
- First build: ~27 minutes
- Incremental: ~10 minutes
- Adapter only: ~1 minute

### Standard Build (Linux, for reference)
```bash
./configure
make
make install
```

### Configuration Options
```bash
./configure --help                          # Show all options
./configure --without-x                     # No X11 support (OHOS default)
./configure --enable-win64                  # 64-bit only
./configure --disable-tests                 # Skip building tests
./configure --enable-werror                 # Treat warnings as errors
./configure --host=aarch64-linux-ohos       # Target platform
./configure --with-wine-tools=nativeBuild   # Cross-compilation tools
```

### Incremental Build Commands
```bash
# Rebuild adapter only
cd ohos/adapter/build
cmake --build .
cp libadapter.so ../lib/

# Rebuild Wine base
cd build
make -j$(nproc)
make install-lib

# Rebuild single DLL
cd build
make dlls/kernel32
make install-lib

# Copy products to app
cp out/lib/wine/aarch64-unix/*.so ohos/app/wine_engine/libs/arm64-v8a/
cp -r out/lib/wine/aarch64-windows ohos/app/wine_engine/src/main/resources/resfile/lib/wine/
```

## Test Commands

Tests are in `dlls/*/tests/` and `programs/*/tests/` directories.

**Run all tests:**
```bash
make test
```

**Run a single test module:**
```bash
wine dlls/kernel32/tests/kernel32_test.exe
tools/runtest -p kernel32_test.exe dlls/kernel32/tests/process.c
```

**Run a specific test function:**
```bash
wine kernel32_test.exe process
```

**Test environment variables:**
```bash
WINETEST_DEBUG=1              # Show test output (default)
WINETEST_DEBUG=0              # Quiet mode
WINETEST_REPORT_SUCCESS=1     # Show passing tests
WINETEST_INTERACTIVE=1        # Run more tests
WINETEST_PLATFORM=wine        # Mark as Wine platform
```

### Test Framework Macros (wine/test.h)
```c
START_TEST(name)              // Define test entry point
ok(condition, "msg", ...)     // Assert condition is true
skip("msg", ...)              // Skip remaining tests
win_skip("msg", ...)          // Skip on Windows, fail on Wine
trace("msg", ...)             // Debug output
todo_wine { ... }             // Block expected to fail on Wine
flaky { ... }                 // Block that may occasionally fail
broken(condition)             // True on Windows if condition is true
```

## Directory Structure

```
/home/ubuntu/wine/wine/
├── dlls/                   - Windows DLL implementations (717 dirs)
│   ├── kernel32/           - Kernel32.dll (process, thread, memory, file)
│   ├── user32/             - User32.dll (window, message, input)
│   ├── gdi32/              - GDI32.dll (graphics, drawing, fonts)
│   ├── advapi32/           - Advapi32.dll (registry, security, services)
│   ├── ntdll/              - NTdll.dll (NT API底层)
│   ├── shell32/            - Shell32.dll (shell, icons)
│   ├── ole32/              - OLE32.dll (COM/OLE)
│   ├── d3d11/              - Direct3D 11
│   ├── dxgi/               - DirectX Graphics Infrastructure
│   └── */tests/            - Unit tests for each DLL
│
├── programs/               - Windows programs (115 dirs)
│   ├── winecfg/            - Wine configuration
│   ├── notepad/            - Notepad
│   ├── regedit/            - Registry editor
│   ├── winefile/           - File manager
│   └── */tests/            - Unit tests
│
├── server/                 - Wine server (process, thread, memory, window)
├── loader/                 - Wine program loader
├── libs/                   - Third-party libraries (31 libs)
├── tools/                  - Build tools (widl, winebuild, wrc, runtest)
│
├── ohos/                   - OHOS-specific code
│   ├── adapter/            - OHOS adapter layer (53 modules)
│   │   ├── ohos_adapter.cc - Main adapter implementation
│   │   ├── native_initializer.cc - Initialization
│   │   ├── window/         - Window adapter
│   │   ├── xcomponent/     - XComponent adapter
│   │   ├── ime_adapter/    - Input method adapter
│   │   ├── file_manager/   - File manager adapter
│   │   ├── screen/         - Screen adapter
│   │   ├── battery/        - Battery adapter
│   │   ├── bluetooth/      - Bluetooth adapter
│   │   ├── geolocation/    - Geolocation adapter
│   │   ├── media_manager/  - Media adapter
│   │   ├── net/            - Network adapter
│   │   ├── permission_manager/ - Permission adapter
│   │   ├── multiprocess/   - Multiprocess support
│   │   ├── lib/            - Compiled libadapter.so (4.5MB)
│   │   └── include/        - Header files
│   │
│   └── app/                - OHOS application (HAP工程)
│   │   ├── wine_engine/    - Wine engine module (HAR)
│   │   │   ├── libs/arm64-v8a/ - Compiled .so libraries (30)
│   │   │   └── src/main/resources/resfile/lib/wine/ - Windows DLLs (743)
│   │   ├── entry/          - Application entry (HAP)
│   │   └── AppScope/       - Application config
│
├── ohos_sdk/               - OHOS SDK (鸿蒙化平台编译工具链)
│   ├── commandline-tools-linux-x64-6.0.2.650.zip - SDK压缩包 (2.1GB)
│   └── command-line-tools/ - 解压后的SDK
│       ├── bin/            - 命令行工具
│       │   ├── hvigorw     - HAP工程编译工具 (主要)
│       │   ├── ohpm        - OHOS包管理器
│       │   ├── hstack      - 堆栈分析工具
│       │   └── codelinter  - 代码检查工具
│       ├── hvigor/bin/     - Hvigor构建系统
│       ├── tool/node/bin/  - Node.js运行环境
│       └── sdk/default/openharmony/native/
│           ├── llvm/bin/   - LLVM/Clang工具链 (clang, ld.lld等)
│           ├── build/cmake/ohos.toolchain.cmake - OHOS工具链文件
│           └── build-tools/cmake/bin/ - CMake, Ninja
│
├── third_party/            - Third-party dependencies
│   ├── freetype/           - FreeType font library
│   ├── aki/                - AKI framework
│   └── gstreamer/          - GStreamer (optional)
│
├── nativeBuild/            - x86 Wine tools for cross-compilation
│   └── tools/
│       ├── winebuild/      - Wine build tool
│       ├── widl/           - IDL compiler
│       └── wrc/            - Resource compiler
│
├── build/                  - ARM Wine build directory
├── out/                    - Final build products (814MB)
│   ├── bin/                - Wine programs (12)
│   ├── lib/wine/
│   │   ├── aarch64-unix/   - Unix shared libraries (30)
│   │   └── aarch64-windows/ - Windows DLLs (743)
│   └── share/              - Resources
│
├── include/                - Windows API headers
│   └── wine/               - Wine-specific headers
│       ├── debug.h         - Debug system
│       ├── test.h          - Test framework
│       ├── list.h          - Linked list
│       ├── heap.h          - Heap allocation
│       └── server.h        - Server interface
│
├── build.sh                - Main build script
└── AGENTS.md               - This file
```

## Build Products

### out/lib/wine/aarch64-unix/ (30 Unix shared libraries)

**Core libraries**:
| Library | Size | Function |
|---------|------|----------|
| libadapter.so | 4.5MB | OHOS adapter layer |
| libwineserver.so | 1.0MB | Wine server |
| libwine.so | 15KB | Wine core interface |
| libfreetype.so | 890KB | FreeType font rendering |
| ntdll.so | 893KB | Windows NT API底层实现 |
| opengl32.so | 2.8MB | OpenGL implementation |

**System libraries**: mountmgr.so, netapi32.so, dnsapi.so, bcrypt.so, crypt32.so, etc.

### out/lib/wine/aarch64-windows/ (743 Windows DLLs)

**Core system DLLs**: kernel32.dll, user32.dll, gdi32.dll, advapi32.dll, shell32.dll, ole32.dll, ntdll.dll

**Graphics/Game DLLs**: d3d11.dll, d3d9.dll, dxgi.dll, opengl32.dll, dwrite.dll, d2d1.dll

**Network DLLs**: winhttp.dll, wininet.dll, urlmon.dll, netapi32.dll, iphlpapi.dll

**Crypto DLLs**: bcrypt.dll, crypt32.dll, rsaenh.dll, schannel.dll

**Runtime DLLs**: msvcrt.dll, msvcp140.dll, ucrtbase.dll, vcruntime140.dll

**ATL/MFC DLLs**: atl.dll, atl100.dll, mfc42.dll

**Drivers (.sys)**: winebus.sys, winehid.sys, wineusb.sys, mountmgr.sys

### out/bin/ (12 Wine programs)

wine, wineboot, winecfg, wineconsole, winedbg, winefile, winemine, winepath, notepad, regedit, regsvr32, msiexec

## HAP Build Commands

HAP (HarmonyOS Ability Package) is the OHOS application package format. The Wine OHOS project needs to package build products into HAP to run on OHOS devices.

**Build Tool**: `hvigorw` (Hvigor build system, located in `ohos_sdk/command-line-tools/bin/hvigorw`)

**HAP Project Directory**: `ohos/app/`

### Build HAP Package

```bash
# Navigate to OHOS app directory
cd /home/ubuntu/wine/wine/ohos/app

# Set environment variables
export PATH=$PATH:/home/ubuntu/wine/wine/ohos_sdk/command-line-tools/bin
export DEVECO_SDK_HOME=/home/ubuntu/wine/wine/ohos_sdk/command-line-tools/sdk
export DEVECO_NODE_HOME=/home/ubuntu/wine/wine/ohos_sdk/command-line-tools/tool/node

# Build HAP package
hvigorw --mode module -p product=default assembleHap --analyze=normal --parallel --incremental --daemon
```

### HAP Build Parameters

| Parameter | Description |
|-----------|-------------|
| `--mode module` | Module mode build (build single module) |
| `--mode project` | Project mode build (build entire project) |
| `-p product=default` | Use default product configuration |
| `assembleHap` | Build HAP package (main target) |
| `assembleHar` | Build HAR package (shared library) |
| `--analyze=normal` | Normal analysis mode |
| `--parallel` | Enable parallel build |
| `--incremental` | Enable incremental build |
| `--daemon` | Enable background daemon (speed up subsequent builds) |

### HAP Output Location

```
ohos/app/entry/build/default/outputs/default/
├── entry-default-signed.hap    # Signed HAP package (installable)
└── entry-default-unsigned.hap  # Unsigned HAP package
```

### Incremental HAP Build

```bash
# Incremental build (modules already compiled)
hvigorw --mode module -p product=default assembleHap --incremental --daemon

# Clean and rebuild
hvigorw clean
hvigorw --mode module -p product=default assembleHap --parallel

# Build specific module
hvigorw --mode module -p module=wine_engine@default assembleHar
hvigorw --mode module -p module=entry@default assembleHap
```

### HAP Package Contents

The HAP package contains:
- Wine engine module (HAR): `wine_engine.har`
  - Unix shared libraries (30 .so files in libs/arm64-v8a/)
  - libadapter.so (4.5MB), libwineserver.so (1MB), etc.
- Entry module (HAP): `entry/`
  - Windows DLLs (743 in resources/resfile/lib/wine/aarch64-windows/)
  - Application resources and code

### Install HAP on OHOS Device

```bash
# Install using hdc tool (requires OHOS device connected)
hdc install ohos/app/entry/build/default/outputs/default/entry-default-signed.hap

# Or use DevEco Studio IDE
# Open project in DevEco Studio and click Run
```

## Code Style Guidelines

### Formatting (.editorconfig)
```
[*]
indent_style = space
indent_size = 4
tab_width = 8
charset = utf-8
max_line_length = 100

[Makefile*]
indent_style = tab
indent_size = 8
```

### File Headers

All source files must have LGPL license header:
```c
/*
 * Module description
 *
 * Copyright YYYY Author Name <email@example.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */
```

### Include Order

1. Standard C headers (`<stdlib.h>`, `<stdarg.h>`, `<stdio.h>`)
2. Windows/NT status headers (`ntstatus.h`, then `#define WIN32_NO_STATUS`)
3. Windows API headers (`windef.h`, `winbase.h`, `winreg.h`, etc.)
4. Wine internal headers (`wine/debug.h`, `wine/list.h`, etc.)

```c
#include <stdlib.h>
#include <stdarg.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"
#include "winreg.h"

#include "wine/debug.h"
#include "wine/list.h"
```

### Naming Conventions

- **Functions**: lowercase with underscores (`RegCreateKeyW`, `get_process_info`)
- **Windows API wrappers**: Follow Microsoft naming (`CreateFileW`, `RegOpenKeyW`)
- **Internal functions**: Descriptive lowercase (`init_registry`, `parse_inf_file`)
- **Types**: CamelCase for structs, uppercase for handles (`HKEY`, `HWND`)
- **Macros/Constants**: UPPERCASE (`ERROR_SUCCESS`, `MAX_PATH`)
- **Variables**: lowercase, descriptive (`hkey`, `lpSubKey`, `buffer_size`)

### Debug Logging

```c
WINE_DEFAULT_DEBUG_CHANNEL(reg);    // Declare at file start

TRACE("operation: %s\n", debugstr_w(name));   // Normal trace
WARN("unexpected behavior\n");                 // Warning
ERR("failed with error %d\n", GetLastError()); // Error
FIXME("not implemented\n");                    // Missing feature
```

Debug helpers:
```c
debugstr_a(str)     // ASCII string debug output
debugstr_w(str)     // Wide string debug output
wine_dbg_sprintf()  // Format debug string
```

### Error Handling

- Windows API functions: Return error codes (`ERROR_SUCCESS`, `ERROR_INVALID_PARAMETER`)
- NT functions: Return `NTSTATUS` values
- Use `SetLastError()` before returning failure
- Check parameters at function start:

```c
LSTATUS WINAPI RegCreateKeyW(HKEY hkey, LPCWSTR lpSubKey, PHKEY phkResult)
{
    if (!phkResult)
        return ERROR_INVALID_PARAMETER;
    // ...
}
```

## OHOS Adapter Development

### Adapter Module Structure

Each adapter module in `ohos/adapter/` follows this pattern:

```cpp
// ohos/adapter/module_name/module_adapter.cc

#include "napi/native_api.h"
#include "module_adapter.h"

EXTERN_C_START

// Exported functions for Wine
napi_value CreateModuleAdapter(napi_env env, napi_value exports)
{
    // Initialize module
    // Register callback functions
    // Export to Wine
    return exports;
}

EXTERN_C_END
```

### Key Adapter Interfaces

```cpp
// Window features (ohos/adapter/ohos_adapter.cc)
typedef enum {
    WINDOW_TITLE_BAR,
    WINDOW_CLOSE_BUTTON,
    WINDOW_MINIMIZE_BUTTON,
    WINDOW_MAXIMIZE_BUTTON,
    WINDOW_RESIZABLE,
    WINDOW_TOUCHABLE,
    WINDOW_HAS_SHADOW,
    WINDOW_NO_ACTIVATION,
    USE_FLOATING_WINDOW,
    USE_DARK_MODE
} WindowFeatures;

// Window state
typedef enum {
    WINDOW_DISABLED,
    WINDOW_NO_FOREGROUND,
    WINDOW_TOP_MOST,
    WINDOW_EXCLUDED_BY_CONTROL,
    WINDOW_MINIMIZED
} WindowState;
```

### OHOS CMake Build

```cmake
# ohos/adapter/CMakeLists.txt
cmake_minimum_required(VERSION 3.16)
project(adapter)

set(CMAKE_CXX_STANDARD 20)
set(ADAPTER_SRC
    native_initializer.cc
    ohos_adapter.cc
)
add_library(adapter SHARED ${ADAPTER_SRC})
target_include_directories(adapter PUBLIC ../../ ./include/)

add_subdirectory(../../third_party/aki aki)
add_subdirectory(window)
add_subdirectory(xcomponent)
# ... 53 modules

target_link_libraries(adapter PUBLIC 
    aki_jsbind 
    ohcommonevent 
    hitrace_ndk.z 
    native_window 
    # ... other OHOS libs
)

target_compile_definitions(adapter
    PRIVATE
        USE_LOADER_TARGET=2
        LIBADAPTER_SOURCE
        ENABLE_START_ISOLATE_PROCESS)
```

### OHOS Application Structure

```
ohos/app/wine_engine/
├── Index.ets           - Entry point
├── BuildProfile.ets    - Build configuration
├── childProcess.ets    - Child process handling
├── build-profile.json5 - Build parameters
├── oh-package.json5    - Package config
├── hvigorfile.ts       - Hvigor build script
│
├── libs/arm64-v8a/     - Compiled .so libraries (30)
│   ├── libadapter.so   - OHOS adapter (4.5MB)
│   ├── libwineserver.so - Wine server (1MB)
│   └── ...
│
└── src/main/
    ├── cpp/            - C++ source
    ├── ets/            - ArkTS source
    └── resources/resfile/
        └── lib/wine/
            └── aarch64-windows/ - Windows DLLs (743)
```

## Wine DLL Development

### Makefile.in Format

```makefile
MODULE    = advapi32.dll      # Target module name
IMPORTLIB = advapi32          # Import library name
IMPORTS   = kernelbase sechost msvcrt  # Dependencies
DELAYIMPORTS = cryptsp rpcrt4 user32  # Delay-loaded DLLs

SOURCES = \
    advapi.c \
    registry.c \
    security.c \
    service.c
```

### Spec Files

DLL exports are defined in `.spec` files:

```
# Standard export with calling convention
stdcall RegOpenKeyW(long wstr ptr)

# Export with ordinal number
100 stdcall RegCloseKey(long)

# Forward to another DLL
stdcall GetName(long) krnl386.exe16.GetName

# Variable argument function
varargs wsprintfA(str str)

# Special attributes
stdcall -private -norelay VxDCall0(long)
```

Calling conventions:
- `stdcall` - Windows standard (__stdcall)
- `cdecl` - C calling convention
- `fastcall` - Fast call
- `varargs` - Variable arguments

## Common Patterns

### Function Pointer Initialization

```c
static HRESULT (WINAPI *pSomeFunction)(void);

static void init_function_pointers(void)
{
    HMODULE hmod = GetModuleHandleA("some.dll");
    pSomeFunction = (void *)GetProcAddress(hmod, "SomeFunction");
}
```

### Heap Allocation (wine/heap.h)

```c
#include "wine/heap.h"

void *buffer = heap_alloc(size);
void *buffer = heap_alloc_zero(size);
char *str = heap_strdupA("string");
WCHAR *str = heap_strdupW(L"string");
heap_free(buffer);
```

### Critical Sections

```c
static CRITICAL_SECTION cs;
static CRITICAL_SECTION_DEBUG cs_debug =
{
    0, 0, &cs,
    { &cs_debug.ProcessLocksList, &cs_debug.ProcessLocksList },
    0, 0, { (DWORD_PTR)(__FILE__ ": cs") }
};
static CRITICAL_SECTION cs = { &cs_debug, -1, 0, 0, 0, 0 };

// Usage
EnterCriticalSection(&cs);
// ... critical code ...
LeaveCriticalSection(&cs);
```

### Window Procedure

```c
static LRESULT WINAPI window_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
        // Handle creation
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}
```

## Architecture Overview

```
┌─────────────────────────────────────────────────┐
│           Windows Application                     │
└─────────────────────────────────────────────────┘
                    ↓ Calls Windows API
┌─────────────────────────────────────────────────┐
│           Wine DLL Implementation Layer           │
│  kernel32.dll, user32.dll, gdi32.dll, etc.       │
│  (743 Windows DLLs)                              │
└─────────────────────────────────────────────────┘
                    ↓ Calls Wine Unix APIs
┌─────────────────────────────────────────────────┐
│           Wine Unix Layer                         │
│  ntdll.so, libwine.so, opengl32.so, etc.         │
│  (30 Unix shared libraries)                       │
└─────────────────────────────────────────────────┘
                    ↓ Through libadapter.so
┌─────────────────────────────────────────────────┐
│           OHOS Adapter Layer                      │
│  libadapter.so (4.5MB)                            │
│  - Window, IME, File, Device, Network, etc.       │
│  (53 adapter modules)                             │
└─────────────────────────────────────────────────┘
                    ↓ Calls OHOS Native APIs
┌─────────────────────────────────────────────────┐
│           OHOS System                             │
│  - Window service, IME service, File service      │
│  - Device service, Network service, etc.          │
└─────────────────────────────────────────────────┘
```

## Cross-Compilation Environment

### OHOS SDK Toolchain

The OHOS SDK provides a complete toolchain for cross-compiling to aarch64-linux-ohos:

**Location**: `ohos_sdk/command-line-tools/`

**Key Components**:
| Component | Path | Description |
|-----------|------|-------------|
| LLVM/Clang | sdk/default/openharmony/native/llvm/bin/ | C/C++ compiler toolchain |
| CMake | sdk/default/openharmony/native/build-tools/cmake/bin/ | Build system |
| Ninja | sdk/default/openharmony/native/build-tools/cmake/bin/ninja | Fast build tool |
| Node.js | tool/node/bin/ | JavaScript runtime for Hvigor |
| Hvigorw | bin/hvigorw | HAP build tool wrapper |
| ohpm | bin/ohpm | OHOS package manager |

### Build Variables (from build.sh)

```bash
# OHOS SDK paths
OHOS_SDK=ohos_sdk/command-line-tools/sdk/default/openharmony
CMAKE_PATH=${OHOS_SDK}/native/build-tools/cmake/bin/cmake
NINJA_PATH=${OHOS_SDK}/native/build-tools/cmake/bin/ninja
TOOLCHAIN_FILE=${OHOS_SDK}/native/build/cmake/ohos.toolchain.cmake

# Cross-compilation toolchain
export AS=${OHOS_SDK}/native/llvm/bin/llvm-as
export CC="${OHOS_SDK}/native/llvm/bin/clang --target=aarch64-linux-ohos"
export CXX="${OHOS_SDK}/native/llvm/bin/clang++ --target=aarch64-linux-ohos"
export LD=${OHOS_SDK}/native/llvm/bin/ld.lld
export STRIP=${OHOS_SDK}/native/llvm/bin/llvm-strip
export AR=${OHOS_SDK}/native/llvm/bin/llvm-ar
export NM=${OHOS_SDK}/native/llvm/bin/llvm-nm
export RANLIB=${OHOS_SDK}/native/llvm/bin/llvm-ranlib
export OBJDUMP=${OHOS_SDK}/native/llvm/bin/llvm-objdump
export OBJCOPY=${OHOS_SDK}/native/llvm/bin/llvm-objcopy

# Compilation flags
export CFLAGS="-fPIC -D__MUSL__=1"
export CXXFLAGS="-fPIC -D__MUSL__=1"
export CPPFLAGS="-I${ADAPTER_DIR}/include -I${FREETYPE_PREFIX}/include/freetype2"
export LDFLAGS="-L${ADAPTER_DIR}/lib -L${FREETYPE_PREFIX}/lib"

# Build arguments
buildargs="--without-x --without-dlltool --host=aarch64-linux-ohos --with-wine-tools=nativeBuild"

# HAP build environment
export PATH=$PATH:/home/ubuntu/wine/wine/ohos_sdk/command-line-tools/bin
export DEVECO_SDK_HOME=/home/ubuntu/wine/wine/ohos_sdk/command-line-tools/sdk
export DEVECO_NODE_HOME=/home/ubuntu/wine/wine/ohos_sdk/command-line-tools/tool/node
```

### Target Platform

- **Host**: x86_64-pc-linux-gnu (build machine)
- **Target**: aarch64-linux-ohos (OHOS ARM64)
- **C Library**: musl (not glibc)
- **Position Independent Code**: Required (-fPIC)
- **Compiler**: LLVM Clang 15 (from OHOS SDK)
- **Build System**: Hvigor (for HAP), CMake (for adapter)

## Documentation

Detailed documentation available in `/home/ubuntu/wine/docs/`:

| Document | Content |
|----------|---------|
| project_structure.md | Full project structure analysis |
| build_output.md | Build products analysis (743 DLLs, 30 SOs) |
| build_process.md | Build process step-by-step |
| README.md | Documentation index |

## Submitting Changes

Follow guidelines at: https://gitlab.winehq.org/wine/wine/-/wikis/Submitting-Patches

- Patches should be self-contained
- Include proper license headers
- Add tests for new functionality
- Reference bug reports if applicable
- For OHOS-specific changes, also update ohos/adapter/

## Version Information

- **Wine Version**: 10.12
- **OHOS SDK Version**: 6.0.2.650
- **Target Architecture**: aarch64-linux-ohos
- **Build Host**: x86_64-pc-linux-gnu
- **Cross-Compiler**: LLVM/Clang (OHOS SDK)
- **Build Products Size**: 814MB
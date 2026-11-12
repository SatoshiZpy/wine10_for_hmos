// stdafx.h : includefile for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once



#define _CRT_SECURE_NO_WARNINGS 1

#include <stdio.h>
#include <tchar.h>
#include <stdlib.h>
#include <string.h>

#define SSE2NEON_ALLOC_DEFINED 1
#define __aarch64__ 1
#include "sse2neon.h"
#undef __aarch64__
#include <assert.h>
#include <windows.h>

#pragma warning (disable: 4244)

// TODO: reference additional headers your program requires here
typedef unsigned char byte;
typedef unsigned char uint8;
typedef unsigned int uint32;
typedef unsigned __int64 uint64;
typedef signed __int64 int64;
typedef signed int int32;
typedef unsigned short uint16;
typedef signed short int16;
typedef unsigned int uint;

#define _byteswap_uint64 __builtin_bswap64
#define _byteswap_ulong __builtin_bswap32
#define _byteswap_ushort __builtin_bswap16

// GPL-3.0-or-later: native ARM64EC adapter for the unmodified ooZ decoder.
#include "stdafx.h"
#include <stdint.h>
#include <limits.h>
extern int Kraken_Decompress(const byte*, size_t, byte*, size_t);
using Decode = int64_t (__cdecl *)(void*,int64_t,void*,int64_t,int,int,int,void*,int64_t,void*,void*,void*,int64_t,int);
static volatile LONG64 nativeCalls=0,nativeBytes=0,fallbackCalls=0,nativeFailures=0;
static void Report(){
 char text[512];int n=snprintf(text,sizeof(text),"{\"codec\":\"ooZ ARM64EC\",\"nativeCalls\":%lld,\"nativeBytes\":%lld,\"fallbackCalls\":%lld,\"nativeFailures\":%lld,\"pid\":%lu}\n",(long long)InterlockedCompareExchange64(&nativeCalls,0,0),(long long)InterlockedCompareExchange64(&nativeBytes,0,0),(long long)InterlockedCompareExchange64(&fallbackCalls,0,0),(long long)InterlockedCompareExchange64(&nativeFailures,0,0),(unsigned long)GetCurrentProcessId());
 HANDLE f=CreateFileW(L"C:\\windows\\temp\\hmos-oodle-native.json",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
 if(f!=INVALID_HANDLE_VALUE){DWORD bytes;WriteFile(f,text,n,&bytes,NULL);CloseHandle(f);}
}
extern "C" __declspec(dllexport) int64_t OodleLZ_Decompress(void* src,int64_t comp,void* dst,int64_t raw,int safe,int crc,int verbosity,void* base,int64_t size,void* callback,void* userdata,void* memory,int64_t memsize,int phase){
 // Accelerate complete, independent Kraken streams only. Dictionary, callback,
 // CRC, phased and other codec requests retain the original implementation.
 if(src&&dst&&comp>=2&&comp<=0x40000000&&raw>0&&raw<=0x40000000&&
    (((byte*)src)[1]&0x7f)==6&&!crc&&!callback&&!base&&!size&&phase==3){
  // ooZ explicitly requires 64 padding bytes. Keep its speculative reads and
  // writes in private buffers, never beyond the caller's supplied allocation.
  byte* input=(byte*)calloc(1,(size_t)comp+64);
  byte* output=(byte*)malloc((size_t)raw+64);
  if(input&&output){
   memcpy(input,src,(size_t)comp);
   int n=Kraken_Decompress(input,(size_t)comp,output,(size_t)raw);
   free(input);
   if(n==raw){
    memcpy(dst,output,(size_t)raw);free(output);
    InterlockedAdd64(&nativeBytes,raw);LONG64 calls=InterlockedIncrement64(&nativeCalls);
    if(calls<=4||(calls%256)==0)Report();
    return raw;
   }
   free(output);InterlockedIncrement64(&nativeFailures);
  }else{free(input);free(output);}
 }
 LONG64 calls=InterlockedIncrement64(&fallbackCalls);if(calls==1)Report();
 // This unmodified DLL comes exclusively from the existing game installation.
 HMODULE original=LoadLibraryW(L"hmos_oodle_x64.dll");
 Decode decode=original?(Decode)GetProcAddress(original,"OodleLZ_Decompress"):nullptr;
 if(!decode)return 0;
 int64_t result=decode(src,comp,dst,raw,safe,crc,verbosity,base,size,callback,userdata,memory,memsize,phase);
 FreeLibrary(original);return result;
}

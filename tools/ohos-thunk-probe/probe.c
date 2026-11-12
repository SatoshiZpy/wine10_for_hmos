/*
 * Diagnose x64 export thunks and trampoline construction under Wine OHOS.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 */
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "vendor/minhook/include/MinHook.h"
#include "vendor/minhook/src/hde/hde64.h"

static HANDLE output;
static unsigned int alloc_calls, query_calls;
static void report(const char *fmt, ...)
{
    char text[2048];
    DWORD written;
    va_list args;
    va_start(args, fmt);
    vsnprintf(text, sizeof(text), fmt, args);
    va_end(args);
    WriteFile(output, text, (DWORD)strlen(text), &written, NULL);
    FlushFileBuffers(output);
}

SIZE_T WINAPI probe_VirtualQuery(LPCVOID addr, PMEMORY_BASIC_INFORMATION info, SIZE_T size)
{
    SIZE_T result = VirtualQuery(addr, info, size);
    DWORD error = GetLastError();
    if (++query_calls <= 40)
        report("ALLOC_QUERY addr=%p ret=%llu err=%lu base=%p alloc=%p size=%llu state=%lx prot=%lx\r\n",
               addr, (unsigned long long)result, error, info->BaseAddress, info->AllocationBase,
               (unsigned long long)info->RegionSize, info->State, info->Protect);
    SetLastError(error);
    return result;
}

LPVOID WINAPI probe_VirtualAlloc(LPVOID addr, SIZE_T size, DWORD type, DWORD prot)
{
    LPVOID result = VirtualAlloc(addr, size, type, prot);
    DWORD error = GetLastError();
    if (++alloc_calls <= 20)
        report("ALLOC_TRY addr=%p size=%llu type=%lx prot=%lx ret=%p err=%lu\r\n",
               addr, (unsigned long long)size, type, prot, result, error);
    SetLastError(error);
    return result;
}

static void report_host_maps(HMODULE module)
{
    HANDLE file = CreateFileA("\\\\?\\unix\\proc\\self\\maps", GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
    char buffer[4096], line[2048];
    DWORD count, i;
    unsigned int pos = 0;
    unsigned long long low, high;
    report("HOST_MAPS handle=%p error=%lu\r\n", file, GetLastError());
    if (file == INVALID_HANDLE_VALUE) return;
    while (ReadFile(file, buffer, sizeof(buffer), &count, NULL) && count)
    {
        for (i = 0; i < count; ++i)
        {
            if (buffer[i] == '\n')
            {
                line[pos] = 0;
                if (sscanf(line, "%llx-%llx", &low, &high) == 2 &&
                    high > (ULONG_PTR)module - 0x40000000ULL &&
                    low < (ULONG_PTR)module + 0x40000000ULL)
                    report("MAP %s\r\n", line);
                pos = 0;
            }
            else if (pos < sizeof(line) - 1) line[pos++] = buffer[i];
        }
    }
    CloseHandle(file);
}

static LONG WINAPI fault(EXCEPTION_POINTERS *info)
{
    report("FAULT code=%08lx pc=%p\r\n", info->ExceptionRecord->ExceptionCode,
           info->ExceptionRecord->ExceptionAddress);
    CloseHandle(output);
    ExitProcess(2);
}

static void *export_address(HMODULE module, const char *name)
{
    BYTE *base = (BYTE *)module;
    IMAGE_DOS_HEADER *dos = (void *)base;
    IMAGE_NT_HEADERS64 *nt = (void *)(base + dos->e_lfanew);
    IMAGE_DATA_DIRECTORY dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    IMAGE_EXPORT_DIRECTORY *exp = (void *)(base + dir.VirtualAddress);
    DWORD *names = (void *)(base + exp->AddressOfNames);
    DWORD *funcs = (void *)(base + exp->AddressOfFunctions);
    WORD *ords = (void *)(base + exp->AddressOfNameOrdinals);
    DWORD i, rva;
    for (i = 0; i < exp->NumberOfNames; ++i)
    {
        if (strcmp((char *)base + names[i], name)) continue;
        rva = funcs[ords[i]];
        if (rva >= dir.VirtualAddress && rva < dir.VirtualAddress + dir.Size)
        {
            report("FORWARD %s -> %s\r\n", name, base + rva);
            return NULL;
        }
        return base + rva;
    }
    return NULL;
}

static void describe(const char *label, void *ptr)
{
    MEMORY_BASIC_INFORMATION mbi;
    BYTE *code = ptr;
    hde64s ins;
    unsigned int i, pos = 0;
    SIZE_T ret;
    memset(&mbi, 0, sizeof(mbi));
    SetLastError(0);
    ret = VirtualQuery(ptr, &mbi, sizeof(mbi));
    report("%s ptr=%p query=%llu err=%lu base=%p alloc=%p size=%llu state=%08lx prot=%08lx type=%08lx\r\n",
           label, ptr, (unsigned long long)ret, GetLastError(), mbi.BaseAddress,
           mbi.AllocationBase, (unsigned long long)mbi.RegionSize, mbi.State, mbi.Protect, mbi.Type);
    if (!ret || mbi.State != MEM_COMMIT || mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) return;
    report("bytes=");
    for (i = 0; i < 32; ++i) report("%02x", code[i]);
    report("\r\n");
    while (pos < 8)
    {
        unsigned int len = hde64_disasm(code + pos, &ins);
        report("decode offset=%u len=%u flags=%08x opcode=%02x\r\n", pos, len, ins.flags, ins.opcode);
        if (!len || ins.flags & F_ERROR) break;
        pos += len;
    }
}

static LONG WINAPI unused_detour(void *arg1, void *arg2)
{
    (void)arg1;
    (void)arg2;
    return 0;
}

static void probe_export(HMODULE module, const char *name)
{
    void *eat = export_address(module, name);
    void *api = (void *)GetProcAddress(module, name), *original = NULL;
    MH_STATUS status;
    report("\r\nEXPORT %s EAT=%p GetProcAddress=%p equal=%d\r\n", name, eat, api, eat == api);
    if (!eat) return;
    describe("EAT", eat);
    if (api && api != eat) describe("API", api);
    SetLastError(0);
    status = MH_CreateHook(eat, (void *)unused_detour, &original);
    report("CREATE_ONLY status=%d (%s) last_error=%lu trampoline=%p\r\n",
           status, MH_StatusToString(status), GetLastError(), original);
    /* Never enable these hooks: system entry instructions remain unchanged. */
    if (status == MH_OK)
    {
        describe("trampoline", original);
        report("REMOVE status=%d\r\n", MH_RemoveHook(eat));
    }
}

static void probe_smc(void)
{
    BYTE *code = VirtualAlloc(NULL, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    int (*fn)(void) = (void *)code;
    int before, after;
    unsigned int i;
    if (!code) { report("SMC alloc failed %lu\r\n", GetLastError()); return; }
    code[0] = 0xb8;
    *(DWORD *)(code + 1) = 11;
    code[5] = 0xc3;
    FlushInstructionCache(GetCurrentProcess(), code, 6);
    for (i = 0; i < 2000; ++i) fn();
    before = fn();
    *(DWORD *)(code + 1) = 22;
    FlushInstructionCache(GetCurrentProcess(), code, 6);
    after = fn();
    report("SMC explicit_flush before=%d after=%d expected=11,22\r\n", before, after);
    VirtualFree(code, 0, MEM_RELEASE);
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR command, int show)
{
    static const char *names[] = {"NtQueryAttributesFile", "NtCreateFile", "LdrGetDllHandle",
        "NtOpenFile", "NtQueryDirectoryFile", "NtQueryFullAttributesFile", "LdrLoadDll"};
    HMODULE module = GetModuleHandleA("ntdll.dll");
    char text[1024];
    IMAGE_NT_HEADERS64 *nt;
    SYSTEM_INFO system;
    MH_STATUS status;
    unsigned int i;
    (void)instance; (void)previous; (void)command; (void)show;
    output = CreateFileA("C:\\wine-thunk-probe.log", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                         NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (output == INVALID_HANDLE_VALUE) return 1;
    SetUnhandledExceptionFilter(fault);
    report("Wine x64 thunk probe v2 PID=%lu\r\n", GetCurrentProcessId());
    GetEnvironmentVariableA("WINEDLLPATH", text, sizeof(text)); report("WINEDLLPATH=%s\r\n", text);
    GetModuleFileNameA(module, text, sizeof(text)); report("ntdll=%s base=%p\r\n", text, module);
    nt = (void *)((BYTE *)module + ((IMAGE_DOS_HEADER *)module)->e_lfanew);
    report("loaded_machine=%04x size=%08lx timestamp=%08lx\r\n", nt->FileHeader.Machine,
           nt->OptionalHeader.SizeOfImage, nt->FileHeader.TimeDateStamp);
    GetSystemInfo(&system);
    report("SYSTEM min=%p max=%p granularity=%lu page=%lu\r\n", system.lpMinimumApplicationAddress,
           system.lpMaximumApplicationAddress, system.dwAllocationGranularity, system.dwPageSize);
    report_host_maps(module);
    describe("detour", (void *)unused_detour);
    status = MH_Initialize(); report("MINHOOK init=%d\r\n", status);
    if (status == MH_OK)
    {
        for (i = 0; i < sizeof(names) / sizeof(names[0]); ++i) probe_export(module, names[i]);
        MH_Uninitialize();
    }
    probe_smc();
    report("allocation_calls=%u query_calls=%u\r\n", alloc_calls, query_calls);
    report("COMPLETE\r\n");
    CloseHandle(output);
    MessageBoxA(NULL, "Diagnostic complete. Report: C:\\wine-thunk-probe.log", "Wine thunk probe", MB_OK);
    return 0;
}

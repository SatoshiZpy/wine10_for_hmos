/*
 * Diagnose x64 guest page protections and inspect a running game's mappings.
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
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */
#define WinMain construction_probe_main
#include "probe.c"
#undef WinMain
#include <tlhelp32.h>
#include <psapi.h>

/* Keep the modified code on a separate image page from the diagnostic itself. */
__attribute__((naked, noinline, section(".probe"))) static int image_code(void)
{
    __asm__("mov $7, %eax\n\tret");
}

static void inspect_code(const char *label, BYTE *code)
{
    void *target = export_address(GetModuleHandleA("ntdll.dll"), "NtQueryAttributesFile");
    void *trampoline = NULL;
    MH_STATUS status;
    report("\r\nPHASE %s\r\n", label);
    describe("code", code);
    SetLastError(0);
    status = MH_CreateHook(target, code, &trampoline);
    report("CREATE_DETOUR status=%d (%s) err=%lu\r\n", status, MH_StatusToString(status), GetLastError());
    if (status == MH_OK) MH_RemoveHook(target); /* Never enabled. */
}

static void memory_tests(void)
{
    BYTE *code = (void *)image_code;
    BYTE *private_code;
    int (*volatile fn)(void) = image_code;
    DWORD old, ignored;
    BOOL result;
    int value;

    inspect_code("image initial", code);
    value = fn(); report("EXEC value=%d expected=7\r\n", value);
    inspect_code("image warmed", code);
    SetLastError(0);
    result = VirtualProtect(code, 6, PAGE_EXECUTE_READWRITE, &old);
    report("IMAGE_RWX result=%d old=%lx err=%lu\r\n", result, old, GetLastError());
    if (result)
    {
        inspect_code("image RWX before write", code);
        *(volatile DWORD *)(code + 1) = 9;
        inspect_code("image RWX after write", code);
        FlushInstructionCache(GetCurrentProcess(), code, 6);
        inspect_code("image RWX after flush", code);
        value = fn(); report("EXEC value=%d expected=9\r\n", value);
        inspect_code("image RWX after execution", code);
        *(volatile DWORD *)(code + 1) = 7;
        FlushInstructionCache(GetCurrentProcess(), code, 6);
        result = VirtualProtect(code, 6, old, &ignored);
        report("IMAGE_RESTORE result=%d old=%lx\r\n", result, ignored);
        inspect_code("image restored", code);
    }

    private_code = VirtualAlloc(NULL, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!private_code) { report("PRIVATE_ALLOC failed=%lu\r\n", GetLastError()); return; }
    private_code[0] = 0xb8;
    *(DWORD *)(private_code + 1) = 7;
    private_code[5] = 0xc3;
    FlushInstructionCache(GetCurrentProcess(), private_code, 6);
    inspect_code("private initialized", private_code);
    fn = (void *)private_code;
    value = fn(); report("EXEC value=%d expected=7\r\n", value);
    inspect_code("private warmed", private_code);
    *(volatile DWORD *)(private_code + 1) = 9;
    inspect_code("private after write", private_code);
    FlushInstructionCache(GetCurrentProcess(), private_code, 6);
    inspect_code("private after flush", private_code);
    value = fn(); report("EXEC value=%d expected=9\r\n", value);
    inspect_code("private after execution", private_code);
    VirtualFree(private_code, 0, MEM_RELEASE);
}

static void inspect_remote_address(HANDLE process, BYTE *address)
{
    MEMORY_BASIC_INFORMATION info = {0};
    BYTE data[32];
    SIZE_T count = 0, result;
    unsigned int i;
    SetLastError(0);
    result = VirtualQueryEx(process, address, &info, sizeof(info));
    report("REMOTE_QUERY addr=%p ret=%llu err=%lu base=%p alloc=%p size=%llu state=%lx prot=%lx\r\n",
           address, (unsigned long long)result, GetLastError(), info.BaseAddress, info.AllocationBase,
           (unsigned long long)info.RegionSize, info.State, info.Protect);
    if (ReadProcessMemory(process, address, data, sizeof(data), &count))
    {
        report("REMOTE_BYTES ");
        for (i = 0; i < count; ++i) report("%02x", data[i]);
        report("\r\n");
    }
    else report("REMOTE_READ failed=%lu\r\n", GetLastError());
}

static void inspect_game(void)
{
    static const DWORD rvas[] = {0x188cc, 0x182cc, 0x1839c, 0x18428, 0x184b4, 0x18870, 0x18928};
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W entry = {.dwSize = sizeof(entry)};
    unsigned int matches = 0;
    if (snapshot == INVALID_HANDLE_VALUE) { report("PROCESS_SNAPSHOT failed=%lu\r\n", GetLastError()); return; }
    if (Process32FirstW(snapshot, &entry)) do
    {
        HANDLE process;
        HMODULE modules[1024];
        DWORD needed = 0;
        unsigned int i, j;
        if (!wcsstr(entry.szExeFile, L"Silksong")) continue;
        ++matches;
        process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, entry.th32ProcessID);
        report("\r\nGAME pid=%lu name=%ls handle=%p err=%lu\r\n",
               entry.th32ProcessID, entry.szExeFile, process, GetLastError());
        if (!process) continue;
        if (EnumProcessModulesEx(process, modules, sizeof(modules), &needed, LIST_MODULES_ALL))
        {
            for (i = 0; i < needed / sizeof(modules[0]) && i < 1024; ++i)
            {
                char name[MAX_PATH] = {0};
                GetModuleBaseNameA(process, modules[i], name, sizeof(name));
                if (!stricmp(name, "steam_api64.dll"))
                {
                    report("REMOTE_MODULE %s base=%p\r\n", name, modules[i]);
                    for (j = 0; j < sizeof(rvas) / sizeof(rvas[0]); ++j)
                        inspect_remote_address(process, (BYTE *)modules[i] + rvas[j]);
                }
                else if (!stricmp(name, "ntdll.dll") || !stricmp(name, "wintrust.dll"))
                {
                    report("REMOTE_MODULE %s base=%p\r\n", name, modules[i]);
                    inspect_remote_address(process, (BYTE *)modules[i]);
                }
            }
        }
        else report("REMOTE_ENUM failed=%lu\r\n", GetLastError());
        CloseHandle(process);
    } while (Process32NextW(snapshot, &entry));
    CloseHandle(snapshot);
    report("GAME_MATCHES=%u\r\n", matches);
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR command, int show)
{
    (void)instance; (void)previous; (void)command; (void)show;
    output = CreateFileA("C:\\wine-thunk-probe4.log", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                         NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (output == INVALID_HANDLE_VALUE) return 1;
    SetUnhandledExceptionFilter(fault);
    report("Wine x64 memory protection probe v4 PID=%lu\r\n", GetCurrentProcessId());
    inspect_game();
    report("MINHOOK init=%d\r\n", MH_Initialize());
    memory_tests();
    MH_Uninitialize();
    report("COMPLETE\r\n");
    CloseHandle(output);
    MessageBoxA(NULL, "Diagnostic complete. Report: C:\\wine-thunk-probe4.log", "Wine memory protection probe", MB_OK);
    return 0;
}

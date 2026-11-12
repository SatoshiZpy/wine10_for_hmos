/*
 * Diagnose the lifecycle of x64 passthrough hooks in this process only.
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
#include <winternl.h>

void *active_original;
volatile LONG detour_hits;

__attribute__((naked, noinline)) static void passthrough_detour(void)
{
    __asm__("lock incl detour_hits(%rip)\n\tjmp *active_original(%rip)");
}

BOOL WINAPI probe_VirtualProtect(LPVOID addr, SIZE_T size, DWORD prot, PDWORD old)
{
    BOOL result = VirtualProtect(addr, size, prot, old);
    DWORD error = GetLastError();
    report("PROTECT addr=%p size=%llu prot=%lx ret=%d old=%lx err=%lu\r\n",
           addr, (unsigned long long)size, prot, result, result ? *old : 0, error);
    SetLastError(error);
    return result;
}

BOOL WINAPI probe_FlushInstructionCache(HANDLE process, LPCVOID addr, SIZE_T size)
{
    BOOL result = FlushInstructionCache(process, addr, size);
    DWORD error = GetLastError();
    report("FLUSH addr=%p size=%llu ret=%d err=%lu\r\n",
           addr, (unsigned long long)size, result, error);
    SetLastError(error);
    return result;
}

/* Exercise the ABI without creating or modifying files. */
static NTSTATUS call_target(unsigned int index, void *target)
{
    WCHAR path[] = L"\\??\\C:\\windows";
    WCHAR dll[] = L"kernel32.dll";
    UNICODE_STRING name = {sizeof(path) - sizeof(WCHAR), sizeof(path), path};
    UNICODE_STRING dllname = {sizeof(dll) - sizeof(WCHAR), sizeof(dll), dll};
    OBJECT_ATTRIBUTES attr = {.Length = sizeof(attr), .ObjectName = &name, .Attributes = 0x40};
    IO_STATUS_BLOCK io;
    BYTE buffer[512] = {0};
    HANDLE file = NULL;
    HMODULE module = NULL;
    NTSTATUS status;

    switch (index)
    {
    case 0:
    case 5:
        return ((NTSTATUS (NTAPI *)(OBJECT_ATTRIBUTES *, void *))target)(&attr, buffer);
    case 1:
        status = ((NTSTATUS (NTAPI *)(HANDLE *, ACCESS_MASK, OBJECT_ATTRIBUTES *,
                  IO_STATUS_BLOCK *, LARGE_INTEGER *, ULONG, ULONG, ULONG, ULONG, void *, ULONG))target)
                  (&file, FILE_LIST_DIRECTORY | SYNCHRONIZE, &attr, &io, NULL, 0, 7, 1, 0x21, NULL, 0);
        if (status >= 0) CloseHandle(file);
        return status;
    case 2:
        return ((NTSTATUS (NTAPI *)(WCHAR *, ULONG *, UNICODE_STRING *, HMODULE *))target)
               (NULL, NULL, &dllname, &module);
    case 3:
        status = ((NTSTATUS (NTAPI *)(HANDLE *, ACCESS_MASK, OBJECT_ATTRIBUTES *,
                  IO_STATUS_BLOCK *, ULONG, ULONG))target)
                  (&file, FILE_LIST_DIRECTORY | SYNCHRONIZE, &attr, &io, 7, 0x21);
        if (status >= 0) CloseHandle(file);
        return status;
    case 4:
        return ((NTSTATUS (NTAPI *)(HANDLE, HANDLE, void *, void *, IO_STATUS_BLOCK *,
                  void *, ULONG, ULONG, BOOLEAN, UNICODE_STRING *, BOOLEAN))target)
                  (INVALID_HANDLE_VALUE, NULL, NULL, NULL, &io, buffer, sizeof(buffer), 1, TRUE, NULL, TRUE);
    case 6:
        status = ((NTSTATUS (NTAPI *)(WCHAR *, ULONG *, UNICODE_STRING *, HMODULE *))target)
                  (NULL, NULL, &dllname, &module);
        if (status >= 0) FreeLibrary(module);
        return status;
    }
    return (NTSTATUS)0xc000000d;
}

static HANDLE stop_event, ready_event;

static DWORD WINAPI worker(void *arg)
{
    (void)arg;
    SetEvent(ready_event);
    WaitForSingleObject(stop_event, INFINITE);
    return 0;
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR command, int show)
{
    static const char *names[] = {"NtQueryAttributesFile", "NtCreateFile", "LdrGetDllHandle",
        "NtOpenFile", "NtQueryDirectoryFile", "NtQueryFullAttributesFile", "LdrLoadDll"};
    HMODULE module = GetModuleHandleA("ntdll.dll");
    HANDLE thread;
    unsigned int i;
    (void)instance; (void)previous; (void)command; (void)show;
    output = CreateFileA("C:\\wine-thunk-probe3.log", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                         NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (output == INVALID_HANDLE_VALUE) return 1;
    SetUnhandledExceptionFilter(fault);
    report("Wine x64 thunk lifecycle probe v3 PID=%lu ntdll=%p\r\n", GetCurrentProcessId(), module);
    describe("passthrough", (void *)passthrough_detour);
    stop_event = CreateEventA(NULL, TRUE, FALSE, NULL);
    ready_event = CreateEventA(NULL, TRUE, FALSE, NULL);
    thread = CreateThread(NULL, 0, worker, NULL, 0, NULL);
    report("WORKER thread=%p ready=%lu\r\n", thread, WaitForSingleObject(ready_event, 5000));
    report("MINHOOK init=%d\r\n", MH_Initialize());
    for (i = 0; i < sizeof(names) / sizeof(names[0]); ++i)
    {
        void *target = export_address(module, names[i]);
        MH_STATUS status;
        NTSTATUS before, during, after;
        LONG hits_before, hits_after;
        BYTE saved[16];
        if (!target) continue;
        memcpy(saved, target, sizeof(saved));
        report("\r\nEXPORT %s target=%p\r\n", names[i], target);
        before = call_target(i, target); /* Warm translated code before changing it. */
        report("BEFORE status=%08lx\r\n", (ULONG)before);
        SetLastError(0);
        status = MH_CreateHook(target, (void *)passthrough_detour, &active_original);
        report("CREATE status=%d (%s) err=%lu trampoline=%p\r\n", status,
               MH_StatusToString(status), GetLastError(), active_original);
        if (status != MH_OK) continue;
        detour_hits = 0;
        report("ENABLE_BEGIN\r\n");
        SetLastError(0);
        status = MH_EnableHook(target);
        report("ENABLE status=%d (%s) err=%lu\r\n", status, MH_StatusToString(status), GetLastError());
        if (status == MH_OK)
        {
            hits_before = detour_hits;
            report("CALL_BEGIN\r\n");
            during = call_target(i, target);
            hits_after = detour_hits;
            report("CALL status=%08lx baseline=%08lx hits_before=%ld hits_after=%ld\r\n",
                   (ULONG)during, (ULONG)before, hits_before, hits_after);
            report("DISABLE_BEGIN\r\n");
            status = MH_DisableHook(target);
            report("DISABLE status=%d (%s) err=%lu\r\n", status, MH_StatusToString(status), GetLastError());
            if (status != MH_OK) ExitProcess(3);
            hits_before = detour_hits;
            after = call_target(i, target);
            report("AFTER status=%08lx hits_delta=%ld bytes_restored=%d\r\n",
                   (ULONG)after, detour_hits - hits_before, !memcmp(saved, target, sizeof(saved)));
        }
        report("REMOVE status=%d\r\n", MH_RemoveHook(target));
        active_original = NULL;
    }
    MH_Uninitialize();
    SetEvent(stop_event);
    WaitForSingleObject(thread, 5000);
    CloseHandle(thread);
    CloseHandle(ready_event);
    CloseHandle(stop_event);
    report("COMPLETE\r\n");
    CloseHandle(output);
    MessageBoxA(NULL, "Diagnostic complete. Report: C:\\wine-thunk-probe3.log", "Wine thunk lifecycle probe", MB_OK);
    return 0;
}

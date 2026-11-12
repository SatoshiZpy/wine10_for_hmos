#!/usr/bin/env python3
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
"""Check the narrow host-only protection path and its failure boundaries."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
text = (root / 'dlls/ntdll/unix/virtual.c').read_text()
start = text.index('static BOOL protect_ubt_host_pages(')
helper = text[start:text.index('\n#endif', start)]
prefix = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
typedef int BOOL;
typedef uint8_t BYTE;
typedef uint32_t NTSTATUS, ULONG;
typedef size_t SIZE_T;
#define TRUE 1
#define FALSE 0
#define PAGE_READONLY 2
#define PAGE_READWRITE 4
#define STATUS_SUCCESS 0
#define STATUS_ACCESS_DENIED 0xc0000022u
#define VPROT_READ 1
#define VPROT_WRITE 2
#define VPROT_EXEC 4
#define VPROT_WRITECOPY 8
#define VPROT_GUARD 0x10
#define VPROT_WRITEWATCH 0x40
#define VPROT_SYSTEM 0x200
#define TRACE(...) ((void)0)
typedef struct { void *EmulatorData[4]; } CHPE_V2_CPU_AREA_INFO;
static CHPE_V2_CPU_AREA_INFO cpu = {{0, (void *)1}};
static struct { CHPE_V2_CPU_AREA_INFO *ChpeV2CpuAreaInfo; } teb = {&cpu};
struct file_view { unsigned int protect; };
static int ubt_host_protect = 1, emulated = 1;
static size_t host_page_size = 4096, page_size = 4096;
static BYTE protections[2] = {7, 13};
static char memory[8192];
static unsigned calls;
static int host_prot, fail_mprotect;
static void *host_base;
static size_t host_size;
#define NtCurrentTeb() (&teb)
#define is_arm64ec() emulated
#define is_vprot_exec_write(p) (((p) & VPROT_EXEC) && ((p) & (VPROT_WRITE | VPROT_WRITECOPY)))
static BYTE get_page_vprot(const void *p) { return protections[((const char *)p - memory) / 4096]; }
static int mock_mprotect(void *p, size_t size, int prot)
{
    ++calls; host_prot = prot; host_base = p; host_size = size;
    return fail_mprotect ? -1 : 0;
}
#define mprotect mock_mprotect
'''
tests = r'''
static void rejected(struct file_view *view, ULONG prot, size_t size)
{
    NTSTATUS status = 0xdeadbeef;
    unsigned before = calls;
    assert(!protect_ubt_host_pages(view, memory, size, prot, &status));
    assert(calls == before && status == 0xdeadbeef);
}
int main(void)
{
    struct file_view view = {0};
    NTSTATUS status = 1;
    assert(protect_ubt_host_pages(&view, memory, sizeof(memory), PAGE_READONLY, &status));
    assert(status == 0 && host_prot == PROT_READ && host_base == memory && host_size == sizeof(memory));
    assert(protections[0] == 7 && protections[1] == 13);
    assert(protect_ubt_host_pages(&view, memory, sizeof(memory), PAGE_READWRITE, &status));
    assert(status == 0 && host_prot == (PROT_READ | PROT_WRITE));
    fail_mprotect = 1;
    assert(protect_ubt_host_pages(&view, memory, sizeof(memory), PAGE_READONLY, &status));
    assert(status == STATUS_ACCESS_DENIED && protections[0] == 7 && protections[1] == 13);
    fail_mprotect = 0;
    ubt_host_protect = 0; rejected(&view, 2, 8192); ubt_host_protect = 1;
    emulated = 0; rejected(&view, 2, 8192); emulated = 1;
    cpu.EmulatorData[1] = 0; rejected(&view, 2, 8192); cpu.EmulatorData[1] = (void *)1;
    teb.ChpeV2CpuAreaInfo = 0; rejected(&view, 2, 8192); teb.ChpeV2CpuAreaInfo = &cpu;
    host_page_size = 16384; rejected(&view, 2, 8192); host_page_size = 4096;
    rejected(&view, 1, 8192); rejected(&view, 0x40, 8192); rejected(&view, 2, 0);
    view.protect = VPROT_WRITEWATCH; rejected(&view, 2, 8192);
    view.protect = VPROT_SYSTEM; rejected(&view, 2, 8192); view.protect = 0;
    protections[1] = 5; rejected(&view, 4, 8192); /* Never grant write to guest RX. */
    protections[1] = 3; rejected(&view, 2, 8192); /* Ordinary data has no override. */
    protections[1] = 13 | VPROT_GUARD; rejected(&view, 2, 8192);
    puts("PASS: guest permissions preserved, host protection applied, failures and non-UBT cases retained");
}
'''
with tempfile.TemporaryDirectory(prefix='wine-ubt-protection-') as temp:
    source, binary = Path(temp)/'check.c', Path(temp)/'check'
    source.write_text(prefix + helper + tests)
    subprocess.run(['cc', '-Wall', '-Wextra', '-Werror', str(source), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)

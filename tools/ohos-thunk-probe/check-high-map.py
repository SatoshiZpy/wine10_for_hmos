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

"""Exercise the real server initializer against hosts with limited VA space."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / "server/mapping.c").read_text()
old = subprocess.check_output(["git", "show", "HEAD:server/mapping.c"], cwd=root, text=True)

def initializer(text):
    start = text.index("#if defined(__OHOS__) && defined(__aarch64__)") if "static void free_available_high_map_addr" in text else text.index("void init_memory(void)")
    end = text.index("static void ranges_dump", start)
    return text[start:end]

prefix = r'''
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
typedef uint64_t client_ptr_t;
typedef uint64_t mem_size_t;
static size_t host_page_mask;
static uint64_t limit, high_base, high_size;
static unsigned int calls, unmapped, low_ranges;
static int mode;
static void free_map_addr(client_ptr_t base, mem_size_t size)
{
    if (base >> 32) { high_base = base; high_size = size; }
    else { assert(base == 0x60000000 && size == 0x1c000000); low_ranges++; }
}
static __attribute__((unused)) void *mock_mmap(void *addr, size_t size, int prot, int flags, int fd, off_t offset)
{
    calls++;
    assert(size == (size_t)sysconf(_SC_PAGESIZE));
    assert(prot == PROT_NONE && flags == (MAP_PRIVATE | MAP_ANON));
    assert(fd == -1 && offset == 0);
    if ((uintptr_t)addr + size <= limit) return addr;
    if (mode == 0) { errno = ENOMEM; return MAP_FAILED; }
    errno = EEXIST; /* stale errno must not turn a relocated hint into success */
    return (void *)0x10000000;
}
static __attribute__((unused)) int mock_munmap(void *addr, size_t size)
{
    assert(addr != MAP_FAILED && size == (size_t)sysconf(_SC_PAGESIZE));
    unmapped++;
    return 0;
}
#define mmap mock_mmap
#define munmap mock_munmap
'''
suffix = r'''
int main(int argc, char **argv)
{
    assert(argc == 4);
    limit = UINT64_C(1) << atoi(argv[1]);
    mode = atoi(argv[2]);
    init_memory();
    assert(low_ranges == 1);
    if (atoi(argv[3]))
    {
        assert(high_base == UINT64_C(0x600000000000));
        assert(high_size == UINT64_C(0x100000000000));
        assert(!calls);
    }
    else
    {
        assert(high_base + high_size <= limit);
        assert((limit <= UINT64_C(0x100000000)) == (high_size == 0));
        if (high_size) assert(high_size >= UINT64_C(0x10000000));
        assert(calls && (mode ? unmapped == calls : unmapped == (high_size != 0)));
    }
    printf("PASS bits=%s mode=%d base=%llx size=%llx probes=%u\n", argv[1], mode,
           (unsigned long long)high_base, (unsigned long long)high_size, calls);
}
'''
with tempfile.TemporaryDirectory(prefix="wine-high-map-") as directory:
    directory = Path(directory)
    for name, text, platform in [("before", old, True), ("after", source, True), ("other-platform", source, False)]:
        cfile, exe = directory / (name + ".c"), directory / name
        cfile.write_text(prefix + initializer(text) + suffix)
        args = ["cc", "-Wall", "-Wextra", "-Werror"]
        if platform:
            args += ["-D__OHOS__=1", "-D__aarch64__=1"]
        args += [str(cfile), "-o", str(exe)]
        subprocess.run(args, check=True)
        if name == "before":
            result = subprocess.run([str(exe), "39", "0", "0"], capture_output=True)
            assert result.returncode != 0, "Old allocator must reproduce the invalid range"
            print("BEFORE: reproduced out-of-host-range assertion")
        elif platform:
            for bits in [32, 36, 39, 42, 47, 48]:
                for mode in [0, 1]:
                    subprocess.run([str(exe), str(bits), str(mode), "0"], check=True)
        else:
            subprocess.run([str(exe), "39", "0", "1"], check=True)

# Also emit the same initializer with real mmap for the OHOS shell probe.
native = r'''
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>
typedef uint64_t client_ptr_t;
typedef uint64_t mem_size_t;
static size_t host_page_mask;
static void free_map_addr(client_ptr_t base, mem_size_t size)
{
    printf("HOST_IMAGE_RANGE base=%llx size=%llx end=%llx\n",
           (unsigned long long)base, (unsigned long long)size,
           (unsigned long long)(base + size));
}
'''
out = root / ".build-cache/thunk-fix/high-map-probe.c"
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text(native + initializer(source) + "int main(void) { init_memory(); return 0; }\n")

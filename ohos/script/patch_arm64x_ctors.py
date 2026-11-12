#!/usr/bin/env python3
"""
patch_arm64x_ctors.py — 修复 lld-link -machine:arm64x 合并 DLL 的构造函数列表。

背景: build_dxvk.sh / build_dxvk_legacy.sh 用 lld-link 把 arm64ec (buildec) 与
aarch64 (buildarm64) 两套 obj + 两套 CRT 链接为单一 ARM64X (0xA64E) PE。
merge 脚本为满足 gccmain.o 的 __CTOR_LIST__ 符号, 用了空 stub [-1,0] ×2,
导致 .ctors / .ctors.6 里两套 CRT/libc++ 的真实全局构造函数指针从不被调用
→ 加载后静态数据为零 → 原生 (ARM64) 进程内首次使用 C++ iostream 即 SIGSEGV
(dxgi.dll RVA 0xaf288, std::streambuf 空 vtable)。

修复: 把 .ctors 重写为单一有效列表 [-1][ptr×N][0](walker = mingw-w64
__do_global_ctors: 从 .rdata 槽读列表头, 倒序调用, 有单次执行保护), 并:
  1) 只保留原生 (aarch64) flavor 的构造函数指针 — EC (arm64ec) 构造函数
     依赖 EC 运行时 (arm64x dispatch / x16 调用), 在原生进程中未初始化,
     调用即 x16=NULL SIGSEGV (d3d9.dll RVA 0x21b6d0, 实测)。NAT/EC 指针靠
     RVA 聚类自动分离 (两套 CRT 代码区间之间有 >0x10000 的空隙);
  2) 重写 .ctors 页的基址重定位块 (IMAGE_REL_BASED_DIR64 = type 10,
     条目数 N, 偏移 +8..8N); 块不存在则在块列表末尾新建;
  3) 块大小变化时平移 .reloc 后续内容, 同步 DataDirectory[5].Size、
     两个视图的 LoadConfig.DynamicValueRelocTableOffset、.reloc VirtualSize;
  4) 把 .rdata 中指向 stub [-1] 标记的所有指针槽重指向 .ctors 起点。

已知限制 (后续工作): EC 视图 (x64 进程) 的构造函数不运行 — 完整方案需用
DVRT 在 EC 视图中把列表头切到 EC 列表 (per-view 双列表布局)。

用法: patch_arm64x_ctors.py [--all-flavors] <dll> [dll...]
      --all-flavors  保留全部 (NAT+EC) 构造函数 (仅调试用)
"""

import struct
import sys

DIR_RELOC = 5
DIR_TLS = 9
DIR_LOAD_CONFIG = 10
MIN_EC_GAP = 0x10000  # NAT/EC 指针簇之间的最小 RVA 间隔


class PE:
    def __init__(self, data):
        self.d = bytearray(data)
        d = self.d
        self.pe = struct.unpack_from('<I', d, 0x3C)[0]
        assert d[self.pe:self.pe + 4] == b'PE\0\0', 'not a PE'
        self.nsec = struct.unpack_from('<H', d, self.pe + 6)[0]
        self.optsize = struct.unpack_from('<H', d, self.pe + 20)[0]
        self.opt = self.pe + 24
        magic = struct.unpack_from('<H', d, self.opt)[0]
        assert magic == 0x20b, 'not PE32+'
        self.imagebase = struct.unpack_from('<Q', d, self.opt + 24)[0]
        self.ndir = struct.unpack_from('<I', d, self.opt + 108)[0]
        self.sections = []
        for i in range(self.nsec):
            o = self.opt + self.optsize + i * 40
            name = bytes(d[o:o + 8]).rstrip(b'\0').decode('ascii', 'replace')
            vs, va, rs, ra = struct.unpack_from('<IIII', d, o + 8)
            self.sections.append({'name': name, 'vs': vs, 'va': va,
                                  'rs': rs, 'ra': ra, 'hdr': o})

    def sec_by_name(self, name):
        for s in self.sections:
            if s['name'] == name:
                return s
        return None

    def rva2off(self, rva):
        for s in self.sections:
            if s['va'] <= rva < s['va'] + max(s['vs'], s['rs']):
                return s['ra'] + (rva - s['va'])
        return None

    def datadir(self, idx):
        return struct.unpack_from('<II', self.d, self.opt + 112 + idx * 8)

    def set_datadir(self, idx, va, size):
        struct.pack_into('<II', self.d, self.opt + 112 + idx * 8, va, size)


def load_config_off(pe):
    va, sz = pe.datadir(DIR_LOAD_CONFIG)
    if not va:
        return None, None
    return pe.rva2off(va), sz


def load_config_offsets(pe):
    """Find both load configs before moving the ARM64X dynamic relocation table."""
    native, size = load_config_off(pe)
    if native is None or size < 0xE6:
        return []
    offsets = {native}
    table_offset, section = struct.unpack_from('<IH', pe.d, native + 0xE0)
    if not section or not table_offset:
        return offsets
    table = pe.sections[section - 1]['ra'] + table_offset
    version, table_size = struct.unpack_from('<II', pe.d, table)
    if version != 1:
        raise ValueError(f'unsupported dynamic relocation table version {version}')
    record = table + 8
    while record < table + 8 + table_size:
        symbol, reloc_size = struct.unpack_from('<QI', pe.d, record)
        block = record + 12
        end = block + reloc_size
        if symbol == 6:  # IMAGE_DYNAMIC_RELOCATION_ARM64X
            while block < end:
                page, block_size = struct.unpack_from('<II', pe.d, block)
                if block_size < 8 or block + block_size > end:
                    raise ValueError('invalid ARM64X relocation block')
                entry = block + 8
                while entry < block + block_size:
                    fixup, = struct.unpack_from('<H', pe.d, entry)
                    entry += 2
                    if not fixup:  # alignment padding
                        continue
                    kind, arg = (fixup >> 12) & 3, fixup >> 14
                    rva = page + (fixup & 0xFFF)
                    if kind == 1:  # VALUE
                        width = 1 << arg
                        if rva == pe.opt + 112 + DIR_LOAD_CONFIG * 8:
                            if width != 4:
                                raise ValueError('invalid hybrid load config RVA fixup')
                            hybrid_rva, = struct.unpack_from('<I', pe.d, entry)
                            hybrid = pe.rva2off(hybrid_rva)
                            if hybrid is None:
                                raise ValueError('unmapped hybrid load config')
                            offsets.add(hybrid)
                        entry += width
                    elif kind == 2:  # DELTA
                        entry += 2
                    elif kind != 0:
                        raise ValueError('unknown ARM64X fixup type')
                block += block_size
        record = end
    return offsets


def split_native_ec(ptrs):
    """按 RVA 聚类把构造指针分为 (native, ec) 两簇: 找最大间隙。"""
    if not ptrs:
        return [], []
    sp = sorted(ptrs)
    if len(sp) == 1:
        return sp, []
    gaps = [(sp[i + 1] - sp[i], i) for i in range(len(sp) - 1)]
    g, i = max(gaps)
    if g >= MIN_EC_GAP and i + 1 < len(sp):
        return sp[:i + 1], sp[i + 1:]
    return sp, []  # 无明显间隙: 视为全部原生


def patch_dll(path, all_flavors=False, verbose=True):
    with open(path, 'rb') as f:
        pe = PE(f.read())
    d = pe.d
    load_configs = load_config_offsets(pe)
    ctors = pe.sec_by_name('.ctors')
    text = pe.sec_by_name('.text')
    if not ctors or not text:
        if verbose:
            print(f'{path}: no .ctors/.text — skip')
        return False
    tstart = pe.imagebase + text['va']
    tend = pe.imagebase + text['va'] + text['vs']

    # 收集 .ctors 与 .ctors.* 的真实指针 (指向 .text 的 VA)
    extra_secs = [s for s in pe.sections
                  if s['name'].startswith('.ctors') and s is not ctors]
    ptrs = []
    markers = []  # -1 槽位 (rva)
    for s in [ctors] + extra_secs:
        off = s['ra']
        for k in range(s['vs'] // 8):
            v = struct.unpack_from('<Q', d, off + k * 8)[0]
            rva = s['va'] + k * 8
            if v == 0xFFFFFFFFFFFFFFFF:
                markers.append(rva)
            elif tstart <= v < tend:
                ptrs.append(v)
    if not ptrs:
        if verbose:
            print(f'{path}: no real ctor pointers — skip')
        return False
    if not markers:
        if verbose:
            print(f'{path}: no __CTOR_LIST__ stub marker found — skip (unexpected)')
        return False

    nat, ec = split_native_ec([p - pe.imagebase for p in ptrs])
    if all_flavors:
        keep = ptrs
        mode = f'all({len(nat)}N+{len(ec)}E)'
    else:
        keep = [p + pe.imagebase for p in nat]
        mode = f'native-only({len(nat)}N, drop {len(ec)}E)'
    n = len(keep)
    if n == 0:
        if verbose:
            print(f'{path}: no native ctors — skip (unexpected)')
        return False

    slots = ctors['vs'] // 8
    if n + 2 > slots:
        print(f'{path}: ERROR list ({n} ptrs + header/term) exceeds .ctors '
              f'{slots} slots — abort')
        return False

    # 1) 重写 .ctors: [-1][ptr×N][0]
    off = ctors['ra']
    struct.pack_into('<Q', d, off, 0xFFFFFFFFFFFFFFFF)
    for i, p in enumerate(keep):
        struct.pack_into('<Q', d, off + 8 + i * 8, p)
    struct.pack_into('<Q', d, off + 8 + n * 8, 0)

    # 2) 重写 .ctors 页重定位块 (不存在则在块列表末尾新建)
    page = ctors['va'] & ~0xFFF
    reloc = pe.sec_by_name('.reloc')
    rva_size = pe.datadir(DIR_RELOC)[1]
    ro = reloc['ra']
    target = None
    roff = 0
    while roff < rva_size:
        va, sz = struct.unpack_from('<II', d, ro + roff)
        if va == 0 and sz == 0:
            break
        if va == page:
            target = roff
            break
        roff += sz
    # PE base relocation blocks must be DWORD aligned; pad with ABSOLUTE entries.
    new_sz = (8 + 2 * n + 3) & ~3
    if target is None:
        # 无既有块: 在块区末尾 (dir[5].Size 处) 插入新块
        target = rva_size
        old_sz = 0
    else:
        _, old_sz = struct.unpack_from('<II', d, ro + target)
    delta = new_sz - old_sz

    if delta > 0:
        # 检查 .reloc 空间 (RS 固定, VS 增长不得超 RS)
        if reloc['vs'] + delta > reloc['rs']:
            print(f'{path}: ERROR .reloc overflow (need +{delta}) — abort')
            return False
    if delta != 0:
        # 平移 target 块之后的所有内容
        tail_start = ro + target + old_sz
        tail_end = ro + reloc['vs']
        tail = bytes(d[tail_start:tail_end])
        d[ro + target + new_sz:ro + target + new_sz + len(tail)] = tail
        # 更新 DataDirectory[5].Size
        va5, sz5 = pe.datadir(DIR_RELOC)
        pe.set_datadir(DIR_RELOC, va5, sz5 + delta)
        # 更新 LoadConfig.DynamicValueRelocTableOffset (若存在)
        for lco in load_configs:
            dvoff = struct.unpack_from('<I', d, lco + 0xE0)[0]
            if dvoff:
                struct.pack_into('<I', d, lco + 0xE0, dvoff + delta)
        # 更新 .reloc VirtualSize
        struct.pack_into('<I', d, reloc['hdr'] + 8, reloc['vs'] + delta)
    # 写新块: N 个 DIR64 (type 10 = IMAGE_REL_BASED_DIR64)
    struct.pack_into('<II', d, ro + target, page, new_sz)
    d[ro + target + 8:ro + target + new_sz] = bytes(new_sz - 8)
    for i in range(n):
        e = (10 << 12) | ((ctors['va'] & 0xFFF) + 8 + i * 8)
        struct.pack_into('<H', d, ro + target + 8 + i * 2, e)

    # 3) 重指 .rdata 中指向 stub 标记的槽 → .ctors 起点
    repointed = 0
    for m in markers:
        pat = struct.pack('<Q', pe.imagebase + m)
        i = 0
        while True:
            i = bytes(d).find(pat, i)
            if i < 0:
                break
            struct.pack_into('<Q', d, i, pe.imagebase + ctors['va'])
            repointed += 1
            i += 1

    with open(path, 'wb') as f:
        f.write(d)
    if verbose:
        print(f'{path}: patched [{mode}] — reloc block '
              f'0x{old_sz:x}->0x{new_sz:x} (delta {delta:+d}), '
              f'{repointed} ptr slot(s) repointed')
    return True


def main():
    args = [a for a in sys.argv[1:] if a != '--all-flavors']
    all_flavors = '--all-flavors' in sys.argv
    if not args:
        print(__doc__)
        sys.exit(1)
    ok = 0
    for p in args:
        if patch_dll(p, all_flavors=all_flavors):
            ok += 1
    print(f'patched {ok}/{len(args)}')
    sys.exit(0 if ok == len(args) else 1)


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""Patch meson 1.12: LLVMDynamicLinker 不再注入 --allow-shlib-undefined.

背景: clang --target=arm64ec-w64-mingw32 -fuse-ld=lld 走 lld 的 MinGW PE
前端, 它不认 --allow-shlib-undefined (ELF 共享库专属语义)。meson 的能力
探测 (_supports_flag) 只跑 --version 不发生真实链接, 检测不到这一点, 导致
find_library() 的每次链接探测都带着该参数而失败。

注意: meson 构造 LLVMDynamicLinker 时 system 属性常常是 'unknown system'
(detect.py 部分分支不传), 因此不能靠 system == 'windows' 判断, 直接无条件
去除该 flag。lld PE 前端下此 flag 无任何合法用途, 无条件去除是安全的。

用法: python3 patch_meson_lld.py [linkers.py 路径]
      省略路径时自动定位当前 python3 的 mesonbuild 安装。
"""
import subprocess
import sys

# 自动发现 mesonbuild 包内 linkers.py 的路径 (调用方可用 argv[1] 覆盖)
if len(sys.argv) > 1:
    p = sys.argv[1]
else:
    try:
        p = subprocess.run(
            [sys.executable, '-c',
             'import mesonbuild, os; print(os.path.join(os.path.dirname('
             'mesonbuild.__file__), "linkers", "linkers.py"))'],
            capture_output=True, text=True, check=True).stdout.strip()
    except subprocess.CalledProcessError:
        p = ''
    if not p:
        print('ERROR: 无法定位 mesonbuild/linkers/linkers.py, '
              '请显式传入路径: python3 patch_meson_lld.py <linkers.py>')
        sys.exit(1)

src = open(p, encoding='utf-8').read()

# 撤掉旧补丁 (若有), 换成无条件版本
prev_patch = '''    def get_allow_undefined_args(self) -> T.List[str]:
        # lld MinGW PE frontend (clang -fuse-ld=lld, windows targets) rejects
        # --allow-shlib-undefined (ELF-only concept); the probe cannot detect
        # it, so force the flag off for windows targets.
        if self.system == 'windows':
            return []
        if self.has_allow_shlib_undefined:
            return self._apply_prefix('--allow-shlib-undefined')
        return []'''
new_impl = '''    def get_allow_undefined_args(self) -> T.List[str]:
        # lld's MinGW PE frontend (clang -fuse-ld=lld for *-w64-mingw32
        # targets) rejects --allow-shlib-undefined, an ELF shared-library
        # concept. meson's capability probe never links so it cannot detect
        # this, and the linker's system attribute is often
        # 'unknown system', so the only reliable fix is to drop the flag
        # unconditionally. It has no legitimate use with PE targets.
        return []'''
if prev_patch in src:
    src = src.replace(prev_patch, new_impl)
    print('replaced previous conditional patch')

old1 = '''    def get_allow_undefined_args(self) -> T.List[str]:
        if self.has_allow_shlib_undefined:
            return self._apply_prefix('--allow-shlib-undefined')
        return []'''
if new_impl not in src:
    assert src.count(old1) == 1, 'pattern not found or ambiguous: %d' % src.count(old1)
    src = src.replace(old1, new_impl)

open(p, 'w', encoding='utf-8').write(src)
print('PATCHED LLVMDynamicLinker.get_allow_undefined_args (unconditional)')

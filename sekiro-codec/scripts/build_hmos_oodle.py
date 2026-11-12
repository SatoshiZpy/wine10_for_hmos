#!/usr/bin/env python3
"""Build only the native resource codec; Wine/UBT retain game execution."""
from pathlib import Path
import argparse,hashlib,json,subprocess
ROOT=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def build(mingw, wine):
 vendor=ROOT/'thirdparty/hmos_ooz'; source=ROOT/'entry/src/main/cpp/hmos_codec';out=ROOT/'build/hmos-codec';out.mkdir(parents=True,exist_ok=True)
 upstream=json.loads((vendor/'upstream.json').read_text())
 for n,h in upstream['files'].items():
  if sha(vendor/n)!=h:raise ValueError('Codec upstream changed: '+n)
 text=(vendor/'kraken.cpp').read_text();anchor='// The decompressor will write outside of the target buffer.'
 if text.count(anchor)!=1:raise ValueError('Unknown ooZ source layout')
 (out/'kraken-library.cpp').write_text(text[:text.index(anchor)])
 dll=out/'oo2core_6_win64.dll'
 cmd=[str(Path(mingw)/'bin/arm64ec-w64-mingw32-clang++'),'-O3','-DNDEBUG','-fno-strict-aliasing','-shared','-Wl,--strip-all,--no-insert-timestamp','-I'+str(source),'-I'+str(vendor),str(out/'kraken-library.cpp'),str(vendor/'lzna.cpp'),str(vendor/'bitknit.cpp'),str(source/'oodle_bridge.cpp'),str(source/'oodle_exports.def'),'-o',str(dll)]
 subprocess.run(cmd,check=True)
 subprocess.run([str(Path(wine)/'build-tools/tools/winebuild/winebuild'),'--builtin',str(dll)],check=True)
 inputs=[vendor/n for n in upstream['files']]+[vendor/'upstream.json',source/'stdafx.h',source/'oodle_bridge.cpp',source/'oodle_exports.def',Path(__file__)]
 receipt={'upstream':upstream,'files':{str(p.relative_to(ROOT)):sha(p) for p in inputs},'sha256':sha(dll),'abi':'ARM64EC','originalGameDllSha256':'988e8226d129b03011a96f51b476e86d38c0c3e817266375a78a7cf3b85d93c4'}
 (out/'build.json').write_text(json.dumps(receipt,indent=2)+'\n');print('Built native ARM64EC codec',sha(dll))
if __name__=='__main__':
 ap=argparse.ArgumentParser();ap.add_argument('--mingw',required=True);ap.add_argument('--wine-root',required=True);args=ap.parse_args();build(args.mingw,args.wine_root)

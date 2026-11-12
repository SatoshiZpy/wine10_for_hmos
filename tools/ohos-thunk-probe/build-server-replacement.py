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

"""Build one replacement library without writing to the active build tree."""
from pathlib import Path
import hashlib
import json
import re
import shlex
import subprocess
import sys

root = Path(__file__).resolve().parents[2]
parent = Path('/Users/zhangpeiyu/.codex/worktrees/wine-quickstart-build/wine10_for_hmos')
output = root / '.build-cache/thunk-fix'
output.mkdir(parents=True, exist_ok=True)
container = 'wine-quickstart-20261111'
isolated = '/tmp/wine-silksong-thunks-20261111'
module = sys.argv[1] if len(sys.argv) > 1 else 'server'
assert module in ('server', 'ntdll')
source_path = 'server/mapping.c' if module == 'server' else 'dlls/ntdll/unix/virtual.c'
object_path = source_path[:-2] + '.o'
library_path = 'server/libwineserver.so' if module == 'server' else 'dlls/ntdll/ntdll.so'
library_name = Path(library_path).name
source_data = (root/source_path).read_bytes()
if module == 'ntdll':
    isolated += '-ntdll'
    # Apply only our diff on top of the existing, tested compatibility repairs.
    overlay = output/'ntdll-source'
    path = overlay/source_path
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes((parent/source_path).read_bytes())
    patch = subprocess.check_output(['git', 'diff', '--', source_path], cwd=root)
    subprocess.run(['patch', '-p1', '--batch'], cwd=overlay, input=patch, check=True)
    source_data = path.read_bytes()
    assert (parent/'build'/library_path).read_bytes() == (output/'app/wine_engine/libs/arm64-v8a'/library_name).read_bytes(), 'Parent library differs from installed baseline'
makefile = (parent / 'build/Makefile').read_text().replace('\\\n', '')
variables = dict(re.findall(r'^(\w+) = (.*)$', makefile, re.M))

def recipe(target):
    line = re.search(r'^' + re.escape(target) + r':[^\n]*\n\t([^\n]+)', makefile, re.M).group(1)
    line = line.replace('$@', target)
    for _ in range(10):
        if '$(' not in line:
            break
        line = re.sub(r'\$\((\w+)\)', lambda m: variables[m[1]], line)
    return shlex.split(line)

compile_args = recipe(object_path)
compile_args += ['-I' + str((parent/source_path).parent)]
compile_args = [isolated + '/' + Path(source_path).name if arg == '../' + source_path else
                isolated + '/' + Path(object_path).name if arg == object_path else arg for arg in compile_args]
link_args = recipe(library_path)
objects = [arg for arg in link_args if arg.endswith('.o')]
link_args = [isolated + '/' + Path(arg).name if arg in objects or arg == library_path else arg for arg in link_args]
manifest = {'objects': {name: hashlib.sha256((parent/'build'/name).read_bytes()).hexdigest() for name in objects},
            'source_path': source_path, 'source_sha256': hashlib.sha256(source_data).hexdigest(),
            'compile': compile_args, 'link': link_args}

subprocess.run(['docker', 'exec', container, 'mkdir', '-p', isolated], check=True)
subprocess.run(['docker', 'exec', '-i', container, 'python3', '-c',
                'import pathlib,sys; pathlib.Path(sys.argv[1]).write_bytes(sys.stdin.buffer.read())',
                isolated + '/' + Path(source_path).name], input=source_data, check=True)
subprocess.run(['docker', 'exec', container, 'cp', *[str(parent/'build'/name) for name in objects], isolated], check=True)
for args in [compile_args, link_args]:
    subprocess.run(['docker', 'exec', '-w', str(parent/'build'), container, *args], check=True)
(output/library_name).write_bytes(subprocess.check_output(
    ['docker', 'exec', container, 'cat', isolated + '/' + library_name]))
manifest['output_sha256'] = hashlib.sha256((output/library_name).read_bytes()).hexdigest()
for name, digest in manifest['objects'].items():
    assert hashlib.sha256((parent/'build'/name).read_bytes()).hexdigest() == digest, 'Parent objects changed during build'
(output/('build-manifest.json' if module == 'server' else 'ntdll-build-manifest.json')).write_text(json.dumps(manifest, indent=2) + '\n')
print('Built isolated ' + library_name + ' SHA256=' + manifest['output_sha256'])

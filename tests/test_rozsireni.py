#!/usr/bin/env python3
"""Tests for VALMEM/PARAMLISTS added to the uploaded FUNEXP implementation."""
import argparse
from collections import Counter
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import tempfile
from rozsireni_cases import extension_cases

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--suite',choices=('all','valmem','paramlists','funexp'),default='all')
    ap.add_argument('--interpreter')
    ap.add_argument('--cc',default=os.environ.get('CC','gcc'))
    ap.add_argument('--sanitize',action='store_true')
    ap.add_argument('--color',choices=('auto','always','never'),default='auto')
    args=ap.parse_args()
    root=Path(__file__).resolve().parent.parent
    has_params=(root/'src/paramlists.c').is_file()
    if args.suite=='paramlists' and not has_params:
        print('[NEOVERENO] PARAMLISTS jeste neni v teto variante.');return 2
    vm=shlex.split(args.interpreter) if args.interpreter else None
    if not vm:
        for name in ('ic26int-linux-x86_64','ic26int-linux-aarch64','ic26int'):
            local=root/name
            if local.is_file():vm=[str(local)];break
            path=shutil.which(name)
            if path:vm=[path];break
    if not vm:
        print('[NEOVERENO] Pouzij --interpreter ./ic26int-linux-x86_64');return 2
    if '/' in vm[0]:vm[0]=str(Path(vm[0]).resolve())
    result_dir=root/'tests/results';result_dir.mkdir(parents=True,exist_ok=True)
    result_dir=Path(tempfile.mkdtemp(prefix='rozsireni-',dir=result_dir))
    binary=result_dir/'ifj26'
    cmd=shlex.split(args.cc)+['-std=c23','-Isrc','-O1','-g','-Wall','-Wextra','-Wpedantic']
    if args.sanitize:cmd+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer']
    cmd += [str(p) for p in sorted((root/'src').glob('*.c'))]+['-o',str(binary)]
    build=subprocess.run(cmd,cwd=root,capture_output=True,timeout=90)
    (result_dir/'build.stdout').write_bytes(build.stdout);(result_dir/'build.stderr').write_bytes(build.stderr)
    if build.returncode:
        print('[NE] Preklad selhal:',result_dir/'build.stderr');return 2
    color=args.color=='always' or (args.color=='auto' and sys.stdout.isatty())
    results=[]
    for i,case in enumerate(extension_cases(has_params)):
        if args.suite!='all' and case['group']!=args.suite:continue
        name=case['group']+'/'+case['name'];folder=result_dir/f'{i:03d}';folder.mkdir()
        (folder/'source.ifj26').write_text(case['source'])
        try:
            p=subprocess.run([str(binary)],input=case['source'].encode(),cwd=root,capture_output=True,timeout=10)
            (folder/'compiler.stdout').write_bytes(p.stdout);(folder/'compiler.stderr').write_bytes(p.stderr)
            ok=p.returncode==case['rc'] and not any(x in p.stderr for x in (b'ERROR: AddressSanitizer',b'ERROR: LeakSanitizer',b'runtime error:'))
            reason=f'preklad: ocekavano {case["rc"]}, skutecnost {p.returncode}'
            # The existing parser can emit code before a later semantic error.
            # Error tests therefore check its error code, not an empty stdout.
            if ok and case['rc']==0:
                path=folder/'program.ifjcode26';path.write_bytes(p.stdout)
                q=subprocess.run(vm+[str(path)],input=case['stdin'].encode(),cwd=root,capture_output=True,timeout=10)
                (folder/'vm.stdout').write_bytes(q.stdout);(folder/'vm.stderr').write_bytes(q.stderr)
                ok=q.returncode==0 and q.stdout==case['output'].encode()
                reason=f'interpret {q.returncode}, vystup {q.stdout!r}, ocekavano {case["output"]!r}'
        except (OSError,subprocess.TimeoutExpired) as error:ok,reason=False,str(error)
        label='ANO' if ok else 'NE'
        if color:label=('\033[32m' if ok else '\033[31m')+label+'\033[0m'
        print(f'[{label}] {name}: {reason}',flush=True)
        results.append(dict(name=name,passed=ok,reason=reason))
    (result_dir/'report.json').write_text(json.dumps(results,indent=2))
    for group in ('valmem','funexp','paramlists'):
        selected=[r for r in results if r['name'].startswith(group+'/')]
        if selected:print(f'{group}: ANO {sum(r["passed"] for r in selected)}/{len(selected)}')
    passed=sum(r['passed'] for r in results)
    print(f'CELKEM: ANO {passed}/{len(results)}, NE {len(results)-passed}')
    print('Podrobnosti:',result_dir)
    return int(passed!=len(results))

if __name__=='__main__':raise SystemExit(main())

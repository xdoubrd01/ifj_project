#!/usr/bin/env python3
"""Compare the inherited e2e corpus with known failures in the uploaded version."""
import argparse
from concurrent.futures import ThreadPoolExecutor
from dataclasses import replace
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shlex
import sys
import tempfile


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--interpreter',required=True)
    ap.add_argument('--color',choices=('auto','always','never'),default='auto')
    ap.add_argument('--workers',type=int,default=min(8,os.cpu_count() or 2))
    args=ap.parse_args()
    if args.workers<1:ap.error('--workers musi byt kladne cislo')
    root=Path(__file__).resolve().parent.parent
    path=root/'tests/e2e/run_tests.py'
    sys.path.insert(0,str(path.parent))
    spec=importlib.util.spec_from_file_location('e2e_for_rozsireni',path)
    runner=importlib.util.module_from_spec(spec);spec.loader.exec_module(runner)
    runner.case_module.add_fuzz(26,1)
    selected=list(runner.case_module.CASES)
    if (root/'src/paramlists.c').is_file():
        # PARAMLISTS explicitly permits the first source and changes the second
        # from an undefined variable to a missing argument list. No other oracle changes.
        selected=[replace(c,rc=0,out=b'1') if c.name=='02_syntax.def_no_paren' else
                  replace(c,rc=5) if c.name=='03_undefined.func_as_value' else c for c in selected]
    known=json.loads((root/'tests/rozsireni_vychozi_chyby.json').read_text())
    interpreter=shlex.split(args.interpreter)
    if not interpreter:ap.error('Chybi interpret')
    if '/' in interpreter[0]:interpreter[0]=str(Path(interpreter[0]).resolve())
    output=root/'tests/results';output.mkdir(parents=True,exist_ok=True)
    output=Path(tempfile.mkdtemp(prefix='regrese-rozsireni-',dir=output))
    color=args.color=='always' or (args.color=='auto' and sys.stdout.isatty())
    with tempfile.TemporaryDirectory(prefix='ifj26-regrese-') as tmp:
        binary,sanitized=runner.build(str(root/'src'),tmp)
        def run(case):
            result=runner.run_case(case,binary,sanitized,interpreter)
            result['source_sha256']=hashlib.sha256(case.source.encode('latin-1')).hexdigest()
            previous=known.get(case.name)
            result['known_failure']=result['status']=='FAIL' and previous is not None and (
                result['phase']==previous['phase'] and result['source_sha256']==previous['source_sha256'])
            return result
        with ThreadPoolExecutor(max_workers=args.workers) as pool:results=list(pool.map(run,selected))
    for r in results:
        if r['status']=='PASS':continue
        label='ZNAME' if r['known_failure'] else 'NE'
        if color:label=('\033[33m' if r['known_failure'] else '\033[31m')+label+'\033[0m'
        print(f'[{label}] {r["name"]}: {r["reason"]}')
    passed=sum(r['status']=='PASS' for r in results)
    known_count=sum(r['known_failure'] for r in results)
    failed=len(results)-passed-known_count
    leaks=sum(r.get('leak',False) for r in results)
    print(f'CELKEM: ANO {passed}/{len(results)}, ZNAME {known_count}, NOVE_CHYBY {failed}, LEAKS {leaks}')
    print('ZNAME jsou skutecne neprosle pripady, ktere stejne selhaly uz v nahrane verzi.')
    print('Navratovy kod 0 znamena zadnou novou regresi; neznamena uplnou spravnost prekladace.')
    (output/'report.json').write_text(json.dumps(results,indent=2))
    print('Podrobnosti:',output)
    return int(failed!=0 or leaks!=0)

if __name__=='__main__':raise SystemExit(main())

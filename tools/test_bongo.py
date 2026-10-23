#!/usr/bin/env python3
"""Reproduce Tiny Studio host regressions using generated build/gen assets."""
from pathlib import Path
import subprocess
import sys
ROOT=Path(__file__).resolve().parents[1]
CASES={'bongo_test':True,'bongo_music_test':False,'bongo_metronome_test':True,
       'bongo_bass_party_test':False,'bongo_editor_test':True,'bongo_release_test':False}
flags=['-O1','-g','-fsanitize=address,undefined','-fno-sanitize=shift,signed-integer-overflow',
       '-fno-sanitize-recover=all','-ffp-contract=off','-w','-Ibuild/gen','-Ifirmware/src']
if not (ROOT/'build/gen/felucca_tables.h').exists():sys.exit('Build the firmware once to generate build/gen first.')
for name,restore in CASES.items():
    binary='build/'+name+'-release'
    log=ROOT/'build'/(name+'-release.log')
    with log.open('w') as output:
        for cmd in [['clang',*flags,'-o',binary,'tests/'+name+'.c','-lm'],[binary],*([[binary,'restore']] if restore else [])]:
            result=subprocess.run(cmd,cwd=ROOT,stdout=output,stderr=subprocess.STDOUT)
            if result.returncode:print(log.read_text());sys.exit(result.returncode)
    print(log.read_text(),end='',flush=True)
print('PASS: complete Tiny Studio host regression suite',flush=True)

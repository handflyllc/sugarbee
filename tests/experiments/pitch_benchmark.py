#!/usr/bin/env python3
"""Offline rendering benchmark. Python standard library and C99 compiler only."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess
import sys
import tarfile
import tempfile
sys.dont_write_bytecode = True
from pitch_candidate import candidate_source

ROOT = Path(__file__).resolve().parents[2]
SOURCES = ['sb_audio_bank.c','sb_audio_triangle.c','sb_audio_saw.c','sb_audio_pulse.c',
           'sb_audio_noise.c','sb_audio_envelope.c','sb_audio_filter.c','sb_wav.c',
           'sb_music_sequence.c','sb_music_transport.c']


def inputs():
    return sorted(set([ROOT/'src'/name for name in SOURCES] +
                      list((ROOT/'src').glob('*.h')) + list((ROOT/'include').glob('*.h')) +
                      [Path(__file__),Path(__file__).with_name('pitch_benchmark.c'),
                       Path(__file__).with_name('pitch_candidate.py')]))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--frames',type=int,default=512,help='use actual SDL obtained buffer frames (default 512)')
    parser.add_argument('--blocks',type=int,default=1000)
    parser.add_argument('--package',type=Path,help='only create a source bundle for an uncommitted Pi test')
    args = parser.parse_args()
    if not 64 <= args.frames <= 4096 or not 100 <= args.blocks <= 10000:
        parser.error('frames must be 64..4096; blocks must be 100..10000')
    if args.package:
        args.package.parent.mkdir(parents=True,exist_ok=True)
        with tarfile.open(args.package,'w:gz') as archive:
            for path in inputs():
                archive.add(path,arcname=str(path.relative_to(ROOT)),recursive=False)
        print(args.package.resolve())
        print('sha256:',hashlib.sha256(args.package.read_bytes()).hexdigest())
        return
    out = ROOT/'build/pitch-benchmark'
    out.mkdir(parents=True,exist_ok=True)
    metadata = {'platform':platform.platform(),'machine':platform.machine(),
                'frames':args.frames,'blocks':args.blocks,
                'source_sha256':{str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs()}}
    compiler = os.environ.get('CC','cc')
    metadata['compiler'] = subprocess.check_output([compiler,'--version'],text=True).splitlines()[0]
    metadata['cpuinfo'] = Path('/proc/cpuinfo').read_text() if Path('/proc/cpuinfo').exists() else ''
    flags = ['-std=c99','-O2','-Wall','-Wextra','-Werror','-I',str(ROOT/'src'),'-I',str(ROOT/'include')]
    metadata['flags'] = flags
    results = []
    with tempfile.TemporaryDirectory(prefix='sugarbee-benchmark-') as temp:
        folder = Path(temp)
        for candidate in [False,True]:
            sources = []
            for name in SOURCES:
                original = (ROOT/'src'/name).read_text()
                target = folder/name
                target.write_text(candidate_source(name,original) if candidate else original)
                sources.append(str(target))
            executable = folder/('candidate' if candidate else 'baseline')
            subprocess.run([compiler,*flags,str(Path(__file__).with_suffix('.c')),*sources,
                            '-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=free',
                            '-lm','-o',str(executable)],check=True)
            for wide in ([False,True] if candidate else [False]):
                label = ('candidate' if candidate else 'baseline')+('-wide' if wide else '-old-band')
                print(label,flush=True)
                result = subprocess.run([str(executable),str(args.frames),str(args.blocks),str(int(wide))],
                                        text=True,capture_output=True,timeout=180)
                if result.returncode:
                    sys.stderr.write(result.stderr)
                    result.check_returncode()
                print(result.stdout,flush=True)
                results.append(label+'\n'+result.stdout)
    (out/'results.txt').write_text('\n'.join(results))
    (out/'metadata.json').write_text(json.dumps(metadata,indent=2)+'\n')
    print('Saved',out/'results.txt',flush=True)


if __name__ == '__main__':
    main()

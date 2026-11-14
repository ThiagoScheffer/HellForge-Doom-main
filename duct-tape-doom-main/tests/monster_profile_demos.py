"""Record/play real demos through the game loop, comparing canonical traces."""
import argparse, hashlib, json, os, shutil, subprocess
from pathlib import Path

def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    p=argparse.ArgumentParser()
    for name in ('baseline','instrumented','iwad','out'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();out=a.out.resolve();out.mkdir(parents=True,exist_ok=True)
    engines={'out':a.baseline.resolve(),'disabled':a.instrumented.resolve(),'enabled':a.instrumented.resolve()}
    summary={'binaries':{m:sha(e) for m,e in engines.items()},'iwad':sha(a.iwad),'runs':[]}
    traces={}
    for action in ('record','play'):
        for mode,engine in engines.items():
            folder=out/(action+'-'+mode);folder.mkdir(exist_ok=True)
            command=[str(engine),'-iwad',str(a.iwad.resolve()),'-bench-demo-trace',str(folder),
                     '-bench-tics','700','-config',str(folder/'doom.cfg'),'-nosound','-nomusic',
                     '-bench-zone-base','0x20000000000']
            if action=='play':shutil.copy2(out/'record-out'/'phase2.lmp',folder/'phase2.lmp')
            command+=['-warp','2','-skill','4','-record','phase2'] if action=='record' else ['-playdemo','phase2']
            if mode=='enabled':command+=['-profilemonsters',str(folder/'probes')]
            env=os.environ.copy();env['TUIN_CRASH_NO_DIALOG']='1';env['HELLFORGE_CRASH_DIR']=str(folder)
            result=subprocess.run(command,cwd=folder,env=env,capture_output=True,timeout=120)
            (folder/'engine.log').write_bytes(result.stdout+result.stderr)
            expected=1 if action=='record' else 0
            if result.returncode!=expected:raise RuntimeError(f'{action}-{mode}: unexpected exit {result.returncode}')
            if action=='record' and (not (folder/'phase2.lmp').exists() or b'recorded' not in result.stderr+result.stdout):
                raise RuntimeError('Recorder did not complete normally')
            trace=(folder/'demo.csv').read_bytes();traces[action,mode]=trace
            if len(trace.splitlines())!=701:raise RuntimeError('Incomplete demo trace')
            summary['runs'].append({'action':action,'mode':mode,'command':command,'exit':result.returncode,
                                   'trace_sha256':hashlib.sha256(trace).hexdigest(),
                                   'matches_baseline':trace==traces[action,'out']})
    summary['demo_bytes_match']=len({sha(out/('record-'+m)/'phase2.lmp') for m in engines})==1
    # Recording/playback flags can be part of extension state: compare each mode
    # against the same action's baseline rather than normalizing any fields.
    (out/'results.json').write_text(json.dumps(summary,indent=2))
    print(json.dumps(summary,indent=2))
    if not summary['demo_bytes_match'] or not all(r['matches_baseline'] for r in summary['runs']):raise RuntimeError('Demo mismatch')

if __name__=='__main__':main()

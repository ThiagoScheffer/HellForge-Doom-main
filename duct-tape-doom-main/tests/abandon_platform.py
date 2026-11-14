"""Only Abandon MAP01's two spawn switches; no broad benchmark matrix."""
import argparse,csv,hashlib,json,subprocess
from pathlib import Path

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for key in ('engine','iwad','wad','out'):parser.add_argument('--'+key,type=Path,required=True)
    a=parser.parse_args();out=a.out.resolve();out.mkdir(parents=True,exist_ok=True)
    results=[];reference=None
    for name,preset,extra in [('off','classic',[]),('on','hibernate',[]),
                              ('reverse','hibernate',['-bench-platform-reverse']),('save-load','hibernate',['-bench-platform-save'])]:
        folder=out/name;folder.mkdir(exist_ok=True)
        cmd=[str(a.engine.resolve()),'-iwad',str(a.iwad.resolve()),'-file',str(a.wad.resolve()),'-warp','1','-skill','4',
             '-monsterbench',str(folder),'-bench-scenario','platform','-bench-preset',preset,'-bench-warmup','0','-bench-tics','350',
             '-bench-verify','-bench-zone-base','0x20000000000','-config',str(folder/'doom.cfg'),'-nosound','-nomusic']+extra
        with (folder/'engine.log').open('w') as log:
            run=subprocess.run(cmd,cwd=folder,stdout=log,stderr=subprocess.STDOUT,timeout=20)
        results.append(dict(case=name,exit=run.returncode,command=cmd));(out/'results.json').write_text(json.dumps(results,indent=2))
        assert run.returncode==0,f'{name} failed: {folder}/engine.log'
        print(name,next(l for l in (folder/'engine.log').read_text(errors='replace').splitlines() if l.startswith('PLATFORM ')),flush=True)
        with (folder/'tics.csv').open(newline='') as f:
            trace=[tuple(r[k] for k in ('state_hash','rng_calls','rng_trace','rng_index')) for r in csv.DictReader(f)]
        assert len(trace)==350
        if name=='off':reference=trace
        if name=='on':assert trace==reference,'hibernation changes platform gameplay/RNG'
    (out/'manifest.json').write_text(json.dumps({str(f.resolve()):hashlib.sha256(f.read_bytes()).hexdigest() for f in (a.engine,a.iwad,a.wad)},indent=2))
    print('PASS both switch orders, one-switch gate, save/load continuation and hibernation off/on state/RNG')

if __name__=='__main__':main()

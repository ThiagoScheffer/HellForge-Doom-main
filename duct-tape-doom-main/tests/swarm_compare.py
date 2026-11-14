"""Small MAP01-only acceptance run; never starts the broad profiling matrix."""
import argparse,csv,hashlib,json,os,statistics,subprocess
from pathlib import Path
def rows(path):
    with path.open(newline='') as f:return list(csv.DictReader(f))
def quantiles(values):
    values=sorted(values);return {key:values[int((len(values)-1)*q)] for key,q in [('median',.5),('p95',.95),('p99',.99)]}|{'mean':statistics.mean(values)}
def main():
    p=argparse.ArgumentParser()
    for key in ('engine','iwad','wad','out'):p.add_argument('--'+key,type=Path,required=True)
    p.add_argument('--activation-only',action='store_true',help='Only test MAP01 gun/use switches, voodoo release and off/on state/RNG equivalence; no timing matrix')
    a=p.parse_args();out=a.out.resolve();out.mkdir(parents=True,exist_ok=True)
    allResults=[]
    protocol=[(True,1,['activation'])] if a.activation_only else [(True,1,['idle','combat','lifecycle']),(False,3,['idle'])]
    for verify,repeats,scenarios in protocol:
        for scenario in scenarios:
            baseline=None
            for repetition in range(repeats):
                for enabled in ([0,1] if repetition%2==0 else [1,0]):
                    folder=out/f'{"verify" if verify else "timing"}-{scenario}-{repetition}-{enabled}';folder.mkdir(exist_ok=True)
                    command=[str(a.engine.resolve()),'-iwad',str(a.iwad.resolve()),'-file',str(a.wad.resolve()),'-warp','1','-skill','4',
                        '-monsterbench',str(folder),'-bench-scenario','chase' if scenario=='lifecycle' else scenario,
                        '-bench-preset','hibernate' if enabled else 'classic','-bench-warmup','70','-bench-tics','350' if verify else '700',
                        '-bench-zone-base','0x20000000000','-config',str(folder/'doom.cfg'),'-nosound','-nomusic']
                    if verify:command+=['-bench-verify']
                    if scenario=='lifecycle':command+=['-bench-lifecycle']
                    env=os.environ.copy();env.update(TUIN_CRASH_NO_DIALOG='1',HELLFORGE_CRASH_DIR=str(folder))
                    with (folder/'engine.log').open('w') as log:subprocess.run(command,cwd=folder,env=env,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=120)
                    data=rows(folder/'tics.csv');trace=rows(folder/'warmup.csv')+data if verify else data
                    state=[tuple(r[k] for k in ('state_hash','rng_calls','rng_trace','rng_index')) for r in trace]
                    if baseline is None:baseline=state
                    if verify and state!=baseline:raise RuntimeError(f'State/RNG mismatch in {folder}')
                    result=dict(verify=verify,scenario=scenario,repeat=repetition,enabled=enabled,command=command,
                        tic_ms=quantiles([int(r['ns'])/1e6 for r in data]),swarm=rows(folder/'swarm.csv')[0])
                    allResults.append(result);print(folder.name,result['tic_ms'],result['swarm'],flush=True)
                    (out/'results.json').write_text(json.dumps(allResults,indent=2))
    (out/'manifest.json').write_text(json.dumps({'engine_sha256':hashlib.sha256(a.engine.read_bytes()).hexdigest(),
        'resources':{str(x.resolve()):hashlib.sha256(x.read_bytes()).hexdigest() for x in (a.iwad,a.wad)}},indent=2))
    print('PASS MAP01 switch/conveyor activation and state/RNG comparison' if a.activation_only else 'PASS MAP01 state/RNG and lifecycle comparisons; timing runs are separate')
if __name__=='__main__':main()

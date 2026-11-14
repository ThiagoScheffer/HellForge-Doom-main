"""Existing in-game host/join path, with one profiled native peer."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path

def main():
    p=argparse.ArgumentParser()
    p.add_argument('--features',action='store_true');p.add_argument('--disabled',action='store_true')
    for name in ('engine','iwad','out'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();out=a.out.resolve();out.mkdir(parents=True,exist_ok=True)
    signature=hashlib.sha256(a.engine.read_bytes()+a.iwad.read_bytes()).hexdigest()
    children=[];logs=[];commands=[]
    try:
        for role in range(2):
            folder=out/str(role);folder.mkdir(exist_ok=True)
            env=os.environ.copy();env.update(LAN_TRACE=str(folder/'trace.txt'),LAN_INGAME='1',LAN_ROLE=str(role),LAN_COUNT='2',
                TUIN_LAN_SIGNATURE=signature,HELLFORGE_LAN_SIGNATURE=signature,
                TUIN_CRASH_NO_DIALOG='1',HELLFORGE_CRASH_DIR=str(folder))
            for key in ('LAN_FEATURES','LAN_LONG'):env.pop(key,None)
            if a.features:env.update(LAN_FEATURES='1',LAN_LONG='1')
            command=[str(a.engine.resolve()),'-iwad',str(a.iwad.resolve()),'-warp','1','-skill','2',
                     '-config',str(folder/'doom.cfg'),'-nosound','-nomusic']
            if role and not a.disabled:command+=['-profilemonsters',str(folder/'probes')]
            commands.append(command);log=(folder/'engine.log').open('w');logs.append(log)
            children.append(subprocess.Popen(command,cwd=folder,env=env,stdout=log,stderr=subprocess.STDOUT))
        codes=[child.wait(timeout=120) for child in children]
    finally:
        for child in children:
            if child.poll() is None:child.kill();child.wait()
        for log in logs:log.close()
    traces=[dict(line.split() for line in (out/str(i)/'trace.txt').read_text().splitlines()) for i in range(2)]
    common=sorted(set(traces[0])&set(traces[1]),key=int)
    mismatches=[tic for tic in common if traces[0][tic]!=traces[1][tic]]
    result={'commands':commands,'codes':codes,'common_tics':len(common),'mismatches':mismatches,
            'engine_sha256':hashlib.sha256(a.engine.read_bytes()).hexdigest(),'test_content_signature':signature,
            'features':a.features,'profiling_disabled':a.disabled}
    (out/'results.json').write_text(json.dumps(result,indent=2));print(result)
    if any(codes) or len(common)<300 or mismatches:raise RuntimeError('Native lobby comparison failed')

if __name__=='__main__':main()

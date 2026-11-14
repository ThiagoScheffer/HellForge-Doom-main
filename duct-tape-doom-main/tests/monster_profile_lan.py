"""Two localhost peers using the existing native UDP backend (never headless)."""
import argparse, json, os, subprocess, hashlib
from pathlib import Path

def main():
    p=argparse.ArgumentParser()
    for name in ('engine','iwad','out'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();out=a.out.resolve();out.mkdir(parents=True,exist_ok=True)
    children=[];logs=[];commands=[]
    try:
        for i in range(2):
            folder=out/str(i);folder.mkdir(exist_ok=True)
            env=os.environ.copy();env.update(LAN_TRACE=str(folder/'trace.txt'),TUIN_CRASH_NO_DIALOG='1',HELLFORGE_CRASH_DIR=str(folder))
            command=[str(a.engine.resolve()),'-iwad',str(a.iwad.resolve()),'-warp','1','-skill','2',
                     '-config',str(folder/'doom.cfg'),'-nosound','-nomusic',
                     '-lan-token','0123456789abcdef0123456789abcdef','-port',str(26891+i),
                     '-net',str(i+1),f'127.0.0.1:{26892-i}']
            if i==1:command+=['-profilemonsters',str(folder/'probes')]
            log=(folder/'engine.log').open('w');logs.append(log);commands.append(command)
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
            'engine_sha256':hashlib.sha256(a.engine.read_bytes()).hexdigest()}
    (out/'results.json').write_text(json.dumps(result,indent=2));print(result)
    if any(codes) or len(common)<300 or mismatches:raise RuntimeError('Native UDP comparison failed')

if __name__=='__main__':main()

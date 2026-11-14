"""Serial remaining measurement queue; never run performance jobs concurrently."""
import argparse, json, subprocess, sys
from pathlib import Path

def main():
    p=argparse.ArgumentParser();p.add_argument('--root',type=Path,required=True)
    p.add_argument('--iwad',type=Path,required=True);p.add_argument('--wad',type=Path,required=True)
    p.add_argument('--label',default='v3')
    a=p.parse_args();root=a.root.resolve();runner=root/'duct-tape-doom-main/tests/monster_profile_runs.py'
    common=['--iwad',str(a.iwad.resolve()),'--wad',str(a.wad.resolve()),'--scenarios','chase','--repeats','5',
            '--zone-base','0x20000000000']
    jobs=[('final-verify','profile-off','profile-on',['--maps','1,32,3,31,2','--scenarios','idle,chase,combat','--verify','--repeats','2','--tics','700','--zone-base','0x20000000000']),
          ('final-lifecycle','profile-off','profile-on',['--maps','2','--verify','--lifecycle','--repeats','2','--warmup','35','--tics','350','--zone-base','0x20000000000']),
          ('final-extended-verify','profile-off','profile-on',['--maps','1,32','--presets','fluid,solid,tactics,feeding,frenzy,recovery','--verify','--repeats','2','--tics','700']),
          ('native-verify','native-off','native-on',['--maps','1,32,2','--native','--verify','--repeats','2','--tics','350']),
          ('native-extended-verify','native-off','native-on',['--maps','1,32','--presets','fluid,solid,tactics,feeding,frenzy,recovery','--native','--verify','--repeats','2','--tics','700']),
          ('current-classic','profile-off','profile-on',['--maps','1,32,3,31,2','--cpu','0','--scenarios','idle,chase,combat']),
          ('pinned-control','profile-off','profile-on',['--maps','2','--cpu','0','--scenarios','idle,chase,combat']),
          ('stock-control','profile-off','profile-on',['--maps','1','--stock','--cpu','0','--scenarios','idle,chase,combat']),
          ('arguments','profile-off','profile-args',['--maps','1,32','--cpu','0']),
          ('native-matrix','native-off','native-on',['--maps','1,32,3,31,2','--native','--scenarios','idle,chase,combat']),
          ('native-fenced','native-off','native-on',['--maps','1,32,2','--native','--fenced','--repeats','1']),
          ('optional-matrix','profile-off','profile-on',['--maps','1,32','--presets','fluid,solid,tactics,feeding,frenzy','--cpu','0']),
          ('native-optional','native-off','native-on',['--maps','1,32','--presets','fluid,solid,tactics,feeding,frenzy','--native'])]
    for name,off,on,extra in jobs:
        name=a.label+'-'+name
        binary='doom-monster-bench.exe' if off.startswith('native') else 'doom-headless.exe'
        command=[sys.executable,str(runner),'--baseline',str(root/'build'/off/'Release'/binary),
                 '--instrumented',str(root/'build'/on/'Release'/binary),'--out',str(root/'build'/name),*common,*extra]
        print('START',name,flush=True)
        subprocess.run(command,cwd=root,check=True)
        subprocess.run([sys.executable,str(root/'duct-tape-doom-main/tests/monster_profile_archive.py'),
                        str(root/'build'/name/'manifest.json'),str(root/'build'/name/'measured-sources.zip')],cwd=root,check=True)
        subprocess.run([sys.executable,str(root/'duct-tape-doom-main/tests/monster_profile_analysis.py'),str(root/'build'/name)],cwd=root,check=True)
        print('COMPLETE',name,flush=True)

if __name__=='__main__':main()

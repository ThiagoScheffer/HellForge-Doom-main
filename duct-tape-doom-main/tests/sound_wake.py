import csv,json,os,subprocess,statistics
from pathlib import Path
r=Path(__file__).resolve().parents[2];out=r/'build/sf2012-wake';out.mkdir(parents=True,exist_ok=True);results=[]
for scenario in ('idle','shotwake'):
 d=out/scenario;d.mkdir(exist_ok=True);g=d/'graphics.cfg'
 g.write_text('fullscreen 0\nscale 100\nfps 0\nweather 0\nmonster_recovery 0\nadvanced_tactics 0\naggressive_ai 0\nmod_fluid_monsters 0\nmod_fluid_weapons 0\nmonster_feeding 0\nheadshot_frenzy 0\nmonster_hibernation 0\nsprite_frustum_filter 1\n')
 cmd=[str(r/'build/native-on/Release/doom-monster-bench.exe'),'-iwad',str(r/'test-references/DOOM2.WAD'),'-file',str(r/'test-references/SF2012_final.wad'),'-warp','2','-skill','4','-solidmaps','-uzmovement','-monsterbench',str(d),'-bench-scenario',scenario,'-bench-preset','classic','-bench-wake-audit','-bench-warmup','35','-bench-tics','175','-bench-zone-base','0x20000000000','-profilemonsters',str(d/'profile'),'-graphics',str(g),'-config',str(d/'doom.cfg'),'-width','960','-height','540','-fifo-present','-nosound','-nomusic']
 env=os.environ.copy();env.update(TUIN_CRASH_NO_DIALOG='1',HELLFORGE_CRASH_DIR=str(d),HELLFORGE_MAP_CACHE=str(r/'build/sf2012-fix/cache'))
 with (d/'engine.log').open('w') as f:p=subprocess.run(cmd,cwd=d,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=90)
 if p.returncode:raise RuntimeError(f'{scenario}: {p.returncode}')
 with (d/'tics.csv').open() as f:rows=list(csv.DictReader(f))
 with (d/'wake.csv').open() as f:wake=list(csv.DictReader(f))
 def stat(key,skip=0):
  vals=sorted(int(x[key])/1e6 for x in rows[skip:]);return dict(median=statistics.median(vals),p95=vals[int((len(vals)-1)*.95)],maximum=max(vals))
 result=dict(scenario=scenario,command=cmd,tic_ms=stat('ns'),frame_ms=stat('frame_ns'),late_tic_ms=stat('ns',35),wake=wake)
 results.append(result);(out/'results.json').write_text(json.dumps(results,indent=2));print(scenario,'tic',result['tic_ms'],'frame',result['frame_ms'],'initial/final',wake[:2],wake[-1],flush=True)

"""Focused SF2012 MAP02 equivalence/timing checks for stack-local ambush reuse."""
import csv,json,os,statistics,subprocess,sys
from pathlib import Path
r=Path(__file__).resolve().parents[2];out=r/'build/ambush-sight';out.mkdir(parents=True,exist_ok=True)
engine=r/'build/native-on/Release/doom-monster-bench.exe'
results=[]
def rows(p):
 with p.open(newline='') as f:return list(csv.DictReader(f))
def run(name,enabled,extra=(),length=350):
 d=out/name;d.mkdir(exist_ok=True);g=d/'graphics.cfg'
 g.write_text('fullscreen 0\nscale 100\nfps 0\nweather 0\nmonster_recovery 0\nadvanced_tactics 0\naggressive_ai 0\nmod_fluid_monsters 0\nmod_fluid_weapons 0\nmonster_feeding 0\nheadshot_frenzy 0\nmonster_hibernation 0\nsprite_frustum_filter 1\n')
 cmd=[str(engine),'-iwad',str(r/'test-references/DOOM2.WAD'),'-file',str(r/'test-references/SF2012_final.wad'),'-warp','2','-skill','4','-solidmaps','-uzmovement','-monsterbench',str(d),'-bench-scenario','shotwake','-bench-preset','classic','-bench-wake-audit','-bench-verify','-bench-warmup','35','-bench-tics',str(length),'-bench-zone-base','0x20000000000','-profilemonsters',str(d/'profile'),'-graphics',str(g),'-config',str(d/'doom.cfg'),'-width','960','-height','540','-fifo-present','-nosound','-nomusic',*extra]
 if enabled:cmd+=['-ambushsightreuse']
 env=os.environ.copy();env.update(TUIN_CRASH_NO_DIALOG='1',HELLFORGE_CRASH_DIR=str(d),HELLFORGE_MAP_CACHE=str(r/'build/sf2012-fix/cache'))
 with (d/'engine.log').open('w') as f:p=subprocess.run(cmd,cwd=d,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=90)
 if p.returncode:raise RuntimeError(f'{name} exit {p.returncode}: {(d/"engine.log").read_text(errors="replace")[-900:]}')
 t=rows(d/'warmup.csv')+rows(d/'tics.csv');events=rows(d/'wake-events.csv');sight=rows(d/'wake-sight.csv');measured=rows(d/'tics.csv')
 def stats(key,start,end):
  v=sorted(int(x[key])/1e6 for x in measured[start:end]);return dict(median=statistics.median(v),p95=v[int((len(v)-1)*.95)])
 result=dict(name=name,enabled=enabled,command=cmd,windows={label:dict(tic_ms=stats('ns',a,b),frame_ms=stats('frame_ns',a,b)) for label,a,b in [('first8',0,8),('settling',8,35),('later',35,length)]},
  first_transitions=sum(x['first_idle_to_see']=='1' for x in events),all_transitions=sum(x['reason'] in ('sound','sight') for x in events),teleport126_success=sum(x['reason']=='teleport126_success' for x in events),
  reused=sum(int(x['reused_false']) for x in sight),false_duplicates=sum(int(x['false_duplicates']) for x in sight))
 results.append(result);(out/'results.json').write_text(json.dumps(results,indent=2));print(name,'first',result['first_transitions'],'total',result['all_transitions'],'reused',result['reused'],'later',result['windows']['later'],flush=True)
 return [tuple(x[k] for k in ('state_hash','rng_calls','rng_trace','rng_index')) for x in t],[tuple((k,v) for k,v in x.items() if k!='thinker_address') for x in events],rows(d/'wake.csv')
if '--scope-only' in sys.argv:
 ref=run('scopes-off',False)
 assert run('scopes-on',True)==ref,'Scope follow-up differs'
 print('PASS isolated before/first8/later cost windows and exact equivalence',flush=True)
else:
 for pair in range(2):
  ref=None
  for enabled in ([False,True] if pair==0 else [True,False]):
   trace=run(f'pair{pair}-{int(enabled)}',enabled)
   if ref is None:ref=trace
   else:assert trace==ref,f'pair {pair}: state/RNG/events/populations differ'
  print('PASS paired canonical state, RNG, transitions and population',pair,flush=True)
 ref=run('lifecycle-off',False,('-bench-lifecycle','-bench-ambush-behind-test','-bench-teleport-test'),300)
 assert run('lifecycle-on',True,('-bench-lifecycle','-bench-ambush-behind-test','-bench-teleport-test'),300)==ref,'Lifecycle state/RNG/events/populations differ'
 print('PASS actual save/load, lifecycle, ambush-behind and type126 teleport equivalence',flush=True)
 
"""Focused native sprite filtering comparisons with external child timeouts."""
import argparse,csv,hashlib,json,os,statistics,subprocess,importlib.util
from pathlib import Path

def rows(p):
    with p.open(newline='') as f:return list(csv.DictReader(f))
def stats(v):
    v=sorted(v);return dict(median=statistics.median(v),p95=v[int((len(v)-1)*.95)],maximum=v[-1],mean=statistics.mean(v))
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ('engine','iwad','wad','out'):p.add_argument('--'+key,type=Path,required=True)
    p.add_argument('--stage',choices=['probe','bench','verify','fallback'],default='probe')
    p.add_argument('--cache',type=Path)
    p.add_argument('--groups',help='Override scene groups for a focused follow-up')
    p.add_argument('--single',type=int,choices=[0,1],help='One side only for baseline repeatability checks')
    p.add_argument('--short-verify',action='store_true',help='40-tic state/RNG check without lifecycle injection')
    a=p.parse_args();out=a.out.resolve();out.mkdir(parents=True,exist_ok=True)
    results=[]
    groups=['stock','oku'] if a.stage!='fallback' else ['plain','reflection','flashlight','liquid']
    if a.groups:groups=a.groups.split(',')
    if a.stage=='fallback':
        spec=importlib.util.spec_from_file_location('filter_arena',Path(__file__).with_name('ragdoll_arena.py'));arena=importlib.util.module_from_spec(spec);spec.loader.exec_module(arena)
        fixtures=out/'fixtures';fixtures.mkdir(exist_ok=True)
        arena.write(fixtures/'gray.wad','zombie',False);arena.FLOOR='FWATER1';arena.write(fixtures/'water.wad','zombie',False)
    repeatCount=2 if a.stage=='bench' else 1
    for group in groups:
        reference=None
        for repeat in range(repeatCount):
            for enabled in ([a.single] if a.single is not None else [0,1] if repeat%2==0 else [1,0]):
                d=out/f'{group}-{repeat}-{enabled}';d.mkdir(exist_ok=True)
                base='fullscreen 0\nscale 100\nfps 0\nmonster_recovery 0\nadvanced_tactics 0\naggressive_ai 0\nmod_fluid_monsters 0\nmod_fluid_weapons 0\nmonster_feeding 0\nheadshot_frenzy 0\nmonster_hibernation 0\n'
                visual=a.stage in ('probe','fallback')
                if visual:base+='weather 0\nmoving_relight 0\neye_adaptation 0\n'
                if group=='liquid':base+='reflections 0\n'
                if group=='reflection':base+='reflections 1\n'
                if group=='flashlight':base+='flashlight_shadows 1\n'
                graphics=d/'graphics.cfg';graphics.write_text(base+f'sprite_frustum_filter {enabled}\n')
                warm,length=(35,140) if a.stage=='bench' else (5,10)
                if a.stage=='verify':warm,length=35,300
                if a.stage=='verify' and a.short_verify:warm,length=5,35
                if a.stage=='fallback':warm,length=5,20
                cmd=[str(a.engine.resolve()),'-iwad',str(a.iwad.resolve()),'-warp','1','-skill','4','-monsterbench',str(d),
                     '-bench-scenario','idle','-bench-preset','classic','-bench-warmup',str(warm),'-bench-tics',str(length),
                     '-bench-zone-base','0x20000000000','-config',str(d/'doom.cfg'),'-graphics',str(graphics),
                     '-width','960','-height','540','-fifo-present','-nosound','-nomusic','-bench-progress']
                if group=='oku':cmd+=['-file',str(a.wad.resolve())]
                if a.stage=='fallback':cmd+=['-file',str(fixtures/('water.wad' if group in ('reflection','liquid') else 'gray.wad'))]
                if group=='plain':cmd+=['-bench-filter-fixture']
                if group=='flashlight':cmd+=['-flashlight','-bench-fixed-light-clock']
                if visual:cmd+=['-bench-image',str(d/'frame.png'),'-bench-image-frame','15' if a.stage=='fallback' else '10']
                if a.stage=='verify':
                    cmd+=['-bench-verify']
                    if not a.short_verify:cmd+=['-bench-lifecycle']
                env=os.environ.copy();env.update(HELLFORGE_MAP_CACHE=str((a.cache or out/'cache').resolve()),HELLFORGE_CRASH_DIR=str(d/'crashes'),TUIN_CRASH_NO_DIALOG='1')
                with (d/'engine.log').open('w') as f:
                    try:r=subprocess.run(cmd,cwd=d,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=120);code=r.returncode
                    except subprocess.TimeoutExpired:code='timeout'
                result=dict(group=group,repeat=repeat,enabled=enabled,exit=code,command=cmd)
                if code!=0:
                    results.append(result);(out/'results.json').write_text(json.dumps(results,indent=2));raise RuntimeError(f'{d}: {code}')
                data=rows(d/'tics.csv');visibility=rows(d/'visibility.csv')[warm:]
                result.update(tic_ms=stats([int(x['ns'])/1e6 for x in data]),frame_ms=stats([int(x['frame_ns'])/1e6 for x in data]),
                    filter_ms=stats([int(x['filter_ns'])/1e6 for x in visibility]),
                    actors={key:statistics.mean(int(x[key]) for x in visibility) for key in ('examined','rejected','retained','eligible','secondary','pose','wound','liquid','voxel')})
                results.append(result);(out/'results.json').write_text(json.dumps(results,indent=2));print(d.name,result['frame_ms'],result['actors'],flush=True)
                if a.stage=='verify':
                    trace=[tuple(x[k] for k in ('state_hash','rng_calls','rng_trace','rng_index')) for x in rows(d/'warmup.csv')+data]
                    assert len(trace)==warm+length
                    if reference is None:reference=trace
                    else:
                        assert trace==reference,f'{group} gameplay/RNG differs'
                        print('PASS',group,'canonical gameplay and RNG trace',flush=True)
                if a.stage=='fallback' and enabled:
                    if group=='plain':assert result['actors']['pose']>0 and result['actors']['wound']>0,'presentation fallbacks not exercised'
                    if group in ('reflection','flashlight'):assert result['actors']['secondary']==result['actors']['examined'],'secondary view bypass missing'
                    if group=='liquid':assert result['actors']['liquid']>0,'liquid fallback not exercised'
            if visual and a.single is None:
                from PIL import Image,ImageChops
                left=Image.open(out/f'{group}-{repeat}-0/frame.png').convert('RGB');right=Image.open(out/f'{group}-{repeat}-1/frame.png').convert('RGB')
                delta=ImageChops.difference(left,right);box=delta.getbbox();print('IMAGE',group,'identical' if box is None else f'different bbox {box}',flush=True)
                (out/f'{group}-image.json').write_text(json.dumps(dict(identical=box is None,bbox=box)))
                if a.stage=='fallback':assert box is None,f'{group} controlled image differs'
    (out/'manifest.json').write_text(json.dumps({str(x.resolve()):hashlib.sha256(x.read_bytes()).hexdigest() for x in (a.engine,a.iwad,a.wad)},indent=2))

if __name__=='__main__':main()

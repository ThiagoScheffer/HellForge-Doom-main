"""Bounded oku MAP01 isolation. Every child has a timeout; keep partial results."""
import argparse, csv, ctypes, hashlib, json, os, platform, re, statistics, subprocess, time
from pathlib import Path

def rows(path):
    with path.open(newline='') as f: return list(csv.DictReader(f))

def distribution(values):
    values=sorted(values)
    if not values: return None
    return dict(mean=statistics.mean(values),median=statistics.median(values),
                p95=values[int((len(values)-1)*.95)],p99=values[int((len(values)-1)*.99)],maximum=values[-1])

def peak_memory(child):
    if os.name!='nt': return None
    from ctypes import wintypes as w
    class Memory(ctypes.Structure):
        _fields_=[('cb',w.DWORD),('faults',w.DWORD)]+[(n,ctypes.c_size_t) for n in
            ('peak','working','peak_paged','paged','peak_nonpaged','nonpaged','pagefile','peak_pagefile','private')]
    memory=Memory();memory.cb=ctypes.sizeof(memory)
    api=ctypes.WinDLL('psapi').GetProcessMemoryInfo
    api.argtypes=[w.HANDLE,ctypes.c_void_p,w.DWORD];api.restype=w.BOOL
    return int(memory.peak) if api(int(child._handle),ctypes.byref(memory),memory.cb) else None

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ('headless','native','iwad','wad','out'):p.add_argument('--'+key,type=Path,required=True)
    p.add_argument('--timeout',type=float,default=60)
    p.add_argument('--cache',type=Path,help='Reuse an existing isolated lighting cache')
    p.add_argument('--cases',default='headless,no-monsters,hibernate,alert,native,native-no-monsters,native-hibernate,profile')
    a=p.parse_args();out=a.out.resolve();out.mkdir(parents=True,exist_ok=True)
    manifest=dict(platform=platform.platform(),cpu=platform.processor(),resolution='960x540 FIFO',
        hashes={str(f.resolve()):hashlib.sha256(f.read_bytes()).hexdigest() for f in (a.headless,a.native,a.iwad,a.wad)},
        notes='Single bounded runs. Optional enhanced AI/Fluid disabled. Fixed initial camera; godmode/plasma benchmark setup. No deferred-spawning implementation.')
    (out/'manifest.json').write_text(json.dumps(manifest,indent=2))
    results=[]
    for case in a.cases.split(','):
        native=case.startswith('native') or case=='profile'
        verify=case.startswith('verify') or case.startswith('lifecycle')
        enabled=case.endswith('hibernate') or case=='hibernate'
        probes=case=='profile'
        d=out/case;d.mkdir(exist_ok=True)
        warm,length=(5,70) if native else (70,350)
        if probes:warm,length=5,35
        if case=='native-menu':warm,length=5,15
        if verify:warm,length=70,350
        preset='hibernate' if enabled else 'classic'
        cmd=[str((a.native if native else a.headless).resolve()),'-iwad',str(a.iwad.resolve()),'-file',str(a.wad.resolve()),
             '-warp','1','-skill','4','-monsterbench',str(d),'-bench-scenario','alert' if case=='alert' else 'idle',
             '-bench-preset',preset,'-bench-warmup',str(warm),'-bench-tics',str(length),'-bench-progress',
             '-bench-zone-base','0x20000000000','-config',str(d/'doom.cfg'),'-nosound','-nomusic']
        if 'no-monsters' in case:cmd+=['-nomonsters']
        if verify:cmd+=['-bench-verify']
        if case.startswith('lifecycle'):cmd+=['-bench-lifecycle','-bench-teleport-test']
        if native:
            graphics=d/'graphics.cfg'
            graphics.write_text('fullscreen 0\nscale 100\nfps 0\nmonster_recovery 0\nadvanced_tactics 0\naggressive_ai 0\n'
                'mod_fluid_monsters 0\nmod_fluid_weapons 0\nmonster_feeding 0\nheadshot_frenzy 0\n'
                f'monster_hibernation {int(enabled)}\nswarm_debug 0\n')
            cmd+=['-graphics',str(graphics),'-width','960','-height','540','-fifo-present','-bench-menu-test']
        if probes:cmd+=['-profilemonsters',str(d/'probes'),'-profilespikes']
        env=os.environ.copy();env.update(HELLFORGE_MAP_CACHE=str((a.cache or out/'cache').resolve()),HELLFORGE_CRASH_DIR=str(d/'crashes'),TUIN_CRASH_NO_DIALOG='1')
        started=time.perf_counter();peak=0;timed_out=False
        with (d/'engine.log').open('w') as log:
            child=subprocess.Popen(cmd,cwd=d,env=env,stdout=log,stderr=subprocess.STDOUT)
            try:
                while child.poll() is None:
                    peak=max(peak,peak_memory(child) or 0)
                    if time.perf_counter()-started>a.timeout:
                        timed_out=True;child.kill();break
                    time.sleep(.025)
                child.wait()
            finally:
                if child.poll() is None:child.kill();child.wait()
        report=dict(case=case,exit=child.returncode,timeout=timed_out,seconds=time.perf_counter()-started,
                    peak_working_set_bytes=peak or None,command=cmd)
        log=(d/'engine.log').read_text(errors='replace')
        report['menu']=[l for l in log.splitlines() if 'BENCH_MENU' in l]
        report['setup_stages']=[l for l in log.splitlines() if 'MONSTER_STAGE' in l]
        report['loading']=[l for l in log.splitlines() if '3D loading:' in l]
        report['initial_population']=(d/'scenario.txt').read_text() if (d/'scenario.txt').exists() else None
        setup=re.search(r'map_setup_ns (\d+)',report['initial_population'] or '')
        report['map_setup_ms']=int(setup.group(1))/1e6 if setup else None
        report['partial_samples']=len(rows(d/'progress.csv')) if (d/'progress.csv').exists() else 0
        data=rows(d/'tics.csv') if (d/'tics.csv').exists() else []
        if data:
            report['tic_ms']=distribution([int(r['ns'])/1e6 for r in data])
            all_samples=rows(d/'progress.csv');report['first_tic_ms']=int(all_samples[0]['tic_ns'])/1e6
            if native:
                report['first_frame_ms']=int(all_samples[0]['frame_ns'])/1e6
                report['frame_ms']=distribution([int(r['frame_ns'])/1e6 for r in data])
                intervals=[int(r['frame_interval_ns'])/1e6 for r in data if int(r['frame_interval_ns'])>0]
                report['frame_interval_ms']=distribution(intervals)
                report['average_fps']=1000/statistics.mean(intervals) if intervals else None
        (d/'result.json').write_text(json.dumps(report,indent=2));results.append(report)
        (out/'results.json').write_text(json.dumps(results,indent=2))
        print(case,'TIMEOUT' if timed_out else f'exit {child.returncode}',f'{report["seconds"]:.2f}s',
              'tic',report.get('tic_ms'),'frames',report.get('frame_ms'),'menu',report['menu'],flush=True)
    for first,second in [('verify','verify-hibernate'),('lifecycle','lifecycle-hibernate')]:
        if not(out/first/'tics.csv').exists() or not(out/second/'tics.csv').exists():continue
        left=rows(out/first/'warmup.csv')+rows(out/first/'tics.csv');right=rows(out/second/'warmup.csv')+rows(out/second/'tics.csv')
        assert len(left)==len(right)==420
        assert all(all(x[k]==y[k] for k in ('state_hash','rng_calls','rng_trace','rng_index')) for x,y in zip(left,right)),first+' state/RNG mismatch'
        print('PASS',first,'state/RNG off/on',flush=True)

if __name__=='__main__':main()

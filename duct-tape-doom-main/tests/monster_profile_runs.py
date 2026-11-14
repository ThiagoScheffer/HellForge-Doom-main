"""Phase 2 runner. Isolated outputs; no user profile, release artifact or WAD edits."""
import argparse, csv, hashlib, json, os, platform, statistics, subprocess, time
from pathlib import Path

def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def rows(path):
    with Path(path).open(newline="") as file:
        return list(csv.DictReader(file))

def stats(values):
    values=sorted(values)
    return dict(median=statistics.median(values), p95=values[int((len(values)-1)*.95)],
                p99=values[int((len(values)-1)*.99)], maximum=values[-1])

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--baseline",type=Path,required=True);p.add_argument("--instrumented",type=Path,required=True)
    p.add_argument("--iwad",type=Path,required=True);p.add_argument("--wad",type=Path,required=True)
    p.add_argument("--out",type=Path,required=True);p.add_argument("--maps",default="1,32,3,31,2")
    p.add_argument("--repeats",type=int,default=5);p.add_argument("--warmup",type=int,default=350)
    p.add_argument("--tics",type=int,default=2100);p.add_argument("--scenarios",default="idle,chase,combat")
    p.add_argument("--presets",default="classic");p.add_argument("--verify",action="store_true")
    p.add_argument("--native",action="store_true");p.add_argument("--fenced",action="store_true")
    p.add_argument("--stock",action="store_true");p.add_argument("--resume",action="store_true")
    p.add_argument("--dump-tic",type=int,default=-1)
    p.add_argument("--lifecycle",action="store_true")
    p.add_argument("--cpu",type=int,help="Pin simulation benchmark to one logical CPU; leave unset for native worker measurements")
    p.add_argument("--zone-base",help="Windows benchmark-only fixed zone address for address-dependent AI comparisons")
    a=p.parse_args();out=a.out.resolve();out.mkdir(parents=True,exist_ok=True)
    root=Path(__file__).resolve().parents[2]
    engines={"out":a.baseline.resolve(),"disabled":a.instrumented.resolve(),"enabled":a.instrumented.resolve()}
    manifest={"base_commit":subprocess.check_output(["git","rev-parse","HEAD"],cwd=root,text=True).strip(),
              "status":subprocess.check_output(["git","status","--short"],cwd=root,text=True),
              "platform":platform.platform(),"cpu":platform.processor(),"arguments":vars(a).copy(),
              "sha256":{str(x.resolve()):digest(x) for x in [a.baseline,a.instrumented,a.iwad,a.wad]}}
    manifest["arguments"]={k:str(v) if isinstance(v,Path) else v for k,v in manifest["arguments"].items()}
    manifest['graphics_environment']={key:os.environ.get(key) for key in ('SDL_GPU_DRIVER','SDL_VIDEO_DRIVER','SDL_VIDEODRIVER','SDL_RENDER_DRIVER')}
    manifest_path=out/"manifest.json"
    manifest["timing_valid"]=not a.verify
    manifest["source_sha256"]={str(path.relative_to(root)):digest(path) for path in (root/"duct-tape-doom-main").rglob("*")
                              if path.is_file() and (path.suffix in (".c",".cpp",".h",".inc",".cmake",".py") or path.name=="CMakeLists.txt") and "third_party" not in path.parts}
    manifest["build_cache_sha256"]={str(engine.parent.parent/"CMakeCache.txt"):digest(engine.parent.parent/"CMakeCache.txt")
                                     for engine in set(engines.values()) if (engine.parent.parent/"CMakeCache.txt").exists()}
    if manifest_path.exists() and a.resume:
        old=json.loads(manifest_path.read_text())
        if old["sha256"]!=manifest["sha256"]:raise RuntimeError("Resume binaries/resources differ")
    manifest_path.write_text(json.dumps(manifest,indent=2))
    results=[];comparisons=[]
    for preset in a.presets.split(","):
        for mapnum in map(int,a.maps.split(",")):
            for scenario in a.scenarios.split(","):
                group=[]
                for repeat in range(a.repeats):
                    modes=["out","disabled","enabled"] if repeat%2==0 else ["enabled","disabled","out"]
                    for mode in modes:
                        folder=out/f"{preset}-map{mapnum:02}-{scenario}-{repeat}-{mode}"
                        folder.mkdir(exist_ok=True)
                        command=[str(engines[mode]),"-iwad",str(a.iwad.resolve()),"-warp",str(mapnum),"-skill","4",
                                 "-monsterbench",str(folder),"-bench-scenario",scenario,"-bench-preset",preset,
                                 "-bench-warmup",str(a.warmup),"-bench-tics",str(a.tics),
                                 "-config",str(folder/"doom.cfg"),"-nosound","-nomusic"]
                        if not a.stock:command+=["-file",str(a.wad.resolve())]
                        if mode=="enabled":command+=["-profilemonsters",str(folder/"probes")]
                        if preset=="fluid":command+=["-fluidmonsters"]
                        if preset=="solid":command+=["-solidmaps"]
                        if a.verify:command+=["-bench-verify"]
                        if a.dump_tic>=0:command+=["-bench-dump-tic",str(a.dump_tic)]
                        if a.lifecycle:command+=["-bench-lifecycle"]
                        if a.zone_base:command+=["-bench-zone-base",a.zone_base]
                        if a.native:
                            graphics=folder/"graphics.cfg"
                            graphics.write_text("fullscreen 0\nscale 100\ntexel_lighting 0\nemissive 1\nweather 0\nfps 0\nmap_light_cache 1\n")
                            command+=["-graphics",str(graphics),"-width","960","-height","540","-fifo-present"]
                        if a.fenced:command+=["-bench-fenced"]
                        env=os.environ.copy()
                        env.update(HELLFORGE_MAP_CACHE=str(out/"lighting-cache"),TUIN_MAP_CACHE=str(out/"lighting-cache"),
                                   HELLFORGE_CRASH_DIR=str(folder),TUIN_CRASH_NO_DIALOG="1")
                        if not(a.resume and (folder/"complete.json").exists()):
                            started=time.perf_counter()
                            with (folder/"engine.log").open("w") as log:
                                run=subprocess.Popen(command,cwd=folder,env=env,stdout=log,stderr=subprocess.STDOUT)
                                if a.cpu is not None:
                                    if os.name=="nt":
                                        import ctypes
                                        api=ctypes.WinDLL("kernel32",use_last_error=True)
                                        api.SetProcessAffinityMask.argtypes=[ctypes.c_void_p,ctypes.c_size_t]
                                        if not api.SetProcessAffinityMask(int(run._handle),1<<a.cpu):
                                            run.terminate();run.wait();raise ctypes.WinError(ctypes.get_last_error())
                                    else:os.sched_setaffinity(run.pid,{a.cpu})
                                try:code=run.wait(timeout=1200)
                                except subprocess.TimeoutExpired:
                                    run.kill();run.wait();raise
                            if code:raise RuntimeError(f"{folder.name}: exit {code}; see engine.log")
                            data=rows(folder/"tics.csv")
                            if len(data)!=a.tics:raise RuntimeError("Wrong sample count")
                            (folder/"complete.json").write_text(json.dumps({"seconds":time.perf_counter()-started,"command":command}))
                        data=rows(folder/"tics.csv");summary={"preset":preset,"map":mapnum,"scenario":scenario,"repeat":repeat,"mode":mode,
                            "tic_ms":stats([int(r["ns"])/1e6 for r in data]),"rng_calls":data[-1]["rng_calls"],
                            "rng_trace":data[-1]["rng_trace"],"folder":str(folder)}
                        if a.native:
                            summary["frame_ms"]=stats([int(r["frame_ns"])/1e6 for r in data])
                            summary["frame_interval_ms"]=stats([int(r["frame_interval_ns"])/1e6 for r in data if int(r["frame_interval_ns"])])
                        results.append(summary)
                        compared=rows(folder/"warmup.csv")+data if a.verify else data
                        if a.verify and len(compared)!=a.tics+min(7000,max(0,a.warmup)):raise RuntimeError('Incomplete warmup state trace')
                        group.append((mode,repeat,compared))
                        print(folder.name,round(summary["tic_ms"]["median"],4),"ms",flush=True)
                        (out/"results.json").write_text(json.dumps(results,indent=2))
                if a.verify:
                    reference=group[0][2]
                    for mode,repeat,data in group[1:]:
                        mismatch=next((int(x['tic']) for x,y in zip(reference,data)
                            if any(x[k]!=y[k] for k in ("state_hash","rng_calls","rng_trace","rng_index"))),None)
                        comparisons.append(dict(preset=preset,map=mapnum,scenario=scenario,mode=mode,repeat=repeat,first_mismatch=mismatch))
                    (out/"comparisons.json").write_text(json.dumps(comparisons,indent=2))
    print("Results:",out)

if __name__=="__main__":main()

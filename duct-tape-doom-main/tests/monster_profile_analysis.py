"""Summarize raw Phase 2 results; does not infer an optimization speedup."""
import argparse,collections,csv,json,statistics
from pathlib import Path
from monster_profile_runs import stats

def read(path):
    with path.open(newline="") as f:return list(csv.DictReader(f))

def analyze(root):
    results=json.loads((root/"results.json").read_text())
    groups=collections.defaultdict(lambda:collections.defaultdict(list))
    for r in results:groups[(r["preset"],r["map"],r["scenario"])][r["mode"]].append(r)
    summary=[]
    for key,modes in groups.items():
        row={"preset":key[0],"map":key[1],"scenario":key[2],"modes":{}}
        for mode,entries in modes.items():
            row["modes"][mode]={}
            for metric in ("tic_ms","frame_ms","frame_interval_ms"):
                if metric in entries[0]:
                    row["modes"][mode][metric]={q:statistics.median(e[metric][q] for e in entries) for q in ("median","p95","p99")}
                    row["modes"][mode][metric]["repeat_medians"]=[e[metric]["median"] for e in entries]
        if "out" in modes:
            base={e["repeat"]:e["tic_ms"]["median"] for e in modes["out"]}
            for mode in ("disabled","enabled"):
                pairs=[100*(e["tic_ms"]["median"]/base[e["repeat"]]-1) for e in modes.get(mode,[]) if e["repeat"] in base]
                if pairs:row[mode+"_overhead_percent"]={"median":statistics.median(pairs),"minimum":min(pairs),"maximum":max(pairs)}
        detailed=[Path(e["folder"])/"probes" for e in modes.get("enabled",[]) if (Path(e["folder"])/"probes"/"scopes.csv").exists()]
        scopes=collections.defaultdict(lambda:[0,0,0]);scopeSamples=collections.defaultdict(list);queryMaxima=collections.defaultdict(list);queryHist=collections.defaultdict(collections.Counter);searchSight=collections.defaultdict(lambda:[0,0,0]);counters=collections.defaultdict(list);pop=[];triggers=[]
        for folder in detailed:
            for r in read(folder/"scopes.csv"):
                v=scopes[r["category"]];v[0]+=int(r["calls"]);v[1]+=int(r["inclusive_ns"]);v[2]+=int(r["exclusive_ns"])
                scopeSamples[r['category']].append((int(r['calls']),int(r['exclusive_ns'])))
                if 'max_query_ns' in r:queryMaxima[r['category']].append(int(r['max_query_ns']))
            if (folder/'query_histogram.csv').exists():
                for r in read(folder/'query_histogram.csv'):
                    if r['thread']=='0':queryHist[r['category']][r['upper_inclusive_ns']]+=int(r['calls'])
            if (folder/'search_visibility.csv').exists():
                for r in read(folder/'search_visibility.csv'):
                    if r['thread']=='0':
                        values=searchSight[r['search']]
                        for i,key in enumerate(('calls','inclusive_ns','exclusive_ns')):values[i]+=int(r[key])
            for r in read(folder/"counters.csv"):counters[r["counter"]].append(int(r["value"]))
            local=read(folder/"population.csv");pop+=local
            times=[int(r['ns']) for r in read(folder.parent/'tics.csv')]
            expensive=stats(times)['p95']
            for kind in ('population_trigger','work_trigger'):
                flags=[int(r[kind]) for r in local];previous=0;on=off=0
                for value in flags:
                    on+=value>previous;off+=value<previous;previous=value
                high=[i for i,t in enumerate(times) if t>=expensive]
                activeTimes=[t for t,flag in zip(times,flags) if flag];inactiveTimes=[t for t,flag in zip(times,flags) if not flag]
                triggers.append({'kind':kind,'run':folder.parent.name,'on_transitions':on,'off_transitions':off,
                    'occupancy':statistics.mean(flags),'within_run_p95_ns':expensive,
                    'expensive_tic_recall':sum(flags[i] for i in high)/len(high),
                    'active_median_ns':statistics.median(activeTimes) if activeTimes else None,
                    'inactive_median_ns':statistics.median(inactiveTimes) if inactiveTimes else None})
        n=len(pop)
        if n:
            row["scopes_per_tic"]={k:{"calls":v[0]/n,"inclusive_ms":v[1]/n/1e6,"exclusive_ms":v[2]/n/1e6} for k,v in scopes.items()}
            row["counters_per_tic"]={k:{"mean":statistics.mean(v),"maximum":max(v)} for k,v in counters.items()}
            row['counter_distributions']={k:stats(v) for k,v in counters.items()}
            row['scope_distributions']={k:{'calls_per_tic':stats([c for c,t in values]),
                'exclusive_ms_per_tic':stats([t/1e6 for c,t in values]),
                'mean_query_ns_per_nonempty_tic':stats([t/c for c,t in values if c]) if any(c for c,t in values) else None}
                for k,values in scopeSamples.items()}
            row['trigger_association']=triggers
            row['main_query_histograms']={name:dict(bins) for name,bins in queryHist.items()}
            row['search_visibility_per_tic']={name:dict(calls=v[0]/n,inclusive_ms=v[1]/n/1e6,exclusive_ms=v[2]/n/1e6) for name,v in searchSight.items()}
            row['query_max_ns_per_tic']={name:stats(values) for name,values in queryMaxima.items()}
            row["population"]={k:{"minimum":min(int(r[k]) for r in pop),"maximum":max(int(r[k]) for r in pop),"mean":statistics.mean(int(r[k]) for r in pop)} for k in ("mobjs","eligible","targeted","moving","far","population_trigger","work_trigger","population_ns")}
        summary.append(row)
        row['final_rng_traces_equal']=len({(e['rng_calls'],e['rng_trace']) for entries in modes.values() for e in entries})==1
    (root/"analysis.json").write_text(json.dumps(summary,indent=2))
    for row in summary:
        print(row["preset"],row["map"],row["scenario"],{m:round(v["tic_ms"]["median"],4) for m,v in row["modes"].items()},
              "disabled overhead",round(row.get("disabled_overhead_percent",{}).get("median",0),2))
        if "scopes_per_tic" in row:
            print("  exclusive:",[(k,round(v["exclusive_ms"],3)) for k,v in sorted(row["scopes_per_tic"].items(),key=lambda pair:pair[1]["exclusive_ms"],reverse=True)[:5]])
    return summary

if __name__=="__main__":
    parser=argparse.ArgumentParser();parser.add_argument("output",type=Path);analyze(parser.parse_args().output)

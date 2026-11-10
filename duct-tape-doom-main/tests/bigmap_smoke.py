"""Run the actual engine against every map, without modifying the supplied WAD.
Usage: python bigmap_smoke.py --engine ... --iwad ... --wad ... --out ...
The caller supplies game data; nothing commercial is included in fixtures.
"""
import argparse
import collections
import json
import re
from pathlib import Path
import struct
import subprocess
import time


def directory(path):
    data = path.read_bytes()
    if data[:4] not in (b"IWAD", b"PWAD"):
        raise ValueError("Not a WAD")
    count, offset = struct.unpack_from("<ii", data, 4)
    if count < 0 or offset < 12 or offset + count * 16 > len(data):
        raise ValueError("Invalid WAD directory")
    result = []
    for i in range(count):
        pos, size, name = struct.unpack_from("<ii8s", data, offset + i * 16)
        if pos < 0 or size < 0 or pos + size > len(data):
            raise ValueError("Invalid lump range")
        result.append((name.rstrip(b"\0").decode("ascii"), data[pos:pos + size]))
    return result


def main():
    ap = argparse.ArgumentParser()
    for key in ("engine", "iwad", "wad", "out"):
        ap.add_argument("--" + key, type=Path, required=True)
    ap.add_argument("--maps", default="")
    ap.add_argument("--save-load", action="store_true")
    args = ap.parse_args()
    args.engine, args.iwad, args.wad, args.out = (
        p.resolve() for p in (args.engine, args.iwad, args.wad, args.out))
    args.out.mkdir(parents=True, exist_ok=True)
    lumps = directory(args.wad)
    maps = []
    selected = args.maps.split(",") if args.maps else None
    for i, (name, _) in enumerate(lumps):
        if len(name) != 5 or not name.startswith("MAP") or not name[3:].isdigit():
            continue
        if selected and name not in selected:
            continue
        parts = dict(lumps[i + 1:i + 11])
        line_counts = collections.Counter(
            x[3] for x in struct.iter_unpack("<7H", parts["LINEDEFS"]))
        sector_counts = collections.Counter(
            struct.unpack_from("<H", parts["SECTORS"], at + 22)[0]
            for at in range(0, len(parts["SECTORS"]), 26))
        work = args.out / name
        work.mkdir(exist_ok=True)
        command = [str(args.engine), "-iwad", str(args.iwad), "-file", str(args.wad),
                   "-warp", str(int(name[3:])), "-skill", "1", "-heapsize", "512",
                   "-checkbigmap", "-checkrender", "-config", str(work / "doom.cfg")]
        start = time.monotonic()
        try:
            run = subprocess.run(command, cwd=work, capture_output=True, timeout=90)
            output = (run.stdout + run.stderr).decode(errors="replace")
            passed = run.returncode == 0 and "PASS bigmap:" in output and "Error:" not in output
            code = run.returncode
        except subprocess.TimeoutExpired as error:
            output = ((error.stdout or b"") + (error.stderr or b"")).decode(errors="replace")
            passed, code = False, "timeout"
        (work / "engine.log").write_text(output, encoding="utf-8")
        maps.append(dict(map=name, passed=passed, exit=code,
                         seconds=round(time.monotonic() - start, 3),
                         zone_bytes=(re.search(r"Zone bytes: used=(\d+) cache=(\d+) free=(\d+)", output).groups()
                                     if "Zone bytes:" in output else None),
                         nodes=parts["NODES"][:4].hex(),
                         line_specials=dict(sorted(line_counts.items())),
                         sector_specials=dict(sorted(sector_counts.items()))))
        print(name, "PASS" if passed else "FAIL", maps[-1]["seconds"], flush=True)
    if args.save_load:
        for record in maps:
            if not record["passed"] or record["map"] not in ("MAP03", "MAP21"):
                continue
            work = args.out / record["map"]
            base_cmd = [str(args.engine), "-iwad", str(args.iwad), "-file", str(args.wad),
                        "-heapsize", "512", "-checkbigmap", "-config", str(work / "doom.cfg")]
            for mode in ("save", "load"):
                options = (["-warp", str(int(record["map"][3:])), "-checksave"] if mode == "save" else
                           ["-loadgame", "0", "-checkloaded"])
                run = subprocess.run(base_cmd + options, cwd=work, capture_output=True, timeout=90)
                output = (run.stdout + run.stderr).decode(errors="replace")
                (work / (mode + ".log")).write_text(output, encoding="utf-8")
                ok = run.returncode == 0 and "PASS bigmap:" in output
                record[mode] = ok
                record["passed"] = record["passed"] and ok
                print(record["map"], mode, "PASS" if ok else "FAIL", flush=True)
    (args.out / "results.json").write_text(json.dumps(maps, indent=2), encoding="utf-8")
    if not maps or not all(m["passed"] for m in maps):
        raise SystemExit(1)
    print(f"PASS {len(maps)} maps: texture comparison, classic frame, 35 simulation tics")


if __name__ == "__main__":
    main()

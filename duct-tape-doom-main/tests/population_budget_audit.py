"""Focused original-population audit; deliberately NOT an FPS benchmark.

Usage: python population_budget_audit.py <doom-encounter-tests.exe> <IWAD> <PWAD> <output-dir>
Runs the actual native map-spawn path for six budgets under both safety modes.
"""
import csv
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys


def main():
    executable, iwad, pwad, output = (Path(p).resolve() for p in sys.argv[1:])
    output.mkdir(parents=True, exist_ok=True)
    rows = []
    for policy in (0, 1):
        for mode, value in [(0, 500), (0, 1000), (0, 2000), (1, 12.5), (1, 25), (1, 50)]:
            env = dict(os.environ, HELLFORGE_POPULATION_AUDIT="1",
                       HELLFORGE_POPULATION_POLICY=str(policy), HELLFORGE_INITIAL_MODE=str(mode),
                       HELLFORGE_INITIAL_COUNT=str(value if mode == 0 else 500),
                       HELLFORGE_INITIAL_PERCENT=str(value if mode == 1 else 25))
            label = f"{'performance' if policy else 'compatibility'}-{'percent' if mode else 'count'}-{value}"
            result = subprocess.run([str(executable), str(iwad), str(pwad)], cwd=output, env=env,
                                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=30)
            text = result.stdout.decode("utf-8", errors="replace")
            (output / f"{label}.log").write_text(text, encoding="utf-8")
            if result.returncode:
                raise RuntimeError(f"{label} exited {result.returncode}; inspect its log")
            initial = re.search(r"^POPULATION initial (.+)$", text, re.M)
            facing = re.search(r"^POPULATION post-facing (.+)$", text, re.M)
            if not initial or not facing:
                raise RuntimeError(f"{label}: native population audit output missing")
            fields = dict(re.findall(r"(\w+)=([\d.]+)", initial[1]))
            after = dict(re.findall(r"(\w+)=([\d.]+)", facing[1]))
            row = {"policy": policy, "budget_mode": mode, "input": value, **fields,
                   **{f"post_{k}": v for k, v in after.items()}}
            live, target, hard, total = (int(fields[k]) for k in ("live", "requested", "required_eager", "filtered"))
            assert int(fields["native_live"]) == live, label
            assert live == (max(target, hard) if policy else total), label
            assert live + int(fields["deferred"]) == total, label
            assert int(after["live"]) == live and int(after["deferred"]) == int(fields["deferred"]), label
            assert "WAD facing has no spawn/RNG side effects" in text, label
            rows.append(row)
    with (output / "population.csv").open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    hashes = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in (executable, iwad, pwad)}
    (output / "manifest.json").write_text(json.dumps({"measurement": "population audit; no FPS/GPU benchmark",
                                                    "skill": "medium", "map": "MAP01", "seed": 77,
                                                    "native_options": ["-solidmaps", "-nomusic", "-nosound"],
                                                    "sha256": hashes}, indent=2), encoding="utf-8")
    print("PASS 12 native budget/policy audits; population.csv and raw logs written (no FPS claim)")


if __name__ == "__main__":
    main()

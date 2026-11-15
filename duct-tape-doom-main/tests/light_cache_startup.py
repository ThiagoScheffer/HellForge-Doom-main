#!/usr/bin/env python3
"""Focused DOOM2 MAP01 first-frame/normal-quit regression, cold and warm cache.

Uses doom-reload-smoke's startup mode; never edits the input profile.
"""
import argparse
import os
from pathlib import Path
import shutil
import subprocess


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--iwad', type=Path, required=True)
    parser.add_argument('--graphics', type=Path, required=True)
    parser.add_argument('--engine', type=Path, required=True, help='doom-reload-smoke.exe')
    parser.add_argument('--out', type=Path, required=True, help='fresh isolated output directory')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    out = args.out.resolve()
    if out.exists():
        raise SystemExit('Choose a fresh output directory; existing evidence is preserved.')
    out.mkdir(parents=True)
    for name in ('cold', 'warm'):
        case = out / name
        case.mkdir()
        config = case / 'graphics.cfg'
        shutil.copy2(args.graphics.resolve(), config)
        env = os.environ.copy()
        env.update(TUIN_CRASH_NO_DIALOG='1', HELLFORGE_CRASH_DIR=str(case),
                   HELLFORGE_MAP_CACHE=str(out / 'Cache' / 'Maps'))
        command = [str(args.engine.resolve()), str(args.iwad.resolve()), str(config),
                   'startup', '1', '-solidmaps', '-uzmovement', '-fluidmonsters',
                   '-file', str(root / 'launcher' / 'DiggingMaterials.wad'),
                   '-fluidweapons', str(root / 'launcher' / 'mods' / 'FluidWeapons.pk3'),
                   '-weaponinterpolation']
        with (case / 'engine.log').open('wb') as log:
            try:
                result = subprocess.run(command, cwd=case, env=env, stdout=log,
                                        stderr=subprocess.STDOUT, timeout=90)
            except subprocess.TimeoutExpired:
                raise SystemExit(f'FAIL {name}: timed out; see {case / "engine.log"}') from None
        text = (case / 'engine.log').read_text(encoding='utf-8', errors='replace')
        if (result.returncode or 'STARTUP PASS 4 frames; normal quit' not in text
                or 'first frame submitted for level 1' not in text):
            raise SystemExit(f'FAIL {name}: exit {result.returncode}; see {case / "engine.log"}')
        print(f'PASS {name}: initial frame, four frame callbacks and normal quit', flush=True)


if __name__ == '__main__':
    main()

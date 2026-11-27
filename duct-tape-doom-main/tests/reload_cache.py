#!/usr/bin/env python3
"""Real Vulkan reload/cache tests using the supplied original Doom IWAD.
Profiles, saves, screenshots, dumps and caches stay under the output directory.
"""
import ctypes
from ctypes import wintypes
import argparse
import json
import os
import re
import subprocess
import time
from pathlib import Path


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--iwad', required=True, type=Path)
    parser.add_argument('--engine', required=True, type=Path)
    parser.add_argument('--out', required=True, type=Path)
    parser.add_argument('--cycles', type=int, default=10)
    args = parser.parse_args()
    iwad, engine, out = args.iwad.resolve(), args.engine.resolve(), args.out.resolve()
    assert iwad.name.upper() in {'DOOM.WAD', 'DOOM1.WAD', 'DOOM2.WAD'}, 'Use an original Doom IWAD'
    episodic = iwad.name.upper() != 'DOOM2.WAD'
    out.mkdir(parents=True, exist_ok=True)
    cache, crashes = out / 'Cache' / 'Maps', out / 'crashes'
    crashes.mkdir(exist_ok=True)
    results = []
    base = '''fullscreen 0
scale 100
baked_lights 1
emissive 1
bake_only_lights 1
grid_sprite_light 1
bounce_light 1
sky_light 1
baked_occlusion 1
sun_shadows 1
window_light 1
decoration_shadows 1
light_flow 1
moving_relight 1
weather 0
outdoor_wind 0
fps 0
'''

    def run(name, mode, cycles, enabled=True, extra=''):
        folder = out / name
        folder.mkdir(exist_ok=True)
        config = folder / 'graphics.cfg'
        config.write_text(base + f'map_light_cache {int(enabled)}\n' + extra)
        env = os.environ.copy()
        env.update(TUIN_MAP_CACHE=str(cache), TUIN_CRASH_DIR=str(crashes),
                   TUIN_CRASH_NO_DIALOG='1', SDL_GPU_DRIVER='vulkan')
        command = [str(engine), str(iwad), str(config), mode, str(cycles)]
        if episodic:
            command += ['-nomonsters']
        if mode == 'save':
            command += ['-holdrelight']
        if not enabled:
            command += ['-nobakecache']
        samples = []
        class Memory(ctypes.Structure):
            _fields_ = [('cb', wintypes.DWORD), ('faults', wintypes.DWORD)] + [(key, ctypes.c_size_t) for key in
                ['peak_working', 'working', 'peak_paged', 'paged', 'peak_nonpaged', 'nonpaged', 'commit', 'peak_commit', 'private']]
        memory_call = None
        if os.name == 'nt':
            memory_call = ctypes.WinDLL('kernel32').K32GetProcessMemoryInfo
            memory_call.argtypes = [wintypes.HANDLE, ctypes.POINTER(Memory), wintypes.DWORD]
        start = time.monotonic()
        with (folder / 'engine.log').open('wb') as log:
            process = subprocess.Popen(command, cwd=folder, env=env,
                                       stdout=log, stderr=subprocess.STDOUT)
            try:
                while process.poll() is None:
                    if time.monotonic() - start > 180:
                        raise subprocess.TimeoutExpired(command, 180)
                    if memory_call:
                        memory = Memory()
                        memory.cb = ctypes.sizeof(memory)
                        if memory_call(int(process._handle), ctypes.byref(memory), memory.cb):
                            samples.append([round(time.monotonic() - start, 2), memory.working, memory.private])
                    time.sleep(0.1)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
                raise RuntimeError(f'{name} timed out')
        (folder / 'memory.json').write_text(json.dumps(samples))
        text = (folder / 'engine.log').read_text(errors='replace')
        expected = ('CACHE STORAGE PASS' if mode == 'storage' else
                    'LIGHTING MODE TRANSITIONS PASS' if mode == 'lighting-models' else
                    'STARTUP PASS 4 frames' if mode == 'startup' else
                    'intentional diagnostic regression' if mode == 'crash' else
                    f'RELOAD PASS {mode} {cycles} cycles')
        assert expected in text, f'{name}: exit {process.returncode}; see {folder}/engine.log\n{text[-2000:]}'
        if mode != 'crash':
            assert process.returncode == 0, f'{name}: exit {process.returncode}'
        else:
            assert process.returncode != 0 and list(crashes.glob('*.dmp')), 'No crash dump'
        row = dict(name=name, exit=process.returncode,
                   seconds=round(time.monotonic() - start, 3),
                   hits=text.count(' hit; checksum '),
                   misses=text.count('miss (no matching entry)'),
                   rejected=text.count('rejected ('),
                   frames=text.count('first frame submitted'),
                   peak_private=max((s[2] for s in samples), default=0),
                   final_private=samples[-1][2] if samples else 0,
                   loads=re.findall(r'3D loading: ([^\n]+)', text))
        if mode in ('restart', 'save'):
            assert row['frames'] >= cycles + 1, f'{name}: missing post-reload frame submissions'
        if mode == 'save':
            assert text.count('RELOAD ACTIVE RELIGHT before save load') == cycles
        results.append(row)
        (out / 'results.json').write_text(json.dumps(results, indent=2))
        print(name, 'PASS', f"hits={row['hits']} misses={row['misses']} rejected={row['rejected']}", flush=True)
        return text

    run('uncached-restart', 'restart', args.cycles, False)
    run('uncached-save', 'save', args.cycles, False)
    assert not cache.exists(), 'Disabled cache wrote files'
    cold = run('cold-restart', 'restart', args.cycles)
    assert 'stored; checksum' in cold
    if not episodic:
        assert ' hit; checksum ' in cold
    run('cold-save', 'save', args.cycles)
    warm = run('warm-restart', 'restart', args.cycles)
    run('warm-save', 'save', args.cycles)
    if episodic:
        # Animated Doom textures can be on different frames after an
        # in-process restart. Compare two matching startup snapshots.
        cold = run('cold-startup', 'startup', 1, extra='lighting_model 1\n')
        warm = run('warm-startup', 'startup', 1, extra='lighting_model 1\n')
    assert warm.count(' hit; checksum ') >= 6, 'No cross-process disk reuse'
    computed = set(re.findall(r'3D cache: (\w+) stored; checksum (\w+)', cold))
    restored = set(re.findall(r'3D cache: (\w+) hit; checksum (\w+)', warm))
    assert {stage for stage, _ in restored} == {'sun', 'lights_hf2', 'grid_hf2', 'bounce', 'ambient', 'flow'}
    assert restored <= computed, 'Restored output differs from cold output'
    entries = list(cache.rglob('lights_hf2-*.bake'))
    assert entries
    for entry in entries:
        entry.write_bytes(entry.read_bytes()[:20])
    corrupt = run('corrupt-recovery', 'startup' if episodic else 'restart', 1,
                  extra='lighting_model 1\n' if episodic else '')
    assert 'rejected (' in corrupt and 'stored; checksum' in corrupt
    changed = run('settings-change', 'restart', 1, True, 'window_light 0\n')
    assert 'miss (no matching entry)' in changed
    run('storage-contract', 'storage', 1)
    run('illumination-transitions', 'lighting-models', 1)
    run('crash-diagnostics', 'crash', 1, False)
    print('All reload/cache regressions passed:', out, flush=True)


if __name__ == '__main__':
    main()

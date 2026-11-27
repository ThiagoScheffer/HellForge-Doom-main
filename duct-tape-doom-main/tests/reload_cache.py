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
import statistics
import tempfile
import time
from pathlib import Path


def parse_sessions(text):
    sessions = []
    current = None
    for line in text.splitlines():
        if line.startswith('[LIGHT CACHE] event=map_prepare '):
            current = dict(stages=[], **dict(re.findall(r'(\w+)=([^" ]+|"[^"]*")', line)))
        elif current is not None and line.startswith('[LIGHT CACHE] stage='):
            current['stages'].append(dict(re.findall(r'(\w+)=([^ ]+)', line)))
        elif current is not None and line.startswith('[LIGHT CACHE] event=summary '):
            for key, value in re.findall(r'(\w+)=([\d.]+)', line):
                current[key] = float(value) if '.' in value else int(value)
            sessions.append(current)
            current = None
    return sessions


def require_warm(sessions):
    assert sessions, 'No structured map-preparation reports'
    for session in sessions:
        assert session['eligible'] > 0, session
        assert session['restored'] == session['eligible'], session
        assert session['calculated'] == session['compute_dispatches'] == 0, session
        assert all(stage['result'] == 'HIT' for stage in session['stages']), session
        assert all(stage['uploaded'] == '1' for stage in session['stages']
                   if stage['stage'] != 'grid_hf2'), session


def preparation_stats(sessions):
    if not sessions:
        return {}
    return {key: dict(min=min(values), median=statistics.median(values), max=max(values),
                      p95=statistics.quantiles(values, n=20, method='inclusive')[18]
                      if len(values) >= 20 else None)
            for key in ('total_ms', 'key_ms', 'lookup_ms', 'read_ms',
                        'calculate_ms', 'write_ms', 'upload_ms', 'geometry_ms', 'reconcile_ms')
            if all(key in session for session in sessions)
            for values in [[session[key] for session in sessions]]}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--iwad', required=True, type=Path)
    parser.add_argument('--engine', required=True, type=Path)
    parser.add_argument('--out', required=True, type=Path)
    parser.add_argument('--cycles', type=int, default=10)
    parser.add_argument('--samples', type=int, default=0,
                        help='Independent cold and fresh-process warm startup samples')
    args = parser.parse_args()
    iwad, engine, out = args.iwad.resolve(), args.engine.resolve(), args.out.resolve()
    assert iwad.name.upper() in {'DOOM.WAD', 'DOOM1.WAD', 'DOOM2.WAD'}, 'Use an original Doom IWAD'
    episodic = iwad.name.upper() != 'DOOM2.WAD'
    out.mkdir(parents=True, exist_ok=True)
    if (out / 'Cache').exists() or (out / 'results.json').exists():
        out = Path(tempfile.mkdtemp(prefix='run-', dir=out))
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

    def run(name, mode, cycles, enabled=True, extra='', engine_flags=(), cache_root=None):
        folder = out / name
        folder.mkdir(exist_ok=True)
        config = folder / 'graphics.cfg'
        config.write_text(base + f'map_light_cache {int(enabled)}\n' + extra)
        env = os.environ.copy()
        env.update(HELLFORGE_MAP_CACHE=str(cache if cache_root is None else cache_root), TUIN_CRASH_DIR=str(crashes),
                   TUIN_CRASH_NO_DIALOG='1', SDL_GPU_DRIVER='vulkan')
        command = [str(engine), str(iwad), str(config), mode, str(cycles), *engine_flags]
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
                    'CACHE REVISIT PASS' if mode == 'revisit' else
                    'intentional diagnostic regression' if mode == 'crash' else
                    f'RELOAD PASS {mode} {cycles} cycles')
        assert expected in text, f'{name}: exit {process.returncode}; see {folder}/engine.log\n{text[-2000:]}'
        if mode != 'crash':
            assert process.returncode == 0, f'{name}: exit {process.returncode}'
        else:
            assert process.returncode != 0 and list(crashes.glob('*.dmp')), 'No crash dump'
        sessions = parse_sessions(text)
        row = dict(name=name, exit=process.returncode,
                   seconds=round(time.monotonic() - start, 3),
                   hits=sum(item['restored'] for item in sessions),
                   misses=sum(stage['result'] == 'MISS_NO_FILE' for item in sessions for stage in item['stages']),
                   rejected=sum(stage['result'].startswith('REJECTED_') for item in sessions for stage in item['stages']),
                   sessions=sessions, preparation=preparation_stats(sessions),
                   frames=text.count('first frame submitted'),
                   peak_private=max((s[2] for s in samples), default=0),
                   final_private=samples[-1][2] if samples else 0,
                   loads=re.findall(r'3D loading: ([^\n]+)', text))
        if mode in ('restart', 'save', 'startup'):
            assert sessions, f'{name}: missing structured cache reports'
        if mode in ('restart', 'save'):
            assert row['frames'] >= cycles + 1, f'{name}: missing post-reload frame submissions'
        if mode == 'save':
            assert text.count('RELOAD ACTIVE RELIGHT before save load') == cycles
        results.append(row)
        (out / 'results.json').write_text(json.dumps(results, indent=2))
        print(name, 'PASS', f"hits={row['hits']} misses={row['misses']} rejected={row['rejected']}", flush=True)
        return text, sessions

    _, disabled = run('uncached-restart', 'restart', args.cycles, False)
    assert not cache.exists(), 'Disabled cache wrote files'
    assert all(item['restored'] == 0 and item['calculated'] == item['eligible']
               and all(stage['result'] == 'CACHE_DISABLED' for stage in item['stages'])
               for item in disabled)

    _, cold = run('cold-restart', 'restart', args.cycles)
    assert cold[0]['calculated'] == cold[0]['eligible'] > 0
    assert cold[0]['compute_dispatches'] > 0 and cold[0]['write_failures'] == 0
    require_warm(cold[1:])
    _, warm = run('warm-restart', 'restart', args.cycles)
    require_warm(warm)
    _, fresh = run('fresh-process-warm', 'startup', 1)
    require_warm(fresh)
    computed = {(stage['stage'], stage['checksum']) for stage in cold[0]['stages']}
    restored = {(stage['stage'], stage['checksum']) for stage in fresh[0]['stages']}
    assert computed == restored, 'Restored output differs from cold output'
    assert {stage for stage, _ in restored} == {'sun', 'lights_hf2', 'grid_hf2', 'bounce', 'ambient', 'flow'}

    if not episodic:
        run('uncached-save', 'save', args.cycles, False)
        run('cold-save', 'save', args.cycles)
        _, loaded = run('warm-save', 'save', args.cycles)
        require_warm(loaded)
        text, modified = run('modified-world-save', 'save', 2,
                             engine_flags=('-cache-saved-world',),
                             cache_root=out / 'modified-world-cache')
        assert text.count('CACHE SAVED WORLD PASS') == 2
        assert modified[1]['calculated'] > 0, modified[1]
        first_keys = {stage['stage']: stage['key'] for stage in modified[0]['stages']}
        assert all(first_keys[stage['stage']] != stage['key'] for stage in modified[1]['stages'])
        require_warm(modified[2:])
        _, revisit = run('revisit', 'revisit', 1)
        assert [item['map'] for item in revisit] == ['MAP01', 'MAP02', 'MAP01']
        require_warm(revisit[-1:])

    verification, checks = run('verify-numeric', 'startup', 1, engine_flags=('-verifybakecache',))
    assert checks[0]['verified'] == checks[0]['eligible'] == 6
    assert verification.count(' result=MATCH') == 6 and ' result=MISMATCH' not in verification
    for name, model in (('hybrid', 1), ('physical', 2)):
        config = f'lighting_model {model}\n'
        run(f'{name}-cold', 'startup', 1, extra=config)
        text, checks = run(f'{name}-verify', 'startup', 1, extra=config, engine_flags=('-verifybakecache',))
        assert checks[0]['verified'] == checks[0]['eligible'] == 6
        assert text.count(' result=MATCH') == 6 and ' result=MISMATCH' not in text

    for i in range(args.samples):
        _, sample = run(f'sample-cold-{i+1}', 'startup', 1, cache_root=out / f'ColdCache-{i+1}')
        assert sample[0]['calculated'] == sample[0]['eligible'] == 6
        _, sample = run(f'sample-warm-{i+1}', 'startup', 1)
        require_warm(sample)

    key = next(stage['key'] for stage in fresh[0]['stages'] if stage['stage'] == 'lights_hf2')
    entry, = cache.rglob(f'lights_hf2-{key}.bake')
    entry.write_bytes(entry.read_bytes()[:20])
    _, recovered = run('corrupt-recovery', 'startup', 1)
    assert recovered[0]['restored'] == 5 and recovered[0]['calculated'] == 1
    assert any(stage['result'] == 'REJECTED_INVALID_HEADER' and stage['write'] == 'STORED'
               for stage in recovered[0]['stages'])
    _, changed = run('settings-change', 'startup', 1, extra='window_light 0\n')
    by_stage = {stage['stage']: stage for stage in changed[0]['stages']}
    assert by_stage['sun']['result'] == by_stage['ambient']['result'] == 'HIT'
    assert by_stage['flow']['result'] == 'MISS_NO_FILE'
    run('storage-contract', 'storage', 1)
    run('illumination-transitions', 'lighting-models', 1)

    blocker = out / 'unwritable-cache-root'
    blocker.write_bytes(b'file, not a directory')
    _, failed = run('write-failure', 'startup', 1, cache_root=blocker)
    assert failed[0]['write_failures'] == failed[0]['calculated'] == 6
    assert failed[0]['restored'] == 0 and blocker.read_bytes() == b'file, not a directory'

    run('crash-diagnostics', 'crash', 1, False)
    print('All reload/cache regressions passed:', out, flush=True)


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""Isolated launcher -> real F4 apply -> process restart persistence regression.
No installed profile is read or changed. Requires critical-smoke and Windows csc.
"""
import argparse
import os
from pathlib import Path
import subprocess
import uuid


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--iwad', required=True, type=Path)
    parser.add_argument('--arena', required=True, type=Path)
    parser.add_argument('--engine', type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    output = root / 'build' / ('settings-persistence-' + uuid.uuid4().hex[:10])
    output.mkdir(parents=True)
    helper = output / 'LauncherPersistenceTests.exe'
    compiler = Path(os.environ['WINDIR']) / 'Microsoft.NET/Framework64/v4.0.30319/csc.exe'
    engine = (args.engine or root / 'build/Release/doom-critical-smoke.exe').resolve()
    iwad, arena = args.iwad.resolve(), args.arena.resolve()
    references = ['System.Windows.Forms.dll', 'System.Drawing.dll', 'System.Xml.Linq.dll',
                  'System.IO.Compression.dll', 'System.IO.Compression.FileSystem.dll']
    command = [str(compiler), '/nologo', '/codepage:65001', '/target:exe', '/platform:x64',
               '/main:LauncherPersistenceTests', '/out:' + str(helper)]
    command += ['/r:' + ref for ref in references]
    command += [str(root / 'launcher' / name) for name in
                ['Launcher.cs', 'LanMenu.cs', 'LauncherPersistenceTests.cs']]
    subprocess.run(command, check=True, cwd=output)
    subprocess.run([str(helper), str(output / 'launcher checks')], check=True, cwd=output)
    runs = 0

    def run(directory, mode):
        nonlocal runs
        result = subprocess.run([str(engine), str(iwad), str(arena), str(directory), mode],
                                cwd=directory, text=True, capture_output=True, timeout=60)
        log = directory / (mode.lstrip('-') + '.log')
        log.write_text(result.stdout + result.stderr, encoding='utf-8')
        lines = [line for line in result.stdout.splitlines() if 'settings persistence checks' in line]
        if result.returncode != 0 or not lines or 'Error:' in result.stdout:
            raise RuntimeError(f'{mode} failed; see {log}\n' + (result.stdout + result.stderr)[-3000:])
        print(lines[-1])
        runs += 1

    def prepare(profile, low):
        result = subprocess.run([str(helper), '--prepare', str(profile),
                                 str(root / 'launcher/graphics.cfg'), str(low)],
                                cwd=output, text=True, capture_output=True, check=True)
        assert result.stdout.strip() == str(profile), 'Launcher must pass the persistent path'

    for low, mode in [(False, '--settings-write'), (True, '--settings-write-f4'),
                      (True, '--settings-write-button')]:
        directory = output / (('low FX' if low else 'normal') + mode)
        directory.mkdir()
        profile = directory / 'graphics.cfg'
        prepare(profile, low)
        run(directory, mode)
        applied = profile.read_bytes()
        prepare(profile, low)
        assert profile.read_bytes() == applied, 'Launch must preserve applied choices'
        run(directory, '--settings-read')
        for action in ['--settings-discard', '--settings-unapplied']:
            run(directory, action)
            assert profile.read_bytes() == applied, 'Discard/unapplied draft was written'
            run(directory, '--settings-read')
        run(directory, '--settings-save-errors')
        assert profile.read_bytes() == applied
        assert not list(directory.glob('*.saving-*.tmp')), 'Failed saves left temporary files'

    directory = output / 'BOM comments and malformed lines'
    directory.mkdir()
    text = '''# Header before any options
; Another comment
accelerated 0
critical_parts 0
broken_line
critical_parts banana
critical_parts 1 # inline comment
unknown_future_key 123
monster_feeding 1
enhanced_weak_points 1
monster_weapon_vulnerabilities 1
localized_injury_effects 1
monster_bleeding 1
headshot_frenzy 1
monster_metal_sparks 1
headshot_feedback 3
headshot_feedback 99
headshot_sound 0
fov 95
fov nan
fov 100 trailing_garbage
scale 100
emissive 1
fog 1
corner_peek 1
peek_left 29
wp_bleed_damage_3 7
wp_bleed_damage_3 900
'''
    (directory / 'graphics.cfg').write_bytes(b'\xef\xbb\xbf' + text.encode())
    run(directory, '--settings-parse')
    legacy = output / 'legacy combined options'
    legacy.mkdir()
    (legacy / 'graphics.cfg').write_text(
        '# Legacy options\naccelerated 0\nblood 0\n# gap\nblood_spray 0\nblood_shine 0\n'
        'death_motion 1\nhit_flinch 0\nmalformed\ncritical_parts 1\n', encoding='utf-8')
    run(legacy, '--settings-legacy')
    summary = f'PASS {runs} native process runs plus 18 launcher persistence/backup checks; output: {output}'
    print(summary)
    (output / 'RESULT.txt').write_text(summary + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()

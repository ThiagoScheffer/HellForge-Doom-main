# F4 settings persistence fix

```yaml
project: DOOM: HELLFORGE
build: 2026.11.12.1
updated_utc: 2026-11-11T17:52:02Z
status: active
tests: ../../build/reload-launcher-persistence.log
index: ../README.md
```

Historical fix first validated in build **2026.11.10.1**, 10 November 2026. The current fork artifact is [DOOM-Hellforge.exe](../../dist/DOOM-Hellforge.exe); the original historical binary is not redistributed.

## What was wrong

The launcher created a temporary Low-FX graphics config, passed it to the engine and deleted it when the process exited. F4 correctly wrote to that supplied path, but those saved choices were then lost. The next launch copied the unchanged persistent profile again.

Separately, the engine read options with a token-stream `fscanf` loop that stopped at the first comment or malformed value. The temporary Low-FX config always began with a comment, so none of its settings loaded. Comments in normal/imported files also prevented later options from loading.

Both failures were reproduced during diagnosis. The existing user profile was inspected read-only; testing and fixes did not rewrite it.

## Resulting behavior

- Every game launch uses the existing persistent profile's `graphics.cfg`, including Low-FX launches. The temporary session config/deletion path is removed.
- Low-FX is a starting preset only for newly created profiles. Existing profiles are not overwritten, and F4 choices explicitly re-enabling effects remain enabled on the next launch. The label/tooltip explain this behavior.
- The Low-FX starting resolution is now 75%, a supported F4 choice. The former 85% value was outside the native option table. Existing profiles are left unchanged.
- The reader accepts a UTF-8 BOM, blank lines and comments, and skips individual malformed lines. Unknown keys and invalid option values retain the existing validation behavior; valid options later in the file still load. Legacy combined-option conversion remains supported.
- **Esc, F4 and Apply/close save the draft. F10 discards. Quitting with an unapplied F4 draft does not save it**, as requested.
- Applied settings write to a uniquely named file beside the profile, are flushed/closed, then replace the destination through SDL's rename API. Failed writes/replacements preserve the old file and show a nonfatal error naming the destination. No automatic reset or profile migration is performed.

## Verification

| Check | Result |
|---|---|
| Full configured Release build | Passed |
| Registered CTest suite | **29/29 passed** |
| Launcher persistence/export/import assertions | **18 passed** |
| Native persistence fixture | **23 independent process runs passed** |
| Existing combat integration, ordinary collision | **4,697 assertions passed** |
| Existing combat integration, solid-map collision | **4,697 assertions passed** |

The native persistence fixture applies Combat checkboxes, graphics checkboxes overridden by the former Low-FX mode, a choice, a slider, a key binding and numeric balance tuning. It launches a second process using the same prepared profile and verifies the values. It covers all three Apply/close routes, normal and Low-FX initialization, custom paths with spaces, F10 discard, quitting with an unapplied draft, parser BOM/comments/malformed lines, unknown/invalid keys and legacy keys.

A Windows file handle blocks replacement to verify the real failed-rename path; another case uses an absent parent directory. Both return nonfatal failures, leave the original file unchanged, and leave no save temporary files. The launcher tests also export/import the actual persistent graphics file and verify that preparing a Low-FX launch does not overwrite the restored values.

These fixtures use isolated build directories and the actual native `sceneInit`/F4 control handlers. The dummy video driver has no GPU device, so the fixture explicitly invokes the same scene-initialization entry point before driving F4 events. It is a native process/UI-handler test, not a claim of manual mouse testing. No browser or Node tests were used; fixture processes exited normally. The existing launcher LanMenu CS4014 warning does not prevent successful packaging.

Logs: [full build](../../build/persistence-full-build.log), [process/launcher checks](../../build/persistence-process-tests.log), [CTest](../../build/persistence-regression.log), [ordinary combat](../../build/critical-smoke/persistence-combat.log), [solid combat](../../build/critical-smoke-solid/persistence-combat.log).

## Changed files

- `launcher/Launcher.cs`: persistent path, new-profile-only preset, explanatory text.
- `launcher/LauncherPersistenceTests.cs`: isolated launcher and backup regressions.
- `launcher/VERSION`: 2026.11.10.1.
- `duct-tape-doom-main/platform/scene3d/config.cpp`: line reader, safe replacement and save-failure reporting.
- `duct-tape-doom-main/platform/settings_menu.cpp`: test-only F4 persistence driver; production apply/discard behavior remains intact.
- `duct-tape-doom-main/tests/critical_smoke.cpp`: persistence fixture modes that retain the supplied config.
- `duct-tape-doom-main/tests/settings_persistence.inc`: native restart/read/write/discard/failure assertions.
- `duct-tape-doom-main/tests/settings_persistence.py`: reproducible launcher/native process orchestration.
- `README.md` and this report: usage and verification notes.

## Reproduction

After building the configured Release targets:

```text
python duct-tape-doom-main/tests/settings_persistence.py --iwad <DOOM2.WAD> --arena <critical-arena.wad>
ctest --test-dir build -C Release --output-on-failure
```

The script creates a uniquely named directory under `build/`, compiles the C# test helper with the installed Windows .NET Framework compiler, and keeps all configs, saves and logs there. It does not use the installed player profile.

---
build: 2026.11.12.1-experimental
updated_utc: 2026-11-13T17:33:11Z
status: experimental-targeted-regressions-passed
index: ../README.md
tests: ../../duct-tape-doom-main/tests/sight_sectors.cpp
---

# SF2012 sight crash and experimental destruction launch fix

Changes remain in `experiment/monster-profiling-phase2`, based on
`b59ad46c1d5dcc92874074b3a2805a9b5af4b521`. Existing experimental changes are
preserved; main and release artifacts are untouched.

## Crash evidence and fix

The supplied log identifies engine SHA-256
`C75E83B63EC81A993D70F4FE382A004CC6DCAFDC9F847985FD733CDC672EE983`.
Its matching executable/PDB were preserved before rebuilding. The local dump
records a null read at engine RVA `0x42dfad`, inside `P_CrossSubsector`.
The caller chain is `P_CheckSight` → `A_Look` → thinker processing. This is a
simulation sight failure, not a light-bake or sprite-filter failure.

The faulting instruction reads `back->floorheight` with a null back sector.
SF2012 has lines marked `ML_TWOSIDED` with a missing sidedef: MAP02 lines 427
and 557, MAP15 line 592, and MAP21 line 8411 (zero-based indices).
The final logged geometry, 2,175 subsectors / 3,374 lines, matches MAP02.
Reload serial numbers in the log are not map numbers; MAP01 was also checked.

`p_sight.c` now treats a crossed seg lacking either sector as a closed wall,
instead of dereferencing it. Valid two-sided openings retain their existing
height/slope tests. No map data, actor layout or save format is changed.

## Destruction launch regression

The experimental launcher previously forced `-nodigging`. That removed the
destruction backend even when the player's F4 options enabled wall damage.
Normal experimental launches now use `-solidmaps`, matching the normal launcher.
F4 preferences remain unchanged.

For deliberate classic-only diagnostic runs, launch:

```powershell
.\DOOM-Hellforge-Experimental-Compatibility.exe --classic-geometry
```

This explicit option uses `-nodigging` and therefore disables wall destruction.
Idle-search hibernation continues to bypass solid geometry as designed; the
independent conservative sprite filter remains available. Earlier classic-mode
performance figures do not establish performance with solid geometry enabled.

## Focused verification

- Five actual BSP sight-leaf checks: open two-sided, ordinary one-sided,
  missing back sector, missing front sector, and closed opening. Before the
  fix, the missing-back-sector test exited with `0xC0000005`; all five pass.
- SF2012 MAP01, MAP02, MAP15 and MAP21: 5 warmup + 140 alert/fire simulation
  tics each, using the real Doom II IWAD and WAD's embedded DeHackEd patch.
- Native GPU destruction and full save/load: stock MAP01, SF2012 MAP01 and
  SF2012 MAP02, with sprite filtering enabled. Measured wall-ray depth changed
  96→128.17, 64→95 and 64→160 units respectively.
- MAP02's nearest spawn wall initially failed the generic destruction fixture
  with default door shields. Disabling only key/switch door shields in an
  isolated test profile made that same fixture pass, confirming intentional
  protection. Defaults and user profiles were preserved.
- Actual MAP02 native classic-geometry run with living monsters and two weapon
  shots passed, exercising the original BSP sight path alongside rendering.
- Fourteen experimental launcher packaging checks passed, including embedded
  engine SHA, isolated profiles, destruction default and explicit classic mode.
- `git diff --check` passed. No historical full performance matrix was rerun.

Commands for the small regression and packaging checks:

```powershell
cmake --build build/profile-off --config Release --target doom-sight-sector-tests doom-headless --parallel 6
.\build\profile-off\Release\doom-sight-sector-tests.exe
.\launcher\build.ps1 -LauncherOnly -Experimental -EnginePath build/engine-profile/Release/doom.exe -OutputPath build/experimental-release/DOOM-Hellforge-Experimental-Compatibility.exe
.\launcher\ExperimentalBuildTests.ps1 -Launcher build/experimental-release/DOOM-Hellforge-Experimental-Compatibility.exe -Engine build/engine-profile/Release/doom.exe -Iwad test-references/DOOM2.WAD -Wads test-references/SF2012_final.wad,test-references/Abandon.wad,test-references/dmnsns2k-monsters.wad
```

Raw commands, logs, results, dump-decoding helpers and screenshots are under
`build/sf2012-fix/`. These are short targeted checks, not full-map playthroughs.
The extra external music WAD in the user's log was not available for replay.
Existing compiler conversion warnings and the launcher LAN async warning remain.

## Delivery

Self-contained test launcher:
`build/experimental-release/DOOM-Hellforge-Experimental-Compatibility.exe`.
Matching native engine and PDB:
`build/experimental-release/compatibility-engine/doom.exe` and `doom.pdb`.
Hashes and build identity are in `manifest-compatibility.json` beside the launcher.
Use this launcher normally to restore destruction; restart the game and apply
the desired F4 damage settings. Door shields can still intentionally protect
nearby walls. Older test launchers retain their original launch arguments.

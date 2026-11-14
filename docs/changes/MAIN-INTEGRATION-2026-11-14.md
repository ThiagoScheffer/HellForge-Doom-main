---
project: DOOM: HELLFORGE
build: 2026.11.14.1
updated_utc: 2026-11-13T19:26:29Z
status: main-working-tree-integrated-focused-checks-passed
index: ../README.md
---

# Main integration - 14 November 2026

The experimental source has been imported into the main checkout on **master**,
starting from `b59ad46c1d5dcc92874074b3a2805a9b5af4b521`. This is a local
working-tree integration prepared for review/commit. The experimental worktree
and its original build artifacts remain available. Existing unrelated main
changes, including the SDL cache lock and local oku WAD, were preserved.
The release version is **2026.11.14.1**; package metadata records exact UTC time.

## Included changes

- The opt-in profiler, deterministic benchmark and wake/transition diagnostics,
  retained as developer tooling. Normal builds compile all benchmark/profiling
  instrumentation out; no diagnostic runner is launched by the normal launcher.
- Conservative idle-search reuse, with its classic-geometry and compatibility
  guards. It does not skip thinkers, physics or state actions.
- Conservative sprite frustum filtering, shared shadow-bound limits and cache
  lifecycle handling, with fallbacks for uncertain presentation states.
- Immediate, stack-local duplicate false sight reuse for ambush monsters,
  controlled and persisted through F4. No cross-tic visibility cache.
- Voodoo player-body friction correction and conveyor footprint contacts,
  restoring the tested dmnsns2k/Abandon switch chains.
- Safe sight handling for flagged two-sided walls lacking a sector, covering
  the identified SF2012 crash. Solid-map wall destruction stays enabled through
  the normal launcher.
- Associated tests, proposals, dated investigations and build reports. Deferred
  spawning, broad AI throttling and acoustic distance caps were not implemented.
- README feature presentation, documentation index, current build metadata,
  normal release packaging and explicit diagnostic build separation.

Existing combat, survival AI, lighting/cache, reload stability, save recovery,
launcher theme and settings backup features from the baseline remain included.
No combat balance, actor/save layout, map-cache format or profile migration was
introduced by this integration.

## Performance controls

| F4 location | Setting key | Normal default | Restrictions |
|---|---|---|---|
| Performance / Monster visibility / Ambush sight reuse | `ambush_sight_reuse` | Off | Disabled for multiplayer, recording and playback |
| Performance / Sprite preparation / Conservative sprite filtering | `sprite_frustum_filter` | Off | Conservative artwork/shadow bounds; uncertain states retain original rendering |
| Gameplay / Monster performance / Idle monster hibernation | `monster_hibernation` | Off | Classic geometry only; disabled with solid/destruction modes, patched gameplay and documented compatibility exclusions |
| Gameplay / Monster performance / Swarm diagnostics | `swarm_debug` | Off | Aggregate console diagnostics |

Apply with Esc, F4 or Apply/close. F10 discards drafts. Normal launches use
`-solidmaps`; destruction does not require a diagnostic launch flag.
The native `-ambushsightreuse` option remains an explicit startup override, but
saved F4 preferences need no command-line flag. New settings are not seeded on
by the normal launcher. The experimental launcher retains its separate profile
and prior experimental-only defaults.

Main profiles continue to use `%APPDATA%/Local Games/DOOM Hellforge` and
`%LOCALAPPDATA%/DOOM-HellforgeLauncher`. Experimental profile data is not copied
automatically. Settings can be transferred deliberately with launcher export/import.

## Focused main-build verification

The normal Release engine was rebuilt with `DOOM_MONSTER_PROFILING=OFF`,
`DOOM_MONSTER_PROFILE_ARGUMENTS=OFF`, `DOOM_MONSTER_BENCHMARK=OFF` and the static
MSVC runtime. This is also enforced by the normal full launcher build script.
Matching engine and launcher resources report **2026.11.14.1** and the normal
HELLFORGE identity.

- Six CTest regressions passed: idle-search guards, Boom conveyor contacts,
  missing-sector sight, sprite frustum bounds, wound placement and weak-point rules.
- Six isolated native settings processes passed **14 checks**, covering default
  off, actual Performance-row selection, F4 apply, separate-process restore,
  F10 discard, immediate disable and saved-off restoration.
- **18** launcher persistence checks passed, including byte-for-byte export/import
  and Low-FX preservation of all three new performance keys.
- **17** launcher backup checks passed, including isolated UI/profile handling.
- **13** normal-release package checks passed: product/version/resources, native
  SHA, normal profile roots, destruction mode and opt-in defaults.
- Source parity, documentation links, option/version consistency and
  `git diff --check` passed; package hashes are in the release manifest.

The existing persistence harness requires a fresh profile: an initial repeat
against an already-applied test directory failed its starting-default assertion.
The final changed-fixture run used a fresh isolated directory. This required no
production settings behavior change.

The earlier native performance/image/state/RNG, save/load, type-126 teleport and
short replay evidence remains attached to its original experimental builds.
This migration did not repeat the full map/performance matrix, Sunder tests,
all LAN modes, replacement artwork or manual campaign completion. The normal
compile-out path passed the focused checks above, but its performance was not
re-benchmarked. Existing compiler conversion warnings and the launcher LAN
async warning remain. Historical reports retain their original dates, names,
coverage limits and earlier statements about isolation from main.

## Reproduce the main checks

```powershell
cmake -S duct-tape-doom-main -B build -DDOOM_BUILD_SDL=ON -DDOOM_RENDER3D=ON -DDOOM_SOUNDFONT=OFF -DBUILD_TESTING=ON -DDOOM_MONSTER_PROFILING=OFF -DDOOM_MONSTER_PROFILE_ARGUMENTS=OFF -DDOOM_MONSTER_BENCHMARK=OFF
cmake --build build --config Release --target doom doom-critical-smoke doom-sight-sector-tests doom-boom-carry-tests doom-sprite-frustum-tests doom-swarm-idle-tests doom-weakpoints-tests doom-wounds-tests --parallel 6
ctest --test-dir build -C Release -R "^(swarm-idle-guards|boom-conveyor-contacts|sight-missing-sector|sprite-frustum-bounds|monster-weakpoint-rules|wound-hit-placement)$" --output-on-failure
powershell -ExecutionPolicy Bypass -File launcher/build.ps1 -LauncherOnly -EnginePath build/Release/doom.exe
powershell -ExecutionPolicy Bypass -File launcher/ReleaseBuildTests.ps1 -Launcher dist/DOOM-Hellforge.exe -Engine build/Release/doom.exe -Iwad test-references/DOOM2.WAD -Version 2026.11.14.1
```

The build uses Visual Studio C++/CMake and owned test resources as described in
[BUILDING](../../BUILDING.md). Historical diagnostic scripts may need local
WAD paths adjusted; additional user-supplied WADs were not imported into Git.

## Artifacts, review and rollback

- Launcher: `dist/DOOM-Hellforge.exe`.
- Matching native engine: `dist/engine/doom.exe`.
- Matching PDB: `dist/symbols/2026.11.14.1/doom.pdb`.
- Release manifest: `dist/manifest.json`.
- Binary/documentation archive: `dist/DOOM-Hellforge-2026.11.14.1.zip`.
- Import lists, hashes, logs and backups: `build/main-migration-20261114/`.

Before mutation, the source import captured `main-before.zip`,
`experimental-source.zip`, `experimental.patch`, both statuses and a source hash
manifest. `release-before.zip` preserves the previous release and symbols.
`documentation-before.zip` preserves clean-baseline active documents and the
launcher test fixture extended during integration. Prior release symbols are
retained under that build evidence directory.

Rollback is selective: restore listed existing task files from the appropriate
backup (or their clean Git baseline for unchanged original documents) and remove
only newly imported files whose current hashes still match the integration
manifest. Restore the prior release from `release-before.zip`. Do not reset or
clean the whole working tree; unrelated user changes must remain intact. The
experimental checkout still supplies the original source as a comparison.

No Git history rewrite is part of this preparation. Review the local diff and
commit the integrated source as the next repository operation; publishing the
release remains a separate action.

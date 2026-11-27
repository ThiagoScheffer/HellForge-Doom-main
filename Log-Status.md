# HELLFORGE bug and debugging log

This is the project’s running record of bugs, investigations, and fixes. Use the IDs to find earlier work before debugging a similar symptom. Each entry records what was observed, what caused it, what changed, and how far verification went. The initial entries consolidate the issues investigated in the recent development sessions and link to earlier repository reports.

**Updated 2026-11-19 (NZ).** “Verified” refers to the scenario described in the entry. “Build verified” means the code compiled and packaged successfully. Keep manual checks and unconfirmed causes explicit.

| ID | Issue | Status |
| --- | --- | --- |
| HF-001 | Sunder 2308 slow exit | Verified in game with the smoke driver |
| HF-002 | Heap corruption on exit after storm audio | Verified in an isolated SDL reproduction |
| HF-003 | Pinky walks backwards while fleeing or seeking food | Build verified. In-game visual check open |
| HF-004 | Inferno lightning misses short trees | Actor mapping and regression verified. In-game visual check open |
| HF-005 | CMake fails after moving the checkout | Verified in a fixture and the actual checkout |
| HF-006 | Fast builds can package stale launcher settings | Generation and source hash verified |
| HF-007 | Launcher build fails when PowerShell finds two Python executables | Executable selection verified |
| HF-008 | Git tracks files inside an ignored reference folder | Git index and ignore rule verified |
| HF-009 | Launcher release output omits upstream notices | Notice copies verified |
| HF-010 | Burning City smoke has rectangular edges | Fix committed. In-game visual check open |

## Environmental Footprints 1.0

The accelerated Doom renderer can now transfer water, blood, and mud from live floor flats to a short boot trail. The existing corpse-pool trail shares the record budget and retains its separate blood and rain behavior. Release, Vulkan/Metal shader generation, the focused rules test, the stock Doom II MAP01 F4/config test, and scratch launcher packaging passed. GPU visual acceptance and performance measurements remain open. [Implementation, logs, and stock IWAD check locations](docs/changes/ENVIRONMENTAL-FOOTPRINTS-1.0.md).

## Smart Persistent Lighting Cache 2.0 — M1

Original Doom E1M1 repeated all six static calculations on death restart and produced the same numeric results. The old key included unrelated animation-table state. Doom II MAP01 also showed misleading baking labels on actual cache hits. M1 uses stage-specific numerical inputs and reports lookup, restoration, calculation and GPU staging independently.

The Release build and all 48 configured CTests passed. E1M1 restored six stages with zero ordinary bake dispatches. MAP01 passed ten warm restarts, ten warm save reloads, and a return from MAP02. The Sunder MAP02 direct-light entry exceeded 64 MiB and was reported separately while the other stages restored.

[Dependency audit, benchmarks and regression coverage](docs/changes/RELOAD-LIGHTING-CACHE.md#smart-persistent-lighting-cache-20--m1)

## Universal Atmospheric Fog — Sprite Coverage Fix

The world shader now resolves sprite exposure from the world-position sector lookup for every active haze profile. Monsters, decorations, pickups, and other world sprite draws use the same outdoor exposure and view-depth fog equations as nearby geometry. Deimos retains its whiteout equation.

The Vulkan SPIR-V and Metal MSL shader variants were regenerated. The Release engine built, and 13 focused CTest checks and the standalone Deimos fog regression passed. Matched Vulkan captures checked Burning City, Blood Hell, Starport, Inferno, and Deimos in a custom WAD. At the doorway, only the outdoor monster and tree changed. With fog disabled, both sprites matched the baseline. The held pistol and HUD matched in both cases. A geometry-only Blood Hell frame matched pixel for pixel, and a Blood Hell voxel fixture received the haze. Metal runtime was outside the Windows test coverage.

The Blood Hell capture showed a green armor halo through the fog. The separate bloom pass lacks the sector lookup texture needed to match the base sprite's outdoor extinction. Flat debris particles also use a separate shader.

## Environmental Corpse Reactions 1.0

The native renderer now uses physical acid and lava contact to drive corpse and fragment consumption. It retains native corpse actors for resurrection and filters floor blood at the liquid surface.

The Release engine built successfully. The loaded-map smoke driver passed 20 assertions, including raised support, wall blood, Arch-vile resurrection and save/load. The existing corrosive barrel fixture passed 45 assertions. GPU appearance and large-map performance remain open. [Implementation and validation](docs/changes/ENVIRONMENTAL-CORPSE-REACTIONS.md)

## HF-001 — Sunder 2308 waits on exit

**Reproduction.** Load MAP01 from `Sunder 2308.wad` with accelerated graphics and moving-sector relighting enabled. Idle window closes took about 0.19 seconds. The `relight-quit` smoke mode changes a nearby sector height and requests a normal exit as soon as it detects an active lighting worker. That run stayed in shutdown for at least 88 seconds before the harness timed out.

**Cause and fix.** Renderer shutdown joined the background relight thread while it still had a full batch to finish. Teardown now signals cancellation and joins the workers before releasing the map data they read.

**Evidence.** The same active-worker run exited with code 0 in 0.167 seconds after the fix. The engine and launcher were rebuilt. See commit [`12e3177`](../../commit/12e3177), [relight.cpp](duct-tape-doom-main/platform/scene3d/relight.cpp), and the [smoke driver](duct-tape-doom-main/tests/reload_smoke.c).

**Prevent recurrence.** Test cleanup while work is still active. When teardown discards results, let workers stop early and join them before releasing shared buffers.

## HF-002 — Storm audio can crash the game on exit

**Reproduction.** The Windows report showed `0xc0000374` in `ntdll.dll`. The archived report had no dump. An isolated SDL program reproduced the same exception by shutting down the audio subsystem before destroying the storm stream.

**Cause and fix.** SDL audio shutdown destroys its streams. Renderer cleanup still held the storm stream pointer and tried to destroy it afterward. Normal and error exits now release renderer-owned storm audio and unregister music before closing the audio subsystem.

**Evidence.** The old order produced `0xc0000374`. The corrected order exited with code 0, including a repeated cleanup call. Both storm regression checks passed. See [i_portable_system.c](duct-tape-doom-main/platform/i_portable_system.c) and [storm_shutdown.cpp](duct-tape-doom-main/tests/storm_shutdown.cpp).

**Prevent recurrence.** Track who owns each resource and release dependent resources before shutting down their subsystem. Keep a crash report’s detection site separate from the code that caused the corruption.

## HF-003 — Pinky faces the player while moving away

**Cause and fix.** Feeding and retreat return before the normal chase turn runs. Pinky now sets its sprite angle from its actual horizontal step, including a detour. It turns toward the food before biting.

**Evidence.** The engine and launcher rebuilt successfully. The visual check is still open. Observe a low-health Pinky retreat, turn around an obstruction, approach a corpse, and bite. See commit [`6405b4b`](../../commit/6405b4b) and [p_enemy.c](duct-tape-doom-main/linuxdoom-1.10/p_enemy.c).

**Prevent recurrence.** When an AI branch bypasses normal chase, check who owns movement, facing, and attack orientation. Use the actual step when the movement routine can choose a detour.

## HF-004 — Inferno lightning targets only the large tree

**Cause and fix.** The short tree selector paired `SPR_TRE1` with `MT_MISC75` (map thing 26). The engine’s short tree is `MT_MISC40` (thing 43). The correct pairing now applies to strike selection, burning, and charcoal.

**Evidence.** The local E3M1 WAD contained nine TRE1 trees and three TRE2 trees, with zero thing-26 decorations. The corrected fixture recorded 14 TRE1 and 15 TRE2 strikes under its synthetic rotation scenario. The E3M1 visual check is still open. See [info.c](duct-tape-doom-main/linuxdoom-1.10/info.c), [weather.cpp](duct-tape-doom-main/platform/scene3d/weather.cpp), and the [strike fixture](duct-tape-doom-main/tests/d33_compiled_integration.cpp).

**Prevent recurrence.** Cross-check WAD thing numbers, actor IDs, spawn states, and sprite IDs against the engine data. A fixture that copies the same mistaken mapping can pass while the real map fails.

## Build and repository incidents

| ID | Cause and fix | Evidence and prevention |
| --- | --- | --- |
| HF-005 | CMake recorded absolute paths in the main cache and FetchContent subbuilds. The build now backs up stale metadata and regenerates it after a move. | A moved fixture reproduced the error and recovered. The actual checkout configured successfully. Check dependency caches too, and keep incremental builds on the existing cache when the paths match. [Build-Common.ps1](tools/Build-Common.ps1), [moved checkout fixture](tools/tests/Moved-Checkout.Tests.ps1) |
| HF-006 | Fast builds could skip compiling `settings_panel.cpp` when its timestamp was older than the build output. The launcher only warned about a stale catalogue. Full builds refresh the source timestamp, and every launcher build regenerates its settings catalogue. | Verified the generated catalogue matched the source hash. Keep generated resources in the build path. A launcher-only build refreshes its catalogue and uses the existing engine binary. [build.ps1](launcher/build.ps1), [BUILDING.md](BUILDING.md) |
| HF-007 | `Get-Command` found both installed Python and the Windows Store alias. PowerShell combined the paths when invoking the generator. Discovery now chooses one command for each executable lookup. | The selected Python 3.14 executable launched and the script parsed. Treat executable discovery as a list and choose a single path before invocation. [build.ps1](launcher/build.ps1) |
| HF-008 | `/test-references/` was already ignored, but six files were tracked. The files were removed from Git’s index and kept on disk. | Confirmed the index no longer listed the folder contents and Git ignored sample paths. Use `git ls-files` and `git check-ignore` together when investigating this symptom. [.gitignore](.gitignore) |
| HF-009 | The source contained both upstream attribution notices, but launcher builds did not stage them with the output. The build now puts both notices and license texts in the release directory. | Generated copies matched the source file hashes. Verify license files in the actual package directory during release preparation. [Copy-ReleaseLicenses.ps1](tools/Copy-ReleaseLicenses.ps1), [THIRD_PARTY.md](licenses/THIRD_PARTY.md) |

## HF-010 — Burning City smoke has rectangular edges

**Cause and fix.** Burning City smoke visibly followed the rectangular shape of its particles. Commit `a9b6c5d` changed the effect to rounded layered puffs. Commit `64a85c8` then reused the Fluid smoke alpha masks to shape the puffs. [weather.cpp](duct-tape-doom-main/platform/scene3d/weather.cpp), [fluid_effects.inc](duct-tape-doom-main/platform/scene3d/fluid_effects.inc)

**Evidence.** The commits include integration fixtures and a smoke mask asset check. The in-game visual check remains open. Inspect the outline and fade of a puff against a contrasting sky in the Burning City profile. [Integration fixture](duct-tape-doom-main/tests/e22_fluid_smoke_integration.cpp), [mask asset check](duct-tape-doom-main/tests/fluid_smoke_mask_assets.cpp)

**Prevent recurrence.** When changing particle geometry or blending, check that the rendered opacity follows the smoke shape throughout its lifetime.

Earlier investigations have dedicated records for the [reload and lighting cache](docs/changes/RELOAD-LIGHTING-CACHE.md), [SF2012 sight and destruction](docs/changes/SF2012-SIGHT-AND-DESTRUCTION-FIX.md), [swarm activation](docs/changes/SWARM-ACTIVATION-FIX.md), and [survival and retreat](docs/changes/SURVIVAL-ESCAPE-AND-FRENZY-RETALIATION.md). See [all change reports](docs/changes/) for the wider project history.

## Maintaining this log

Before starting related work, search this log by map, symptom, subsystem, and error code. When a bug recurs, update the original entry with the new evidence. Add a new ID when the cause is different.

For each investigation, record the date, symptom, reproduction steps, map or build, root cause or current hypothesis, fix, verification result, and next open check. Link to commits, tests, or durable reports. Summarize local logs in the entry if the build directory is temporary. Leave private paths, personal data, and proprietary assets out of shareable evidence.

For a portfolio or CV, use claims supported by the recorded checks and describe your own contribution accurately. For example, HF-001 supports: “Diagnosed a renderer shutdown stall in a Doom source port and reduced exit time from over 88 seconds to 0.17 seconds in a reproducible Sunder MAP01 scenario.”

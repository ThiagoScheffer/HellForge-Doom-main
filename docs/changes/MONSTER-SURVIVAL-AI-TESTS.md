# Survival AI and feedback change report

Validated **10 November 2026**, Windows Release x64. Launcher **2026.11.10.0**: [Tuin-Doom-Enhanced-survival.exe](../../dist/Tuin-Doom-Enhanced-survival.exe). Native engine SHA-256: `9c0be32e88d2557404448572c29e37a0e237ea46eca5fc592e427028f4a2bc3f`. The launcher embeds this exact engine; its product version contains the matching first twelve hash characters.

## Delivered behavior and numerical defaults

- Metal-leg impacts: 14 orange sparks per bullet, 8 per qualifying pellet, maximum 32 per target/discharge and 128 per tic; 1.75x size, 0.15-0.35 s lifetime, directional spread and localized flash. One conservative ordinary-sector ricochet is supported. Monster Metal Sparks remains on by default.
- Headshots: blood-colored 1.75x burst at the captured impact, up to three wall marks within 64 units, amber 350 ms HS text, and an original 120 ms confirmation sound aggregated per discharge. Existing four selector modes remain. Headshot Sound defaults on; Feedback Off also silences it.
- Bleeding: the same four eligible species and bullet/pellet-only 30% or guaranteed critical onset. Tiers are 2/3/4 HP each second for 6/8/10 s, all with 20% slower walking. One increase per target/discharge; existing ticks are not postponed. Tier-two/three aggression grants about 15/25% extra decision opportunities, with cooldown-limited pain vocals and heavier trails/pooling. Monster Bleeding remains off by default.
- Feeding: optional Cacodemon/Pinky episodes at <=30% health, 384-unit search, 16-HP live bite, healing capped at actual damage and 5% maximum HP, 10% corpse recovery, 60% recovery ceiling. Search period 35 tics, pursuit/action timeout 105 tics, episode cooldown 175 tics, visible-player threat radius 128 units. Monster Feeding defaults off.
- Save trailer v3 uses 136-byte records and reads v1, v2 and older saves. It persists severity, attribution, feeding state/targets/timers and no-double-recovery markers. Actor layout and existing frenzy projectile provenance remain unchanged. Off switches cannot be re-armed by loading a save.

See [behavior matrix and complete bounded configuration table](MONSTER-SURVIVAL-AI.md) for every tuning key and the explicit species lists. Requested numerical defaults were retained.

## Automated validation

| Check | Result |
|---|---|
| Full configured Release build, including engines, tools and test executables | Passed |
| Registered CTest suite | **29/29 passed** |
| Combat policy, status, lifecycle and save assertions | **392 passed** |
| Native engine assertions, ordinary collision | **4,697 passed** |
| Native engine assertions, solid-map collision | **4,697 passed** |
| GPU scenes | **Eight captures saved and inspected** |
| Synthesized confirmation WAV | Exported successfully, measured peak 0.32 |

Counts are assertions, not distinct gameplay scenarios. Native assertion totals include sample-by-sample validation of the original sound waveform. The previous four-human coverage and extended monster, wound, damage, occlusion, projectile, shotgun, frenzy and save checks remain in the suites.

New coverage includes shotgun spark budgets, cosmetic RNG independence, unknown/brain/chassis/splash exclusions, particle size/lifetime bounds, all feedback modes and sound aggregation, first-wall blood placement across adjacent linedefs, missing/sky textures, hidden blood, tier caps/refresh/tick counts, 15/25 opportunity credits, vocal cooldowns without state changes, healing caps, prey/corpse whitelist, periodic searches, threats/interruptions, pursuit timeout, Cacodemon height, complete native feeding animations, corpse removal, no double recovery, v1/v2/v3 compatibility, source-body replacement and stale target cleanup. Native v3 save/load restores active hemorrhage and feeding references through the actual game save boundary.

Logs: [full build](../../build/survival-all-build.log), [CTest](../../build/survival-regression.log), [ordinary integration](../../build/critical-smoke/survival-integration.log), [solid integration](../../build/critical-smoke-solid/survival-integration.log), [GPU fixture](../../build/survival-visual/gpu.log).

## Observed visual evidence and audio artifact

- [Stronger metal-leg sparks](../../build/survival-visual/survival-metal-sparks.png)
- [Green head blood and nearby wall splatter, with crosshair-off HS text](../../build/survival-visual/survival-head-blood-wall.png)
- [Moving hemorrhage trail](../../build/survival-visual/survival-hemorrhage-moving.png)
- [Stationary hemorrhage accumulation](../../build/survival-visual/survival-hemorrhage-pool.png)
- [Pinky eating animation](../../build/survival-visual/survival-pinky-eating.png) and [completed recovery/corpse removal](../../build/survival-visual/survival-pinky-feeding.png)
- [Cacodemon bite animation](../../build/survival-visual/survival-caco-biting.png) and [successful bite/recovery at feeding height](../../build/survival-visual/survival-caco-feeding.png)
- [Headshot confirmation audition WAV](../../build/survival-audio/headshot-confirm-dry.wav), containing three spaced variations through the existing offline mixer

The images are actual engine captures with stock owned Doom II artwork. Bleeding fixtures use extra health to make tier progression visible; feeding fixtures use the stock maximum-health thresholds and actual AI/attack frames. The observer and prey are controlled fixtures. No new artwork or third-party sound asset was needed.

## Limits and balance observations

Feeding deliberately uses conservative clear-corridor geometry. It does not solve arbitrary mazes or guarantee that every theoretically reachable flying route will be chosen. Searches are bounded and may defer food beyond their candidate budget. Corpse eligibility excludes Baron as a boss, and excludes mechanical enemies, Lost Souls and props. Confident sprite anatomy and projectile contact reconstruction retain their existing stock/replacement-art limitations.

Visual verification covers the representative stock scenes linked above. Every replacement WAD, floor/wall angle, graphics combination and ricochet has not been visually certified. Solid-world sparks retain their ballistic effect without a guessed mesh ricochet. Live-play subjective audio loudness/mix was not auditioned; waveform generation, notification gating/aggregation and the exported sample were verified. Native fixtures ran with sound output disabled. Extended real-map balance and long-duration performance/memory tests were not performed.

The warmed 10,000-query collision fixture measured 0.168990 us for the original human path and 0.177750 us for expanded anatomy in ordinary collision, and 0.177540 / 0.177310 us in solid-map collision. These are isolated cached query timings, not a full frame-time certification.

A separate balance recommendation is to playtest clustered corpses, feeding under continuous fire, and tier-three Pinky survivability. No numerical default was changed based on that recommendation. Existing compiler conversion warnings and the launcher LanMenu CS4014 warning do not prevent successful builds. Fixture processes exited; no browser or Node test processes were started.

## Exact changed source and documentation files

### Gameplay and persistence

- [duct-tape-doom-main/linuxdoom-1.10/p_weakpoints.h](../../duct-tape-doom-main/linuxdoom-1.10/p_weakpoints.h)
- [duct-tape-doom-main/linuxdoom-1.10/p_weakpoints.cpp](../../duct-tape-doom-main/linuxdoom-1.10/p_weakpoints.cpp)
- [duct-tape-doom-main/linuxdoom-1.10/p_feeding.inc](../../duct-tape-doom-main/linuxdoom-1.10/p_feeding.inc)
- [duct-tape-doom-main/linuxdoom-1.10/p_enemy.c](../../duct-tape-doom-main/linuxdoom-1.10/p_enemy.c)
- [duct-tape-doom-main/linuxdoom-1.10/p_mobj.c](../../duct-tape-doom-main/linuxdoom-1.10/p_mobj.c)
- [duct-tape-doom-main/linuxdoom-1.10/p_setup.c](../../duct-tape-doom-main/linuxdoom-1.10/p_setup.c)
- [duct-tape-doom-main/linuxdoom-1.10/p_inter.c](../../duct-tape-doom-main/linuxdoom-1.10/p_inter.c)

### Rendering, audio and F4

- [duct-tape-doom-main/platform/scene3d.h](../../duct-tape-doom-main/platform/scene3d.h)
- [duct-tape-doom-main/platform/settings_panel.cpp](../../duct-tape-doom-main/platform/settings_panel.cpp)
- [duct-tape-doom-main/platform/scene3d/internal.h](../../duct-tape-doom-main/platform/scene3d/internal.h)
- [duct-tape-doom-main/platform/scene3d/scene.cpp](../../duct-tape-doom-main/platform/scene3d/scene.cpp)
- [duct-tape-doom-main/platform/scene3d/impacts.cpp](../../duct-tape-doom-main/platform/scene3d/impacts.cpp)
- [duct-tape-doom-main/platform/scene3d/spray.cpp](../../duct-tape-doom-main/platform/scene3d/spray.cpp)
- [duct-tape-doom-main/platform/scene3d/blood.cpp](../../duct-tape-doom-main/platform/scene3d/blood.cpp)
- [duct-tape-doom-main/platform/audio/impact_synth.h](../../duct-tape-doom-main/platform/audio/impact_synth.h)
- [duct-tape-doom-main/platform/audio/impact_synth.c](../../duct-tape-doom-main/platform/audio/impact_synth.c)
- [duct-tape-doom-main/platform/audio/sound_options.h](../../duct-tape-doom-main/platform/audio/sound_options.h)
- [duct-tape-doom-main/platform/i_sdl_sound.c](../../duct-tape-doom-main/platform/i_sdl_sound.c)

### Tests

- [duct-tape-doom-main/tests/critical_smoke.cpp](../../duct-tape-doom-main/tests/critical_smoke.cpp)
- [duct-tape-doom-main/tests/weakpoints_rules.cpp](../../duct-tape-doom-main/tests/weakpoints_rules.cpp)
- [duct-tape-doom-main/tests/weakpoints_stubs.cpp](../../duct-tape-doom-main/tests/weakpoints_stubs.cpp)
- [duct-tape-doom-main/tests/feedback_integration.inc](../../duct-tape-doom-main/tests/feedback_integration.inc)
- [duct-tape-doom-main/tests/survival_rules.inc](../../duct-tape-doom-main/tests/survival_rules.inc)
- [duct-tape-doom-main/tests/survival_integration.inc](../../duct-tape-doom-main/tests/survival_integration.inc)
- [duct-tape-doom-main/tests/survival_visual.inc](../../duct-tape-doom-main/tests/survival_visual.inc)

### Packaging and documentation

- [launcher/VERSION](../../launcher/VERSION)
- [launcher/graphics.cfg](../../launcher/graphics.cfg)
- [README.md](../../README.md)
- [docs/changes/ENHANCED-MONSTER-COMBAT.md](../../docs/changes/ENHANCED-MONSTER-COMBAT.md)
- [docs/changes/MONSTER-FEEDBACK-AND-STATUSES.md](../../docs/changes/MONSTER-FEEDBACK-AND-STATUSES.md)
- [docs/changes/MONSTER-SURVIVAL-AI.md](../../docs/changes/MONSTER-SURVIVAL-AI.md)
- [docs/changes/MONSTER-SURVIVAL-AI-TESTS.md](../../docs/changes/MONSTER-SURVIVAL-AI-TESTS.md)

## Reproduction

```text
cmake --build build --config Release --parallel 6
ctest --test-dir build -C Release --output-on-failure
build/Release/doom-weakpoints-tests.exe
doom-critical-smoke <DOOM2.WAD> <critical-arena.wad> <absolute-output-dir>
doom-critical-smoke <DOOM2.WAD> <critical-arena.wad> <absolute-output-dir> --solid
doom-critical-smoke <DOOM2.WAD> <critical-arena.wad> <absolute-output-dir> --visual-survival
doom-sfx-render <DOOM2.WAD> -kind headshot-confirm -room dry -gun off -out <existing-output-dir>
```

Use separate fixture working/output directories so configs and saves stay isolated. The visual fixture needs a GPU; ordinary/solid integration uses SDL dummy video/audio. The test arena is generated by the existing `tests/ragdoll_arena.py` helper. Launcher packaging uses the existing `launcher/build.ps1 -LauncherOnly -OutputPath ...` workflow.

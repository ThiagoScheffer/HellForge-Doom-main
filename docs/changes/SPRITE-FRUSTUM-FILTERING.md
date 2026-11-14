---
build: 2026.11.12.1-experimental
updated_utc: 2026-11-13T16:24:12Z
status: optional-experiment-targeted-checks-passed-manual-coverage-pending
index: ../README.md
---

# Phase 3A: conservative sprite frustum filtering

Implemented only in `experiment/monster-profiling-phase2`. The pre-change source
and status were preserved under `build/sprite-filter/before*`. Main and existing
release artifacts remain unchanged.

## Behavior and controls

F4 → Performance → **Conservative sprite filtering** controls
`sprite_frustum_filter`, default **off**. Newly created experimental launcher
profiles seed it on; existing profiles retain their settings and need manual
enablement followed by Apply/F4/Esc. F10 still discards drafts. Turning the option
off restores the original preparation path on the next frame.

The filter considers ordinary living sprite monsters before lighting. It reads
actual patch dimensions/offsets, uses interpolated placement and floor/ceiling
fitting, and includes a conservative floor-shadow envelope. Shadow projection
limits are shared with the shadow renderer. Only bounds wholly outside a camera
side plane are rejected; near-plane intersections, invalid or uncertain bounds
are retained. Collision radius, floor visibility masks, distance cutoffs and wall
occlusion are not used as sprite visibility.

Voxel actors, corpses, active poses, wounds, liquid sectors and uncertain actors
keep normal preparation. Active mirrors, reflective world passes, flashlight
silhouettes and special camera views bypass filtering for the frame. PS2 style
also keeps the original actor path. Static glass cubemap probes were inspected:
they explicitly exclude sprites; glass mirrors use the bypass above.

Monster thinkers, AI, attacks, collision, triggers, gameplay RNG and save formats
are unchanged. Light-source collection, effect/lifecycle updates and critical-hit
surface callbacks remain independent of the filter.

## Important implementation details

Artwork metadata is cached by sprite lump, read without changing zone-cache
allocation order, and cleared with level/image caches. Metadata scratch storage
is reusable and capped at 32 MiB; oversized or unrecognized resources fall back
to normal rendering. Image ownership remains on the render thread.

An initial presentation test caught a real dependency: removing cached surfaces
for skipped actors changed subsequent cosmetic impact handling. The final path
therefore maintains lightweight `woundView` updates and the original image-loading
order for rejected actors. It skips lighting, shadow geometry and sprite emission,
not those surface semantics. The pose/wound/blood reference scene now matches.

The existing actor array/order is retained. Worker callbacks immediately return
for rejected entries before computing lighting; counters report computations
retained, not a reduction in callback dispatch count. There are no per-actor log
messages or steady-state allocations in classification.

`platform/sprite_visibility.h` exposes a C-compatible read-only
`I_SpriteVisibilityStats()` snapshot for main-thread diagnostic consumers.
Native replay writes per-frame aggregate counts and filter time to `visibility.csv`,
including fallback reasons. This is not actor/save state.

## Final measured comparison

Two alternating off/on pairs per map, 35 warmup and 140 measured frames per run:
same executable, resource hashes, initial camera, native Vulkan renderer,
960×540/FIFO, warm lighting cache and identical graphics/optional-AI settings.
No forced GPU fencing, screenshots, state hashing or concurrent builds were used
in timing runs. Loading/warmup are excluded. Measurements are wall-clock native
`D_Display` calls, including normal renderer waits, rather than GPU execution time.

| Map / pair | Off median ms | On median ms | Off p95 ms | On p95 ms |
|---|---:|---:|---:|---:|
| Doom II MAP01 / 1 | 7.968 | 7.961 | 8.992 | 8.856 |
| Doom II MAP01 / 2 | 7.901 | 7.875 | 8.854 | 8.937 |
| oku MAP01 / 1 | 66.723 | 52.046 | 216.523 | 207.603 |
| oku MAP01 / 2 | 67.772 | 52.406 | 226.300 | 204.816 |

Oku's median frame-call cost falls approximately **22.0% and 22.7%** in the two
pairs. Classification costs about **1.86 ms/frame**, included in those results.
Of 27,412 actor preparations, 14,016 are rejected and 13,396 lighting computations
retained. 3,264 liquid actors remain on the fallback path. Simulation medians are
13.50–13.79 ms, measured separately; no AI speedup is claimed.

Stock control classification costs about 0.004 ms and rejects six of 51 actors.
Median differences are under 0.4%, with mixed p95 movement: no meaningful control
speedup or reproducible toggle-on regression is established. These compare the
new revision off/on, not an archived executable's disabled-path overhead.
Two pairs are limited evidence, not a general FPS guarantee. Relighting spikes
over 200 ms remain; this patch does not solve them.

## Focused verification

- Sixteen pure bounds tests pass, covering artwork offsets, large replacement
  dimensions, fitting, camera edges, near-plane intersections, invalid data,
  vertical viewing, wide FOV and shadow-only visibility.
- Final oku off/on screenshots match exactly. Full-lighting stock screenshots
  have small photometric differences; an unfiltered repeat also differs in the
  same region. That comparison remains inconclusive rather than being treated
  as pixel-exact acceptance.
- Controlled native reference images match exactly for the real pose/wound/blood
  scene, reflective water, liquid fallback and flashlight silhouettes. Counters
  assert that the intended fallback was actually exercised. The flashlight image
  fixture explicitly uses a fixed test-only tint/bounce clock; playable builds
  retain ordinary wall-clock smoothing. Earlier failed prototype images/logs are
  retained separately, not reported as passing final evidence.
- Native off/on canonical gameplay state and RNG call/trace/index match for 335
  stock-map tics including save/load, restart/death recovery, spawning, death,
  resurrection fixture and removal; another 40-tic pair matches on oku.
- Default/write/read settings processes pass for the new persisted option.
- The Abandon platform and dmnsns2k release-chain regressions are retained and
  rerun for delivery. No historical full matrix is used.

Real voxel artwork and a dedicated dynamic glass-mirror image fixture were not
available in this test set. Their conservative bypass paths are present, but
those visual cases and broader WAD playthroughs remain manual coverage gates.
Network/demo modes were not revalidated. This is an optional experimental build,
not approval to migrate into main.

## Evidence, reproduction and test executable

Sources: `platform/sprite_frustum.h`, `platform/scene3d/geometry.cpp`, and
`tests/sprite_filter_runs.py`. Raw evidence is under `build/sprite-filter/`:
`final-bench/`, `images-final/`, `controlled-final/`, `state-stock/`, `state-oku/`,
settings logs, bounds results and regression logs. Manifests record resource and
executable hashes; images and generated room WADs remain local build artifacts.

The native runner takes `--engine`, `--iwad`, `--wad`, `--out`, optional `--cache`
and `--stage bench|probe|fallback|verify`. Every child has an external timeout.
`--stage bench` runs only the specified two-pair stock/oku matrix. Verification
uses separate runs, so hashing/readbacks do not contaminate those timings.

Delivery: `build/experimental-release/DOOM-Hellforge-Experimental-Visibility.exe`,
with matching native symbols and a manifest. Existing experimental users should
enable the F4 option explicitly. Uncheck it to compare or roll back behavior.
Wall occlusion, deferred spawning and relighting optimization are not implemented
in this phase.

---
build: 2026.11.12.1-experimental
updated_utc: 2026-11-13T13:47:52Z
status: diagnosis-complete-streaming-not-implemented-interactive-playthrough-pending
index: ../README.md
proposal: ../proposals/OKU-STRESS-TEST.md
---

# oku2v31 MAP01: extreme-load diagnosis

All source, diagnostics and artifacts remain in the experimental worktree,
`experiment/monster-profiling-phase2`, based on `b59ad46`. The existing uncommitted
profiling and activation fixes were preserved; `build/oku-stress/before*` records
the pre-task source/status. Main and release artifacts are unchanged. The user
confirmed the previous `dmnsns2k-monsters.wad` MAP01 playtest passed before this
investigation. That confirmation is manual evidence, not an automated claim.

## Finding

The bounded native replay enters oku MAP01 and completes. An indefinite freeze
was **not reproduced**. However, cold first-frame preparation takes about 19
seconds, and steady graphical replay is only about **4.5 FPS** on this machine.
The minimum diagnostic success criterion is met; acceptable extreme-map play
performance and safe monster streaming are **not** achieved.

Creating actors is not the principal observed bottleneck: complete map setup is
about 69 ms. Idle headless simulation is about 6.6 ms/tic, versus about 152 ms
median native render-call time, with much larger relighting spikes. Removing
all monsters still leaves a 91 ms median render call. Hibernation produces no
meaningful improvement in this workload; no speedup is claimed from these runs.

## Resources and protocol

`oku2v31.wad` SHA-256:
`c7dfcdbbaf33b5f7b13c8405a0b36c562c3c7d9e7f63ad48743c999dccb85061`.
The original Doom II IWAD was used. Audit confirms 27,700 THINGS, 23,241 recognized
monster placements, 3,797 sectors, 24,023 lines, 15,968 subsectors, 27 Player 1
starts, 245 special-126 and one special-125 monster teleporter lines, and zero
classic sound-block lines. **25 monster placements are multiplayer-only**, so
hard-skill single-player correctly instantiates **23,216 living monsters**.
Initial total active mobjs are 27,671, including voodoo dolls and other objects.

The runner uses child processes with a 60-second external timeout, retained logs,
peak process working-set sampling, flushed startup/tic/frame progress, hashes,
and raw timing CSVs. All final isolation cases completed; none timed out. Initial
35-tic checks were triage only. Reported simulation measurements use 70 warmup
plus 350 measured tics; ordinary native cases use five warmup plus 70 measured
frames/tics. One run per case was used to keep this investigation bounded.
These short runs establish bottlenecks, not precise small percentage gains.

Native replay uses Vulkan, 960×540, FIFO presentation and normal default graphics.
Optional tactics, feeding, frenzy, stuck recovery and Fluid behavior are disabled
consistently. Benchmark setup gives godmode/plasma, retains the initial camera
and does not force monster targets for idle or alert scenarios. Alert fires
normally after tic 35; it is not a test of all 23,216 monsters fighting at once.
No Sunder tests were run.

FPS below is **fixed-tic native replay FPS**, calculated from steady frame
intervals, including the intervening simulation tic. Replay advances one tic per
render; it is not a claim about ordinary interactive catch-up scheduling.
Headless has no FPS. CPU tic and render timings are measured separately.

## Measured results

| Configuration | Map setup ms | First tic ms | Median / p95 / p99 tic ms | Peak process RAM MiB | Average replay FPS | p95 render-call ms |
|---|---:|---:|---|---:|---:|---:|
| Headless, monsters normal | 68.82 | 7.33 | 6.561 / 7.388 / 7.820 | 36.2 | N/A | N/A |
| Headless, no monsters | 66.47 | 1.18 | 0.960 / 1.218 / 1.704 | 28.3 | N/A | N/A |
| Headless, hibernation on | 70.31 | 6.82 | 6.472 / 7.502 / 7.895 | 37.3 | N/A | N/A |
| Headless, ordinary firing | 69.04 | 6.45 | 6.593 / 7.453 / 8.212 | 36.1 | N/A | N/A |
| Native, monsters normal | 68.90 | 11.24 | 32.927 / 44.452 / 47.236 | 1,417.6 | 4.50 | 451.20 |
| Native, no monsters | 66.42 | 1.64 | 5.427 / 6.833 / 7.064 | 1,120.6 | 7.31 | 435.64 |
| Native, hibernation on | 74.22 | 10.94 | 33.012 / 43.941 / 50.634 | 1,489.6 | 4.57 | 390.68 |

RAM is Windows peak process working set sampled externally, not total committed
memory or GPU VRAM. `mobj_t` is 224 bytes here: the recognized live monsters'
actor structures alone occupy about 4.96 MiB. Actor-side records and allocator
overhead add more, but eliminating those structures cannot explain away the
observed graphical memory footprint.

Native cold first render calls took 19,378 ms with monsters and 18,723 ms without.
Hibernation's 2,075 ms first render reused the lighting cache: **that difference
is cache reuse, not a hibernation loading improvement**. Initial cold lighting
logs show about 100,000 static lights, including about 94,000 window panels, a
roughly 10.8-second light bake and additional bounce/sky bakes.

## Ranked measured bottlenecks

An additional 35-frame instrumented native run preserves separate main/worker
scope timings and detailed renderer stage logs. Its median tic cost rises to
43.46 ms and median render time to 162.52 ms. Probes are invasive: use the ordinary
cases above for performance numbers. Calibration reports about 62.5 ns per empty
scope and 14.7 ns per clock read. Worker CPU totals overlap main-thread waiting
and must not be added to main-thread elapsed time.

1. **Per-frame actor presentation**, `platform/scene3d/geometry.cpp::geometry`,
   `lightThing`, `addThing`: observed actor-stage median about 76.8 ms. It collects
   and lights actors across all sectors before presentation. This includes sprite
   lighting, shadow work and geometry; it is not an AI timing. Worker sprite-light
   scopes alone record about 8.3 CPU-seconds across workers in this diagnostic.
2. **Live relighting/synchronization stalls**, the first stage of
   `platform/scene3d/scene.cpp::prepareFrame` (including `syncBakeThings` and
   `relightMovingSectors`, the latter in `relight.cpp`):
   sampled prepare-stage relighting spikes exceed 400 ms; the diagnostic maximum
   is about 969 ms. This stage has not been subdivided further. Cold baking in
   `bake_lights.cpp` is another,
   separate startup cost. The renderer category's exclusive time includes these
   uninstrumented descendants; it is not pure GPU work.
3. **World preparation**, `geometry.cpp::flatGeometry` and `wallGeometry`: sampled
   medians about 16.0 and 15.2 ms, plus visibility, masks and surface-light work.
4. **Simulation** remains real overhead but does not explain this idle freeze
   report: roughly 6.6 ms headless, with 23,216 living monsters. Native renderer
   hooks and backend conditions raise tic cost; headless and native timings are
   reported separately rather than treated as identical implementations.

The next useful optimization investigation is renderer-side conservative
visibility filtering before sprite lighting/submission. It must account for
shadows, mirrors, reflections, ragdolls and emissive effects; simply hiding distant
actors is not correct. No such renderer behavior change was slipped into this
diagnostic revision. Large active fights and digging/solid-map mode remain separate
workloads and may expose different costs.

## Deferred spawning boundary

| Requested metric | Streaming OFF, native | Streaming ON |
|---|---|---|
| Map setup / first render | 68.90 ms / 19,378 ms cold | Not implemented; not measured |
| Initial living monsters / pending records | 23,216 / 0 | Not measured |
| Peak process RAM | 1,417.6 MiB | Not measured |
| First / median / p95 / p99 tic | 11.24 / 32.927 / 44.452 / 47.236 ms | Not measured |
| Average replay FPS / p95 render call | 4.50 / 451.20 ms | Not measured |
| ESC/menu | Focused queue-to-menu test below | Not tested |
| Monster teleport / save-load | Targeted hibernation-off/on checks passed | Not tested |

No placements have been proven safe to defer, and no fake streaming switch was
added. Deferred-only and deferred-plus-hibernation cases are therefore unavailable,
not successful zero-cost runs. Existing hibernation is not deferred spawning.

Audit of `P_SpawnMapThing`, `P_SpawnMobj`, `P_RunThinkers`, BLOCKMAP consumers,
radius attacks, sound floods, monster-only teleporters, boss scans and save actor
indices exposes unresolved ordering dependencies. Creation consumes game RNG;
idle actors still advance state/cursors; their collision presence can matter
before sight or sound activation. Later materialization must preserve those facts
and original thinker/BLOCKMAP ordering. Kill totals, boss/level progression and
serialized references also require a real registry/persistence design. No sound
blocks does not prove current sound connectivity, because closed geometry matters.
Distance or initial invisibility is insufficient proof. All player starts and
voodoo actors retain the normal creation and movement paths.

Given the measured small actor-creation cost and much larger graphical costs,
implementing an unproven deferred registry would add encounter/save risks without
evidence that it fixes the observed bottleneck. Streaming's reduction and
compatibility acceptance gates remain unmet.

## Input, correctness and limitations

`TryRunTics` works from a bounded available-command batch; `NetUpdate` services
input between batches/tics and caps queued commands. No unbounded catch-up loop
was found. Loading already pumps SDL window events while baking, but does not
dispatch the normal menu during that work. A single long frame/bake still blocks
menu dispatch. No game tics were dropped and no simulation threading was introduced.

The initial menu fixture failed because `DOOM_TEST_DRIVER` intentionally ignores
desktop keyboard events. Those failures remain in the logs as inconclusive
harness results. A tagged SDL timer event now enters the queue during steady
rendering, is translated to Escape on the main thread and exercises the real Doom
menu responder. Desktop input isolation remains intact. The focused check opens
the menu in **130.630 ms**, passing the explicit **250 ms** test target, then closes
it before the next simulation step. This is one synthetic steady-load sample,
not a physical keyboard latency guarantee or a successful cold-bake ESC test.

Separate off/on headless runs match canonical state and game RNG calls, trace
and index after each of 420 tics. A second pair exercises save/load continuation,
death/Use recovery, removal/respawn fixture and a monster-only teleport. Teleport
line 7820 succeeds through the normal crossing dispatcher and real destination
collision path; contact is injected by the fixture, not a complete navigated
encounter. This does not validate every teleporter or the complete oku campaign.
The earlier successful voodoo friction fix is preserved.

The ordinary playable executable also completed a four-frame capture through
the actual game loop, advanced from tic 5 to tic 10, and exited normally within
the external 30-second limit (15.72 seconds total, including a 14.35-second cold
renderer build). Evidence and the inspected contact sheet are in
`build/oku-stress/actual-loop/`. GPU screenshot readbacks affect these frames,
so they are smoke/visual evidence and are excluded from FPS comparisons.

Raw evidence: `build/oku-stress/final/`, `correctness/`, `menu/`, corresponding logs,
and `wad-audit.json`. Builds pass with probes compiled out and compiled in. The
targeted prior MAP01 activation regression is retained. Broad old matrices were
not repeated. Full oku manual progression, cold-bake menu latency, huge active
battles, digging/solid mode and streaming save compatibility remain unverified.

## Reproduce and test build

`tests/monster_stress.py` takes `--headless`, `--native`, `--iwad`, `--wad`, `--out`
and optional `--cases`, `--timeout`, `--cache`. It preserves failed starts,
timeouts and partial progress rather than requiring Escape to stop a child.
Use `--cases verify,verify-hibernate,lifecycle,lifecycle-hibernate` for the
bounded state/save/teleport comparison, and `--cases native-menu` for input.

The playable diagnostic build is under `build/oku-stress/engine/`; use
`Play-OKU.cmd`. Normal test launches do not enable hot-path probes; optional
`Test-OKU.ps1 -Profile` does, with measurable overhead. Its configs/cache/logs are
isolated. Cold baking can take around 20 seconds before the first frame. This
build is for reproducing and measuring the remaining bottleneck, not a claim of
streaming or a fixed extreme-map frame rate. Main migration is not approved.

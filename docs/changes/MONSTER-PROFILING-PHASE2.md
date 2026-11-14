---
build: "2026.11.12.1"
updated_utc: "2026-11-13T11:01:29Z"
status: "concluded at user request; profiling implemented, benchmark matrix partial"
baseline_commit: "b59ad46c1d5dcc92874074b3a2805a9b5af4b521"
tests: "MONSTER-PROFILING-PHASE2-TESTS.md"
index: "../README.md"
---

# HELLFORGE Phase 2: monster profiling

Base: b59ad46c1d5dcc92874074b3a2805a9b5af4b521.
Branch: experiment/monster-profiling-phase2.
Started: 12 November 2026 (Pacific/Auckland).

This revision contains instrumentation and diagnostic drivers only. The main
checkout, release launcher, gameplay algorithms, actor layout, and save format
are unchanged. Work was concluded on 14 November 2026 (Pacific/Auckland),
at the user's request to stop extended testing and deliver the current work.

## Delivered result and acceptance

The opt-in profiling revision, simulation/native drivers, reproducible inputs,
population diagnostics, query histograms and canonical comparisons are built.
The ordinary engine is `build/engine-profile/Release/doom.exe`, with matching
`doom.pdb`. It has benchmarking OFF and profiling ON; enable it explicitly with
`-profilemonsters <output-directory>`. The release launcher was not replaced.

The full final performance matrix was not completed. No FPS improvement,
gameplay optimization, or validated Phase 3 winner is claimed. The original
Phase 2 acceptance condition for selecting a measured optimization target
therefore remains unmet. No Phase 3 optimization has been implemented.

Final-revision evidence already completed:

| Check | Result |
|---|---|
| Native CTest | 31/31 passed |
| Real-engine combat/wound/status integrations | 4,697 checks passed |
| Settings persistence and backup | 23 native process runs and 18 launcher checks passed |
| Generated large-map/Boom compatibility | 16 cases plus Boom assertions passed; no Sunder |
| Classic canonical comparison | 90 process runs, 75 comparisons, no mismatches |
| Save/load/lifecycle canonical comparison | 6 runs, 5 comparisons, no mismatches |
| Optional-preset canonical comparison | 14 runs completed; 10 completed FluidMonsters comparisons matched; remainder incomplete |
| Actual demo recording/playback | Matching demo bytes and canonical/RNG traces |
| Native UDP | 346 shared tics matched |
| Basic in-game host/join | 348 shared tics matched |
| Native reload with profiling | Ten restarts, ten save loads and ten death-save recoveries passed |

Canonical verification now includes warmup in `warmup.csv`; negative tic
numbers identify that interval. The timed simulation samples remain separate.
See `build/v3-final-verify`, `v3-final-lifecycle`, `v3-final-extended-verify`,
`v3-final-regressions.log`, `v3-combat.log`, `v3-settings.log`, `v3-bigmap.log`,
`v3-demo`, `v3-udp`, `v3-lobby`, and `reload-profile/v3-*.log` for raw evidence.
These generated files are local artifacts, excluded from Git.

Earlier full UI/photo/wheel lobby checks failed the wheel assertion both with
profiling enabled and disabled. Their shared simulation traces matched up to
failure. That UI/harness failure remains unresolved; its cause was not established. The basic native
network passes are not a claim that the extended UI suite passed.

The original checkout's existing Gradle lockfile modification was preserved.
The experimental changes remain on `experiment/monster-profiling-phase2` in
the sibling worktree. A final archive refresh was not executed because the
automatic approval service reached its usage limit. Existing builds, logs,
manifests and earlier source snapshots remain available.
The experimental changes are uncommitted. Use the current worktree files for
the latest implementation; the earlier exported patch/package predates the
final diagnostic refinements.

## Preliminary measurements, not final-revision performance claims

An earlier probe revision completed 225 sustained simulation runs: five maps,
three scenarios, five repetitions and three probe modes, with 350 warmup and
2,100 measured tics. Its exact source snapshot and binaries are retained under
`build/final-classic/`. These measurements must not be relabeled as results
for the later refined probes.

Chase medians in milliseconds (median of the five run medians):

| Map | Compiled out | Compiled in, disabled | Enabled |
|---|---:|---:|---:|
| MAP01 | 7.816 | 7.690 | 12.973 |
| MAP32 | 4.241 | 3.620 | 6.230 |
| MAP03 | 1.840 | 1.869 | 3.471 |
| MAP31 | 2.256 | 2.279 | 4.165 |
| MAP02 | 0.377 | 0.382 | 0.708 |

Run variation, especially MAP32, prevents interpreting negative differences
as improvements. Raw samples and median/p95/p99 summaries are in
`build/final-classic/analysis.json`. Enabled instrumentation materially
perturbs hot paths. The final revision adds counter calibration and removes
repeated disabled iterator flag calls, but its sustained overhead matrix was
not finished.

The earlier MAP01 chase probes counted approximately 11,654 position queries
and 109,613 BLOCKMAP thing entries per tic. Position and BLOCKMAP work were
the largest measured classic simulation areas. Solid-world correctness runs
also exposed substantial `SV_Sight` work and support-cache clears; their
timings are excluded from performance conclusions because they were
verification runs.

| Candidate | Evidence and remaining decision |
|---|---|
| A: FluidMonsters collision work | Call-volume diagnostics implemented; final paired costs incomplete. Medium complexity; fractional movement and overlap escape must remain exact. |
| B: BLOCKMAP/collision traversal | Strongest completed classic simulation evidence. High callback volume; filtering risks callback order, early exits and collision behavior. |
| C: Solid support cache | Hit/miss, invalidation, capacity and clear counts implemented; final sustained comparison incomplete. Capacity changes require stale-result and moving-sector tests. |
| D: Exact visibility | Solid sight is a focused follow-up candidate, not a validated winner. Inspect `platform/solid_runtime.cpp:SV_Sight` and `map_world.cpp:MapWorld::ray`; exact occlusion is a substantial correctness constraint. |
| E: Renderer | Scene, sprite lighting, shadow, upload/encoding/submission probes built; final native comparison incomplete. No CPU/GPU bottleneck ranking claimed. |
| F: Enhanced target searches | Tactics movement, candidates and search-specific LOS attribution implemented; final optional matrix incomplete. Retain target order and RNG progression. |

There is insufficient final evidence to recommend one optimization patch over
all six candidates. If work resumes, use one bounded, targeted comparison of
the suspected expensive path before proposing a Phase 3 patch. Avoid another
broad testing expansion unless that comparison reveals a concrete problem.

## Build and use

Configure the existing native CMake project with:

    -DDOOM_MONSTER_PROFILING=ON -DDOOM_MONSTER_BENCHMARK=ON

Build doom-headless for simulation-only measurements and doom-monster-bench
for native GPU replay. Build a second directory with profiling OFF and the
benchmark driver ON for the compiled-out comparison.

Ordinary engine builds can compile the profiler without the benchmark driver.
Pass -profilemonsters <directory> to enable recording. Without that argument,
there are no profiler timers, allocations, population scans, or file writes.
Compiling with DOOM_MONSTER_PROFILING=OFF removes the probes.

The runner is duct-tape-doom-main/tests/monster_profile_runs.py. Supply
--baseline, --instrumented, --iwad, --wad and --out. Defaults are five repeats,
350 warmup tics, 2100 measured tics, the five supplied maps, and idle/chase/combat
scenarios. --verify hashes state after each tic, outside the timing window.
--native adds native window/graphics settings; --fenced requests the existing
GPU-fenced profiler and must be reported separately.

Use --presets fluid,solid,tactics,feeding,frenzy,recovery to isolate extensions.
The classic preset disables optional AI, including default-on stuck recovery.
The recovery preset retains its original address-dependent behavior.

## Measurement contract

- Timings are monotonic elapsed nanoseconds around CPU stages, not hardware
  cycle counts or process CPU accounting. Scheduling and synchronous waits
  can contribute to elapsed stage time.
- scopes.csv contains per-tic main-thread calls and inclusive/exclusive time.
  Nested instrumented descendants are subtracted once from exclusive time.
  Uninstrumented descendants remain in their nearest measured parent.
- threads.csv separates accumulated main/worker scopes; worker durations must
  not be added to main elapsed time as if they were serial work.
- counters.csv records visits/callbacks, rejects, failures and support-cache
  behavior. support_peak_entries is a maximum gauge, not an additive count.
- actor_types.csv records inclusive/exclusive mobj time by internal type.
- population.csv records one post-tic population traversal and its own cost.
  Benchmark population collection and state hashing are outside timed gameplay.
- calibration.txt records empty-scope/timer costs. These are calibration
  estimates, not corrections subtracted from production measurements.
- Buffers retain at most 8192 tic samples. status.txt reports dropped samples
  and scope-stack errors. Full windows rotate into window-N subdirectories;
  failed writes are nonfatal and reported. There is no per-monster console
  logging. Rotating files include main-thread totals only; worker totals are
  read after renderer shutdown. Thread/type totals are cumulative within the
  measurement generation: do not add repeated snapshots together.
- Profiler controls use atomic access. Workers reset their own records when
  entering a new measurement generation. Jobs crossing its start boundary
  are excluded rather than assigning their pre-window work to the new window.
- gpu_upload, gpu_encode and gpu_submit separate native command stages.
  Submission includes synchronous waits when the fenced profiler is enabled.
  frame_interval_ns measures successive render-call starts, including the
  replay's simulation and observer work; frame_ns measures the render call.
  Neither is a hardware display-present timestamp.

Population eligibility is a live mobj thinker, non-player, positive health,
shootable, and COUNTKILL or Lost Soul. Unknown extended actor types are counted
separately. Nearest living-player horizontal distance defaults to
512/1024/2048 map units; override with -profile-distances a,b,c.
With no living player, eligible actors fall in the far bin.
The `patched` column identifies extended internal actor type IDs and stock
types mentioned by DeHackEd Thing patches. Frame-only replacement behavior
is not attributed back to every affected stock type. The supplied WAD's
embedded DeHackEd lump changes map-title strings only.

Hypothetical population hysteresis is ON at 1500 and OFF below 1200. The workload
comparison additionally requires 128 moving monsters or 1500 position queries.
Override using -profile-on, -profile-off, -profile-moving and -profile-queries.
These diagnostics never gate gameplay. Movement is observed displacement
between population samples; the first sample has no prior displacement.

## Reproducibility and limitations

The benchmark gives the player god mode, plasma equipment/ammunition, and a
fixed repeating movement/turn/fire sequence. Chase/combat setup explicitly
targets and wakes living monsters. These are disclosed synthetic stress
conditions, not an assertion that normal play awakens the entire map.
Waking every monster into its see state also synchronizes state-entry work;
its periodic spikes are part of this synthetic stress case.
Feeding lowers eligible predators to 20% health. Frenzy supplies up to 32
synthetic confirmed SSG head impacts every 175 measured tics through the
existing damage/discharge interface. Setup events happen outside timed tics.

Benchmark-only RNG call-site wrappers count existing random calls and retain
their result sequence. The RNG implementation is unchanged. These wrappers
are present in all compared benchmark binaries; their common observer cost
is not a measurement of the ordinary shipping executable.

Canonical comparisons include actor order/state/timers/physics, target and
tracer indices, players, sector state, common movers, Boom state, the weak-point
save trailer, and read-only private tactics, recovery/arena, FluidMonsters and
combat records. Actor references use stable indices. They exclude pointers,
padding and cosmetic RNG. Unknown/light thinker internals remain a coverage
limit. Earlier diagnostic passes used a narrower hash and are identified as
such in the evidence. The final verification uses the expanded hash.

Initial validation, before explicitly disabling recovery in classic mode,
found two MAP03 chase state mismatches at measured tic 514 with matching
gameplay RNG traces. Default-on recovery uses actor-address hashes and
address-selected escape direction. Preserve these preliminary failures;
they are not a clean compatibility certification.

The --lifecycle fixture tests save/load, same-map reconstruction, actor
spawn/death/removal, synthetic resurrection and death/Use recovery. Synthetic
resurrection does not replace a live Arch-vile action test.
The -bench-demo-trace option records canonical tic traces for actual demos.
The original recorder terminates completed recordings through I_Error;
the test runner must identify the completed-recording message and output.

The native replay advances one gameplay tic per render call with interpolation
fixed at the current tic. It is an unpaced replay, not the ordinary loop's
variable number of tics per displayed frame. Normal measurements force FIFO
presentation at 960×540; GPU-fenced runs use the existing profiler's separate
presentation path and are never merged with those frame intervals. Texture
lighting and weather are disabled in the recorded benchmark graphics config.

The ordinary `doom.exe` startup smoke uses absolute config/output paths. That
startup still creates its standard SDL preferences folder; the verified fresh
folder and its smoke files were moved into `build/playable-smoke/first-relative-startup`.
The dedicated benchmark and regression drivers keep their working directories,
saves and settings inside the experiment.

## Rollback

Remove -profilemonsters for runtime disable, or configure
DOOM_MONSTER_PROFILING=OFF to compile probes out. Set
DOOM_MONSTER_BENCHMARK=OFF for ordinary builds. The original checkout remains
available; saves require no conversion. No Phase 3 optimization is implemented.

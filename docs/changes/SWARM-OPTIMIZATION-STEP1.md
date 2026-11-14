---
build: 2026.11.12.1-experimental
updated_utc: 2026-11-13T11:41:47Z
status: experimental-step-1-not-approved-for-main
index: ../README.md
proposal: ../proposals/MONSTER-STREAMING-AND-SWARM.md
---

# Swarm optimization: conservative idle-search reuse

Follow-up: [MAP01 switch/conveyor activation fix](SWARM-ACTIVATION-FIX.md)
addresses the first manual test's failed monster-release chain. Timing figures
below describe the earlier build and have not been remeasured after that fix.

This development slice stays on `experiment/monster-profiling-phase2` in
`F:/Programming/C/HELLFORGE-profile-phase2`. Main and release artifacts are unchanged.
The actual starting commit is `b59ad46c1d5dcc92874074b3a2805a9b5af4b521`,
which already includes death-save recovery. It differs from the proposal's older
baseline. Existing uncommitted profiling work was retained, with its source archive,
patch and status saved under `build/swarm-step1/before*` before this slice.

## Implemented behavior

F4 Gameplay adds **Idle monster hibernation** and **Swarm diagnostics**, both off
by default. Config keys are `monster_hibernation` and `swarm_debug`.
The first option reuses a completed, negative idle player-search result only when
its inputs remain unchanged. It does not skip thinkers, state actions, physics,
collision, damage, sound propagation or ordinary spawning. Active chase/attack AI
is unchanged. This is a narrow first phase, not full monster streaming.

`linuxdoom-1.10/p_swarm.c` keeps 8,192 fixed direct-mapped records (about 1.2 MiB),
with no hot-path allocations. Keys include actor/player identities, positions,
flags, heights, health, sectors, actor angle and search cursor. A cursor-only
failure is never cached. Sound targets, motion, existing targets and uncertain
states force the original search. Sector changes invalidate globally; removal,
spawn reuse, map initialization, save restoration and option changes invalidate
affected records. No actor layout or save format changes are introduced.

The cache is restricted to unmodified single-player classic geometry and one
living player. Network/demo modes, Arcade, respawning monsters and DEH actor,
frame or code-pointer changes bypass it. Solid/digging modes are excluded.
**The ordinary executable enables digging by default: pass `-nodigging` to test
this slice.** Its absence of benefit in excluded modes is intentional.
Diagnostics report aggregate active/dormant/awake counts and search reuse once
per second; they do not apply budgets or change activation policy.

## Targeted validation

Release builds completed for the headless benchmark, native engine and settings
harness. `doom-swarm-idle-tests` passed all 20 guards: cursor handling, unchanged
reuse, movement, angle, sound, target, damage, geometry, removal, off/on, reload,
network/demo/respawn/solid exclusions and modified gameplay definitions.

Three separate settings processes verified defaults, writing and reloading the
two F4 settings. The harness reported no supported SDL GPU backend; settings
checks passed, but GPU rendering and interactive F4 appearance were not verified.

On Doom II plus `dmnsns2k-monsters.wad` MAP01, hard skill, paired off/on runs
matched canonical state and gameplay RNG calls, trace and index after every tic
for idle, sustained combat and lifecycle scenarios. Each used 70 warmup and 350
measured tics. Lifecycle exercises existing save/load and death/Use recovery
plus the benchmark's respawn fixture. Benchmark setup, fixed zone address and
commands are preserved with raw CSV and resource/executable hashes in
`build/swarm-step1/final-map01/`. The combat preset deliberately alerts monsters;
these scenarios are controlled experiments rather than ordinary play recordings.

Separate timing runs used three alternating off/on pairs, 70 warmup and 700
measured tics per run, no state hashing in the timed region. Probes were compiled
out; optional combat and enhanced AI were disabled. These compare the same
revision's option off versus on, not an untouched historical executable.

| Pair | Off median ms/tic | On median ms/tic | Off / on mean | Off / on p95 | Off / on p99 |
|---|---:|---:|---:|---:|---:|
| 1 | 1.1130 | 1.0574 | 1.1352 / 1.0960 | 1.3258 / 1.3415 | 1.4825 / 1.6058 |
| 2 | 1.1022 | 1.0509 | 1.1223 / 1.1049 | 1.3136 / 1.4144 | 1.4558 / 1.8242 |
| 3 | 1.1105 | 1.0642 | 1.1338 / 1.0900 | 1.3418 / 1.2667 | 1.5154 / 1.4279 |

Median paired reduction in median idle tic cost: **4.65%**. Each enabled timing
run reused 56,794 of 265,936 idle searches (21.36%). Tail timings are mixed,
including regressions, so this is evidence of a modest idle-only opportunity,
not a guaranteed overall improvement. No active-swarm or FPS improvement is
claimed. The workload has 3,821 hard-skill monster placements. Type 32000 is
absent from supplied MAP01; the separate WAD mentioning it was not available.

The complete old regression matrix was not rerun. Teleport/boss/resurrection
edge maps, broad replacement DEH content, ordinary-map disabled-path overhead,
and real GPU/manual gameplay remain migration gates. Matching these targeted
traces is not exhaustive compatibility approval.

## Staged work and migration boundary

Deferred spawning and distant active-AI scheduling are **not implemented**.
Audit points include `P_SpawnMapThing`, thinker ordering, BLOCKMAP interactions,
telefrags, boss scans, resurrection, spawn-time RNG, kill totals and save loader
boundaries. Removing live actors without a complete pending registry would alter
these behaviors. A later deferred-spawn slice needs explicit activation before
interactions, stable pending identities and a versioned persistence design before
it can be enabled. No placeholder switches advertise these capabilities.

Keep this branch separate until interactive testing, affected compatibility
fixtures and normal-map overhead checks pass. Revisit support for destructible
geometry separately. Do not merge or publish these artifacts as a stable release.

## Build, test and rollback

Reuse the existing CMake configurations. Build `doom`, `doom-headless` and
`doom-swarm-idle-tests` in Release. Run only the new guard target and
`tests/swarm_compare.py --help` for the bounded MAP01 driver arguments.
Native experimental files and symbols are staged in `build/swarm-step1/engine/`.
`Test-MAP01.ps1` starts an isolated-config manual run with `-nodigging`; user
settings in that test directory are preserved on subsequent launches.

Disable both F4 switches to stop reuse and diagnostics immediately. Cache state
is transient, so no save conversion is needed. Return to the untouched main
checkout for the release build. The pre-slice archive preserves the prior
experimental modifications; do not reset the whole worktree to discard this slice.

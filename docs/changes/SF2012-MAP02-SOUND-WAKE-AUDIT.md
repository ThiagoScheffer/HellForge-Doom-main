---
build: 2026.11.12.1-experimental
updated_utc: 2026-11-13T17:53:46Z
status: investigation-complete-no-gameplay-change
index: ../README.md
tests: ../../duct-tape-doom-main/tests/monster_bench.cpp
---

# MAP02: one-shot monster wake investigation

Follow-up: [actual idle-to-see transitions and narrow ambush sight reuse](AMBUSH-SIGHT-REUSE.md)
distinguishes the 436 target-bearing actors from the 165 actual first transitions.
Its measured duplicate-query reuse supersedes the speculative REJECT suggestion below.

## Reproduction

Only `SF2012_final.wad` MAP02 was tested, with the correct Doom II IWAD,
hard skill, `-solidmaps -uzmovement`, 960×540 FIFO presentation, sprite filtering
enabled and optional enhanced AI/FluidMonsters disabled to isolate normal wake
behavior. The player remains at spawn, with god mode for repeatability.

Two final native runs used 35 warmup + 175 measured tics: no shot versus one
pistol shot at the first measured tic. Normal gameplay tic commands fired the
weapon; ammunition changed from 100 to 99 in the shot case and stayed 100 in
the control. No targets or monster states were assigned by setup.

| Measurement | Idle | One pistol shot |
|---|---:|---:|
| Initial living monsters | 457 | 457 |
| Target-bearing monsters before measurement | 30 | 30 |
| Target-bearing monsters after 35 measured tics | 30 | 436 |
| Target-bearing monsters over 2,048 units away at that point | 0 | 238 |
| Sectors marked with a sound target | 0 | 407 |
| Median game-tic CPU time | 6.5072 ms | 7.6043 ms |
| p95 game-tic CPU time | 9.0002 ms | 10.9272 ms |
| Median native frame-call time | 5.0616 ms | 5.6718 ms |
| p95 native frame-call time | 6.9912 ms | 9.1013 ms |

These are one short profiled pair, not a general performance estimate or FPS
claim. The first test fixture incorrectly left a pending weapon switch; another
fixture check overlooked the normal ammo cap. Those attempts were discarded;
the final fixture fixes both and requires exactly one round consumed.

## Cause

`P_FireWeapon` calls `P_NoiseAlert`, which invokes `P_RecursiveSound` in
`linuxdoom-1.10/p_enemy.c`. It floods sectors through open two-sided lines.
Closed openings stop it; the first sound-block boundary can be crossed and a
second stops propagation. It has no physical-distance limit. Sector sound targets
persist, so an actor can acquire the target on a later `A_Look` action.

`A_Look` accepts that sound target without LOS for ordinary monsters. Ambush
monsters require LOS. Disconnected pieces can also share a sector, so treating
sector membership as physical proximity would be incorrect.

This is existing Doom-style activation, not caused by sprite frustum filtering.
Many maps rely on remote holding monsters waking, walking onto teleport lines
and joining later fights. Distance alone cannot tell an unnecessary actor from
an intentionally staged trap. This audit does not assert that every distant
MAP02 monster belongs to such a trap.

## Where CPU time goes

Main-thread **exclusive** profiler totals over the 175 measured simulation tics:

| Operation | Idle | One shot |
|---|---:|---:|
| Solid-world sight | 927.831 ms / 14,925 queries | 1,084.284 ms / 15,533 queries |
| Solid support | 106.129 ms / 138,646 queries | 177.409 ms / 184,267 queries |
| Position checks | see raw scopes | 49,719 calls |

Solid sight consumes about 80% of measured simulation scope time at idle and
76% after firing. Position checks rise from 3,954 to 49,719. Counts increase
because awakened actors perform their normal chase/movement work. The sound
flood itself was not separately timed; the first shot tic was 7.8092 ms, while
increased work persists through the following tics. Renderer scope totals are
separate and must not be added to the simulation percentages.

The solid path in `platform/solid_runtime.cpp::SV_Sight` traces up to three rays
per query. `p_sight.c::P_CheckSight` invokes it before the classic REJECT/BSP path.
The existing idle-search optimization intentionally excludes solid geometry and
target-bearing actors, so it cannot remove this workload.

## Recommended next change

Prioritize **exact solid-world visibility cost**, preserving every wake, target,
state transition, RNG call and teleport trigger. Start by assessing a cheap
negative visibility rejection before expensive solid rays. A candidate is the
original REJECT data only where it is demonstrably conservative for the current
solid world and both actor placements. Excavation, restored cuts, unusual
geometry, sector motion or uncertain/custom REJECT data must fall back to the
existing ray path. Do not enable that shortcut merely because a map has a
REJECT lump. No gain or correctness guarantee is claimed before comparison.

If this cannot be proved conservative, optimize the exact solid ray traversal
instead. Support-query reuse is a secondary candidate. Both require a separate
implementation and focused equivalence checks including excavated walls and
moving sectors. This audit implements neither shortcut nor LOS caching.

A hearing-distance option could reduce activation more directly, but it is a
gameplay variant: it can break remote teleport traps and delayed encounters.
It should never silently replace normal wake behavior or be enabled for all
WADs as a compatibility optimization. Similarly, do not freeze distant chase
actors or reset their targets based on camera visibility.

## Changes, reproduction and limits

Only the benchmark driver changed: it adds the `shotwake` scenario and optional
`-bench-wake-audit`. Snapshots scan actors once at selected tics, outside timed
simulation/rendering; they perform no visibility query, RNG call or actor write.
The `moving` column indicates momentum or positive movement count, not proof
that an actor changed position. No normal engine, launcher, F4, save format or
gameplay behavior was changed; the tested Compatibility EXE remains current.

Build `doom-monster-bench` in the existing `build/native-on` configuration, then
run `tests/sound_wake.py` from the imported engine subtree, or use the exact
commands recorded in `build/sf2012-wake/results.json`. Raw scopes, populations,
timings and logs are beside that file. The profiled pair is deliberately small;
optional combat presets and a full MAP02 playthrough were not tested.

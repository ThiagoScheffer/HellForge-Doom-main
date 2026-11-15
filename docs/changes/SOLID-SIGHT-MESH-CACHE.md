---
build: "2026.11.15.6 (local native source rebuild)"
updated_utc: "2026-11-15T18:33:13.668506+00:00"
status: "implemented; focused contracts and 4000 Monsters spawn measurements passed"
index: "../README.md"
tests: "../../ray_mesh_tests.cpp"
related: "LARGE-MAP-IDLE-SIGHT-REVIEW.md"
---

# Exact solid-world sight acceleration

Thousands of idle monsters were rebuilding and traversing voxel meshes through
`P_CheckSight -> SV_Sight -> SV_Trace -> MapWorld::ray`. The ordinary 2,048-volume
cache could not retain the map's sight working set. Earlier idle-search reuse
bypasses solid geometry, while sprite filtering only affects rendering.

## Implementation

Only map-mode monster sight opts into the new ray mesh path. Other `SV_Trace`
callers retain the existing path. Classic geometry, multiplayer and demos retain
their existing solid-mode exclusions. No actors are frozen, skipped or deferred;
hearing, monster actions, movement, RNG calls and save layouts are unchanged.

- Maintain a separate bounded sight mesh cache: at most **16,384 tiles** and
  **64 MiB of accounted storage**, including conservative container overhead.
  Temporary mesh construction and the existing full-volume cache are separate.
- Store triangle vertices/normals using lossless float-bit indexing. Preserve
  original triangle IDs and exact intersection arithmetic, including hit-distance
  ties. Oversized/unrepresentable meshes fall back to the original ray path.
- Use conservative spatial bounds to avoid irrelevant triangle tests. Bounds
  include barycentric tolerance and outward float rounding. This caches geometry,
  not monster/player visibility answers; movement always runs a fresh query.
- Use hashed tile lookup and amortized approximate-LRU bookkeeping without
  allocating on warm hits. Eviction changes only what must be rebuilt.
- Edits and crumble invalidate affected tiles. Door/lift height changes invalidate
  the sector's XY columns plus influenced anchored-cut columns, including old/new
  Z positions. Distant meshes stay warm. Reset and successful save restoration
  discard all sight meshes. Recreating an unchanged collision volume does not
  invalidate its sight copy. No pointers or acceleration structures are saved.

`-nosolidsightcache` restores the old sight ray path for comparison.
`-profilesolidraycache` prints aggregated hits, misses, evictions, tiles and bytes
for the first tics and then once per second. Neither needs a new F4 preference.

## Focused results

4000 Monsters MAP01, Doom II, skill 3, no movement/fire; isolated current profile,
solid geometry enabled, Vulkan/FIFO, 1536x960 internal resolution. New projectile
FX were disabled in the copied profile. Existing user source edits were preserved.

The prior no-input diagnostic reported an **8,208 ms gameplay batch** and 1,474
solid rays above 2 ms during the first ten tics. Those were diagnostic attribution
measurements, not a sustained FPS baseline.

With the accelerated path, 140 warmed native frames (tics 40..121) measured:

| Metric | Result |
|---|---:|
| Median application frame interval | 22.1635 ms |
| p95 | 26.798 ms |
| p99 / maximum | 27.759 / 30.079 ms |
| Stable sight tiles | 13,944 |
| Accounted sight storage | 61,612,368 bytes (about 58.8 MiB) |
| Sight mesh evictions | 0 |

These are one short stationary native capture with diagnostic overhead, not a
60-FPS guarantee, paired long-run benchmark, or combat/level-completion result.
The capture preceded the final localized height-invalidation refinement; its idle
query path is unchanged. The final refinement is covered by door/anchored-cut
contracts. Cold mesh filling, newly visited regions and heavily changing worlds
still cost work. No normal-map performance claim is made without measurement.

## Verification

All four focused CTest contracts passed:

1. `ray-mesh-contract`: exact hit/miss, distance, point and normal comparisons;
   1,024 sloped-surface rays including large coordinates; changed observer/target
   positions; near boundaries; destruction; closing/reopening sectors; retaining
   remote meshes during motion; moving anchored cuts; reset/load; crumble;
   collision-cache turnover; real 16,384-tile LRU eviction and budget checks.
2. `map-world-contract`: existing geometry, destruction, moving-sector anchors,
   save/load, malformed-save handling, protected floors and support regressions.
3. `boom-conveyor-contacts`: existing conveyor/voodoo contact regression.
4. `sight-missing-sector`: existing malformed-sidedef sight safeguards.

The real WAD also completed 35 headless simulation tics and saved successfully
with acceleration enabled and disabled. Saved player data, world data and **all
4,570 actors' scalar/state/target/tracer/player-index fields match**, after clearing
only documented loader-rebuilt native pointer fields. Raw saves differ because
of native pointers; 154 differing bytes remain beyond the compared actor section,
so this is not claimed as full-file equality. Special-thinker/runtime trailers and
full RNG call traces were not canonically compared. No gameplay RNG calls were
introduced by the patch. The short headless runs are correctness checks, not an
FPS benchmark. The final enabled run took about 8.1 seconds including startup and
cold mesh creation; the disabled run took 61.5 seconds under diagnostic conditions.

No full suite, launcher build, distribution packaging, LAN/demo matrix, dmnsns2k
playthrough or extensive gameplay test was run. Broader live combat and trap
progression remain manual coverage. Geometry is shared across WADs; this is not
a map-specific exception or a change to monster activation policy.

## Rebuild and try it

Build the existing native `doom` target. Matching binaries are
`build/Release/doom.exe` and `doom.pdb`; the native version label remains
2026.11.15.6 for this local source revision. The existing packaged launcher still
embeds its earlier engine; it was not rebuilt.

Double-click `build/large-map-fix/Test-Solid-Sight-Fix.cmd` for the rebuilt engine
and supplied 4000 Monsters MAP01. It uses isolated copied graphics/controls.
An optional WAD path argument selects another Doom II PWAD. Solid destruction
remains enabled. No user profile, release EXE or dist file was changed.

Reproduce the fast contracts from the repository root:

```powershell
cmake -S . -B build/large-map-fix/tools -A x64
cmake --build build/large-map-fix/tools --config Release --target ray-mesh-tests map-world-tests
ctest --test-dir build/large-map-fix/tools -C Release -R 'ray-mesh-contract|map-world-contract' --output-on-failure
ctest --test-dir build -C Release -R 'boom-conveyor-contacts|sight-missing-sector' --output-on-failure
```

Local commands, raw frame logs, native samples, save comparisons, baseline EXE/PDB
and resource/final-binary hashes are under `build/large-map-fix/`. See
`native-summary.json`, `actor-comparison.json`, `compare-saves.py`, and
`manifest.json`. Disabling the optimization requires only `-nosolidsightcache`;
there is no save conversion or setting migration.

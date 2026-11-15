---
updated_utc: "2026-11-15T17:56:17.954862+00:00"
status: "idle stall reproduced; solid-ray bottleneck identified; no engine changes"
index: "../README.md"
related: "4000-MONSTERS-QUICK-DIAGNOSTIC.md"
---

# Large-map slowdown immediately after spawn

## What the previous patches actually fixed

The optimizations migrated in commit `64809ed` remain in current source:

- **Idle monster hibernation** (`p_swarm.c`): reuses unchanged negative idle
  searches, preserving thinkers, physics and triggers. It explicitly bypasses
  `-solidmaps`, digging, demos, multiplayer and patched definitions. It therefore
  cannot help the ordinary launcher's solid-world mode, even when F4 enables it.
- **Ambush sight reuse** (`p_enemy.c`): reuses only the immediately preceding
  failed sound-target sight query within one A_Look invocation. No sound target
  at spawn means this optimization cannot remove the ordinary initial search.
- **Conservative sprite filtering** (`scene3d/geometry.cpp`): reduces safely
  offscreen sprite lighting/drawing, not simulation sight. Reflections, flashlight
  silhouettes, voxels and uncertain presentation states have conservative bypasses.
- **Conveyor fixes**: corrected voodoo-doll friction and footprint contacts,
  restoring dmnsns2k/Abandon switch chains. These are progression fixes.
- The later **large-platform shield** (`a252bb0`) prevents destruction on recognized
  large moving floors. It addresses moving-floor rebuild lag, not idle LOS.

See [idle reuse](SWARM-OPTIMIZATION-STEP1.md),
[ambush reuse](AMBUSH-SIGHT-REUSE.md),
[sprite filtering](SPRITE-FRUSTUM-FILTERING.md) and
[solid-mode restoration](SF2012-SIGHT-AND-DESTRUCTION-FIX.md).
The earlier experimental launcher forced classic geometry; destruction restoration
changed normal launches back to solid mode. Earlier classic-mode performance did
not establish solid-mode performance. No removed optimization was found in this
review, and no recent FX change was established as the cause.

## New reproduction: no shooting required

Source inspected at `274fa93533d61037a2cfe74c61a1b4bf6b4a91f1`. Existing native executable reports build
2026.11.15.6. Used supplied `4000 Monsters.wad` MAP01, Doom II IWAD, skill 3,
`-solidmaps -uzmovement`, DiggingMaterials and an isolated copy of the current
user graphics profile. No movement, targeting, firing or monster setup was sent.
`-profilegamesteps -profilespikes` enabled existing diagnostics; no rebuild.

The profile has hibernation, ambush reuse and sprite filtering enabled.
`mod_fluid_monsters` is **off**: the new enhanced projectile FX are ineligible.

| Observation | Result |
|---|---|
| Slow solid traces logged during tics 0 through partial tic 9 | 1,474 |
| Sum of those trace durations | 12,737.628 ms |
| Longest logged individual trace | 90.423 ms |
| Gameplay batch reported at tic 5 | 8,208 ms |
| First submitted frame spike at tic 5 | 23.972 ms total; 5.761 ms preparation |
| Renderer loading stages | 1,557 ms |

Trace logging includes only rays exceeding 2 ms. Its sum is a lower bound on
logged ray time, not a complete query count or a statistical game-tic average.
Per-actor timings include ray timings: do not sum both. Diagnostic logging and
three live process snapshots add overhead; these numbers are attribution evidence,
not production FPS measurements. The explicit no-input reproduction identifies
simulation work far larger than first-frame rendering or baking.

The earlier short live comparison in
[the quick diagnostic](4000-MONSTERS-QUICK-DIAGNOSTIC.md) measured a ~7,986 ms
solid-mode interval versus 9.85 ms median with `-nodigging` and active monsters.
This review's separate no-input classic launch captured only startup before its
time limit; it is **inconclusive**, not a new successful gameplay comparison.
No new dmnsns2k gameplay benchmark or full regression suite was run.

## Cause and remaining boundary

Idle monsters still search for the player. In `p_sight.c`, `P_CheckSight` calls
`SV_Sight` before classic REJECT/BSP processing. `SV_Sight` tries up to three
solid-world rays for different target heights. It does this on intact maps too.

`SV_Trace` -> `MapWorld::ray` traverses 64-unit voxel tiles. A missing tile invokes
`MapWorld::tile` -> `update`: sample sector geometry, allocate volume data and
`Volume::rebuild` triangle meshes. `Volume::ray` then intersects those triangles.
These meshes are required by the current ray implementation, so simply omitting
rebuild is incorrect. Thousands of idle monsters distribute queries over a large
map before the player shoots. Repeated slow rays are directly measured here.

Code also has a 2,048-tile cache with a linear eviction search, plus a horizontal
sample cache that clears at capacity. Cache churn is a plausible contributor,
**not measured in this run**. The exact split between tile creation, eviction and
triangle traversal remains unmeasured; avoid claiming one is proved dominant.
This is a shared engine scalability path, not a proven malformed-WAD problem.

## Next focused fix boundary

Optimize exact solid-world visibility/tile work, preserving destructible openings,
doors, sector movement and the current visibility result. First record aggregated
tile hit/miss/eviction/rebuild time on this same no-input scene, then choose the
smallest measured change. Avoid blindly increasing cache memory or falling back
to vanilla BSP on every intact map: equivalence has not been established.
No wake-radius cap, frozen monsters, cross-tic stale LOS cache or simulation timing
change is justified by these findings.

Classic geometry (`-nodigging`, without `-solidmaps`) remains the diagnostic
workaround and disables solid destruction. The normal release launcher's current
argument builder forces `-solidmaps`; the historical `--classic-geometry` option
is conditional on an experimental launcher build. Merely turning off a wall FX
setting does not remove this sight path.

Only this report was added. Existing projectile source edits were preserved;
no gameplay code, profile, release binary or launcher was changed. Local commands,
logs, settings copy, resource/binary hashes and diagnostic snapshots are in
`build/large-map-review/`. Snapshot dumps are intentional samples, not crashes.

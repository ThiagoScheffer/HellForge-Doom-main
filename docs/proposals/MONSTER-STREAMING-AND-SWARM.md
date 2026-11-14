# DOOM: HELLFORGE — Monster Streaming and Swarm Performance Optimization

## Project context

- Project: DOOM: HELLFORGE, forked from Tuin-Doom-Enhanced / DERP-Duct-Tape DOOM.
- Branch: `master`.
- Reference HEAD: `2423b448da3c90bdd2ad9f2fc0ea63b4541d7599`.
- The working tree has uncommitted changes, including death-save recovery and headless-backend work.
- Engine: C11/C++17, CMake, SDL3 accelerated renderer, Windows x64 MSVC packaging, C# WinForms launcher, plus separate software/headless targets.
- Supports classic Doom, implemented Boom features, and DeHackEd/BEX. Do not assume MBF21 support or a PrBoom complevel system.

**Important:** Preserve all existing uncommitted work. Do not reset, clean, overwrite, or discard unrelated changes. Inspect the actual working tree before making changes. Do not assume `swarm_ai_patch.py` or `SWARM_TEST_README.md` exists.

## Primary objective

Design and progressively implement an optional monster optimization system that improves performance in large WADs containing thousands of monsters.

The solution should prevent unnecessary simultaneous monster processing while preserving gameplay wherever possible.

Implement three independently testable capabilities:

1. Safe Monster Hibernation
2. Sector-Based Deferred Monster Spawning
3. Budgeted Distant-Monster AI Scheduling

Do not implement these as one inseparable patch.

## Reference benchmark

Use `4000 Monsters.wad or dmnsns2k-monsters.wad  only map01` if available in the test environment.

The provided 4000 Monsters.wad WAD's MAP01 has been independently inspected and contains:

- 4,589 THINGS
- 4,317 recognized classic Doom monster placements
- 483 sectors
- 2,386 linedefs
- 56 sound-blocking linedefs
- 18 monster-only teleport linedefs (special 125)

The file also contains 15 placements using thing type 32000, whose meaning must be resolved against the actual engine's definitions.

Do not assume this uploaded WAD automatically exists in the local repository.

## PHASE 1 — Audit and instrumentation

Inspect the actual engine implementations responsible for:

- THINGS loading and map-thing spawning
- Monster thinker execution
- Monster wake-up and sound propagation
- Sight checks and collision handling
- Sector and linedef activation
- Sprite rendering and sprite collection
- Savegame serialization and restoration

Identify the real functions and call sites in the modified source rather than blindly relying on upstream Doom.

Implement low-overhead instrumentation tracking:

- Frame time and simulation tick time
- AI/thinker processing time
- Rendering time
- Line-of-sight calculations
- Awake, dormant, active, and pending monster counts
- Monsters awakened by gunfire
- Monsters spawned per tick
- Monster-related collision workload

Provide benchmark output with average and percentile timings.

Establish baseline measurements before optimizing.

## PHASE 2 — Safe Monster Hibernation

Implement an optional low-overhead idle state for monsters that are confirmed not to require immediate simulation.

Requirements:

- Preserve real monster objects.
- Preserve health, position, momentum, state, and targeting.
- Preserve interactions with damage, collision, and map machinery.
- Wake immediately when gameplay requires it.
- Avoid expensive AI polling when a monster is safely dormant.
- Never freeze combat-relevant actors, projectiles, or moving monsters.
- Do not skip the whole monster thinker indiscriminately.

If an actor cannot be proven safe to hibernate, continue normal simulation.

## PHASE 3 — Deferred Monster Registry

Create a lightweight per-level registry for eligible unspawned monster placements.

Suggested fields:

- Original mapthing data
- Original THINGS index
- Resolved monster type
- Sector and spatial-cell location
- Spawn eligibility and difficulty flags
- Lifecycle state
- Activation reason
- Statistics-accounting state

Separate map-thing filtering, logical registration, and physical monster instantiation where necessary.

Preserve existing spawning logic for players, items, keys, decorations, and other special THINGS.

Defer only monster types whose behavior has been audited.

Preserve correct level kill totals without double counting.

Do not alter WAD files.

## PHASE 4 — Sector-Based Activation

Build an efficient activation manager using:

- Player proximity and movement
- Sector adjacency and portal connections
- Potential visibility
- Sound propagation
- Linedef and sector triggers
- Teleporter dependencies

Maintain indexed pending-monster lists rather than scanning every monster every tic.

Use a priority queue with immediate activation for gameplay-critical situations.

Implement configurable spawn budgets only for noncritical work.

Begin with experimental ordinary spawn budgets of 8–16 monsters per tic, then benchmark and tune.

Prevent visible pop-in, missing collision, missing enemies, and broken encounters.

Do not treat straight-line distance, camera FOV, or REJECT data as sufficient proof that a monster cannot interact with the player.

## PHASE 5 — Sound and AI Optimization

Audit `P_NoiseAlert`, sector sound propagation, monster alerting, and the relevant sight routines.

Investigate whether sound-sector propagation can be made cheaper while retaining existing sound-block behavior.

Avoid triggering redundant expensive visibility calculations for thousands of distant monsters.

Where behavior permits, distribute optional distant-monster AI decisions across multiple game tics.

Prioritize engaged, dangerous, nearby, moving, or potentially visible monsters.

Do not slow global gameplay timing or break state transitions.

An awake monster must never become permanently starved of simulation work.

## PHASE 6 — Gameplay Compatibility

Explicitly protect:

- Monster-only teleport closets
- Monster-triggered linedefs
- Doors, platforms, lifts, crushers, and moving sectors
- Boss-death actions and MAP07-style events
- DeHackEd/BEX-modified monsters
- Arch-vile resurrection and corpses
- Nightmare respawning
- Monster infighting
- Projectile and splash damage
- Telefragging and monster collision
- `-nomonsters` and difficulty filtering

Trigger activation before relevant gameplay events whenever required for correct behavior.

Do not despawn or recreate already-engaged monsters from their original placements.

## PHASE 7 — Savegame and Compatibility Rules

Keep the original behavior as the default.

Provide independent settings for:

- Monster hibernation
- Deferred spawning
- Distant AI scheduling
- Debug and performance counters

Existing saves must continue loading.

If deferred monsters require additional savegame state, implement versioned persistence compatible with the existing death-save recovery work.

Do not ship aggressive deferred spawning until save/load restoration has been validated.

Disable optimizations in demos, multiplayer, or other deterministic modes unless their behavior has been explicitly tested.

## PHASE 8 — Regression and Performance Testing

Compare optimization disabled versus enabled.

Test:

- Initial MAP01 load
- Standing at the player start
- Shooting a weapon and alerting monsters
- Opening doors and crossing trigger lines
- Monster teleport ambushes
- Large groups of simultaneously active monsters
- Save/load with pending and live monsters
- Death-save recovery
- Level completion and kill totals
- Original Doom and Doom II maps
- Boom-compatible WADs where supported

Test the headless simulation and available renderer configurations separately.

Ensure the Windows MSVC release build remains functional.

Report measurable performance changes, gameplay regressions, and remaining limitations.

## Engineering constraints

- Prefer small, isolated modules over invasive rewrites.
- Reuse existing Doom infrastructure where practical.
- Avoid introducing unnecessary dependencies.
- Avoid allocations in hot paths where possible.
- Keep all feature paths optional.
- Preserve the original implementation when optimizations are disabled.
- Make changes incrementally and verify each phase.

## Required deliverables

1. Audit identifying actual bottlenecks and integration points.
2. Implementation of the safest viable optimization phases.
3. Configuration switches and useful diagnostics.
4. Automated and manual regression tests.
5. Before-and-after performance measurements.
6. Documentation of compatibility limitations and known behavior changes.

**Success criteria:** Significant measured reduction in CPU workload for high-monster maps, without crashes, disappearing enemies, broken teleporters, incorrect kill totals, corrupted savegames, or unacceptable changes to combat behavior.

Do not claim a performance improvement without benchmark evidence.
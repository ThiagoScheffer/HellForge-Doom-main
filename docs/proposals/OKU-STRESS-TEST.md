# HELLFORGE — Extreme Stress Test: oku2v31.wad

## New test target

Use `oku2v31.wad` MAP01 as the primary stress test for monster streaming, initial actor creation, AI scheduling, and rendering performance.

**User-reported failure:** After entering the map, FPS drops to approximately zero. The game becomes effectively unresponsive, and even the ESC menu cannot be opened.

This behavior has not yet been reproduced or profiled by Codex.

The previous MAP01 full playtest passed. Preserve the existing successful behavior and fixes.

## Verified WAD characteristics

- 27,700 THINGS.
- 23,241 recognized monster placements.
- 3,797 sectors.
- 24,023 linedefs.
- 15,968 subsectors.
- 27 Player 1 start placements.
- 245 monster-only teleporter linedefs with special 126.
- One monster-only teleporter linedef with special 125.
- No linedefs carrying the classic sound-block flag.

All identified monster placements enable the standard skill levels.

The map also uses generalized Boom-style linedef specials.

Do not assume monster streaming is automatically safe because monsters are far from the initial player position.

## Task 1 — Identify the exact freeze

Instrument the engine to identify whether the freeze is occurring in:

1. Level loading and initial monster instantiation.
2. Monster thinker and AI processing.
3. Collision and BLOCKMAP processing.
4. BSP traversal or visibility handling.
5. Sprite collection and rendering.
6. SDL event handling or frame scheduling.
7. Other unexpectedly expensive operations.

Record timing for map setup, the first simulation tic, subsequent tics, and rendered frames.

Log progress even if the engine becomes unresponsive.

Use a separate process with an external timeout for automated tests. Do not depend on ESC or the graphical interface to end a benchmark.

## Task 2 — Benchmark isolation

Compare the same map under these conditions:

- Normal monster spawning, normal graphics.
- No-monsters mode, if supported by the engine.
- Normal monster spawning, headless simulation.
- Current idle hibernation enabled.
- Experimental deferred spawning enabled.
- Deferred spawning combined with hibernation.

Keep other settings identical where possible.

Determine whether performance improves from fewer instantiated monsters, fewer active thinkers, or reduced rendering workload.

Report failed starts and timeouts rather than discarding them.

## Task 3 — Implement safe deferred spawning

Investigate avoiding full `mobj_t` creation for monster placements that can provably remain inactive until a future gameplay event.

Retain compact placement records.

Avoid constructing full monster thinkers and collision state until required.

Spawn pending monsters before they are needed by:

- Visibility or player interaction.
- Projectile or splash-damage interactions.
- Sound and monster activation.
- Doors and sector machinery.
- Teleporters.
- Monster-triggered linedefs.
- Voodoo-doll-controlled encounters.
- Boss and level-completion mechanics.

Do not defer uncertain or gameplay-critical monsters.

Never defer player start objects or existing voodoo-doll machinery.

Preserve the original actor creation path for unsupported situations.

## Task 4 — Keep ESC and menus responsive

Investigate why the input/event loop stops responding under extreme simulation load.

Ensure SDL events and menu input are serviced frequently enough during prolonged gameplay workloads.

Add an emergency pause or safe-recovery mechanism if the architecture supports one without compromising correctness.

Avoid unbounded catch-up loops that continually execute simulation tics without returning to event processing.

Do not silently drop game tics, change deterministic gameplay behavior, or introduce unsafe simulation/render threading.

If a single simulation tic blocks the main thread excessively, identify and break down the expensive operation safely rather than merely changing the event loop.

Also ensure startup profiling and failure logging work when the graphical window never becomes responsive.

## Task 5 — Compatibility safeguards

This WAD contains many monster teleporters and multiple Player 1 starts.

Preserve the previously fixed Boom voodoo-doll conveyor behavior.

Verify that deferring monsters does not prevent teleporter activation, scripted encounters, or monster-controlled triggers.

Audit spawn-order dependencies, RNG consumption, actor references, kill statistics, and save/load restoration.

Keep streaming disabled by default until these requirements are validated.

## Task 6 — Required performance report

Produce a comparison showing:

| Metric | Streaming OFF | Streaming ON |
|---|---|---|
| Map loading duration | Measure | Measure |
| Initial instantiated monster count | Measure | Measure |
| Pending monster count | 0 | Measure |
| Peak memory usage | Measure | Measure |
| First simulation tic duration | Measure | Measure |
| Median simulation tic time | Measure | Measure |
| P95/P99 simulation tic time | Measure | Measure |
| Average FPS | Measure | Measure |
| P95 frame time | Measure | Measure |
| ESC/menu responsiveness | Test | Test |
| Monster teleport activation | Test | Test |
| Save/load correctness | Test | Test |

Do not invent measurements. State explicitly when a configuration cannot complete a test.

## Success criteria

The optimized build should be able to enter and run this map without the observed indefinite startup freeze, or at minimum identify and quantify any remaining bottleneck.

Monster streaming should show a meaningful reduction in initial live actor count and memory usage when safe candidates exist.

FPS and simulation performance must be measured independently.

ESC/menu responsiveness should remain usable under severe load, subject to an explicit measurable input-latency target.

Do not sacrifice monster encounters, teleporters, savegames, or map progression to produce benchmark improvements.

Keep the existing working tree, full-MAP01 compatibility fixes, and release files intact. Develop on an isolated experimental branch or worktree.

Report results before proposing integration into `master`.
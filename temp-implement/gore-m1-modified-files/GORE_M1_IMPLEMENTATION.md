# HellForge — Advanced Gore M1 (source-only integration)

## Integration

This overlay is relative to the **Doom engine source directory** containing `platform/` and `linuxdoom-1.10/` (the directory where the Doom engine's `CMakeLists.txt` belongs). It is **not** relative to the parent geometry-tools CMake project.

Copy the two folders from the `gore-m1-modified-files.zip` archive over your working tree, preserving paths, after backing up or committing your current files. Alternatively, apply `gore-m1.patch` from that same engine directory with `git apply --check gore-m1.patch` followed by `git apply gore-m1.patch` (once only). **Do not use both the overlay and the patch on the same tree.** Do not replace either CMakeLists.txt; this milestone only modifies existing compiled source units.

## Implemented

1. **Two enemies:** Zombieman (`MT_POSSESSED`) and Shotgun Guy (`MT_SHOTGUY`) only, limited to stock first-death-frame artwork by existing hashed rigs (`POSSH0`, `SPOSH0`). Replacement WADs fall back rather than applying untrusted anatomical cuts.
2. **Two weapons:** shotgun and super-shotgun, from the existing `P_WeakpointBeginShot/EndShot` discharge identity. Only a *lethal hit* can produce M1 gore. It must be within **128 Doom units** of the shooter.
3. **Unmodified gameplay:** sprite impact recorded in `R_BloodHit` **before** damage, confirmed through `R_GoreHit` **after** damage; no change to Doom's hit tracing, attack accuracy, gameplay RNG, health, AI, or vanilla gib-state selection. Critical Parts is **not required**.
4. **Head destruction:** when at least one recorded pellet has a confirmed sprite-space head hit and head pellets account for **at least half** of captured hits, a validated stock death is flagged as headless. No intact head is spawned. Masks use `woundMask`/`maskedRow`/`bodyQuads`/`fallQuads` existing geometry and blood-rim palette. Frame masks are shared and cached per image, without allocating new GPU textures.
5. **Heavy impacts:** directional, gravity-driven sprays and 2-lobed flesh pixel fragments (up to **6 head / 3 torso**), with wall/floor/ceiling collision through the existing blood projectile and decal routines. Up to **3 extra wall marks**, using existing decal budgets. Max **64 live flesh fragments** globally, shared 768 blood-particle budget.
6. **F4 > Combat > Gore**: Advanced Gore (Classic / Enhanced), Head destruction, Flying flesh fragments, Enhanced wall blood. Values are saved to `graphics.cfg` by the existing F4 options registry.
7. **Gate:** `totalkills < 200`, accelerated renderer, player in single-player, not demo playback/recording, visible blood enabled, source actor using supported sprite (not voxel). Only stock death artwork triggers the effect. The gate is for map kill population, **not** count currently alive.
8. **Lifecycle:** gore state is renderer-local, wiped on level restart, forgotten when actors leave the watched set or are resurrected, and cleared on shutdown. Existing decomposition can still affect the corpse texture behind the missing geometry.

## Important prototype limitations

- **Runtime and visual quality have not been verified.** Exact masking of the lying/corpse artwork uses a conservative silhouette heuristic, not per-frame, manually approved masks; examine the full death sequence from front and side before shipping.
- Head removal is **not enabled with Death Ragdoll** on in M1 (to avoid a detached-head appearance when the ragdoll is rendered as an independent card). Heavy-impact sprays still work. Procedural Falls and ordinary sprite deaths use the existing mask path.
- For a *vanilla extreme-death / gib frame* the code does not force a secondary head removal; it preserves the vanilla gib state and adds controlled impact blood/fragments.
- Headless status is **runtime only**: it is not serialized in savegames, and will not survive saving/reloading the corpse.
- Flesh fragments are small colored pixel clusters, **not** independently persistent objects. Their landing impact persists as ordinary bounded blood decals; this is the intentional M1 asset-free approach.
- Very unusual replacement sprite assets or DeHackEd-modified death animations fall back to normal death when the hashed stock first-death frame does not match.
- No new sound or standalone severed-limb/organ artwork is included. Torso holes, legs and organs belong to later milestones.

## F4 setup for first test

- Renderer: Accelerated ON
- Blood: ON
- Advanced Gore: Enhanced
- Head destruction: ON
- Flying flesh fragments: ON
- Enhanced wall blood: ON
- Death Ragdoll: OFF for headless-corpse tests
- Critical Parts: OFF first (to verify cosmetic independence), then ON to verify compatibility
- Optional `-goredebug` startup argument prints qualifying deaths with the stock-art check, pellet counts and whether a headless state was assigned.

## Test cases (run in your Windows build)

1. Stock Doom II IWAD, Zombieman, shotgun from ~32–96 units: aim directly at head and land lethal pellets. Confirm at most one extra gore event per victim, directional pieces, blood behind the body and a correctly headless corpse across all states.
2. Repeat with Shotgun Guy and super-shotgun. Walk around the corpse; ensure no frame unexpectedly restores the head and no neck region is absurdly wide.
3. Hit head nonlethally, or shoot torso lethally, or move beyond 128 units: head should **not** disappear. Near lethal torso may still have a heavier spray.
4. Disable Wound Holes, Critical Parts, regular Blood Spray individually: Gore should still work when Enhanced Gore and **Blood** are on, because masking and heavy sprays have independent capture and presentation paths.
5. Classic gore or Blood OFF: only pre-existing blood/death systems should appear.
6. Try monsters other than the two supported actors, voxel monsters, and a sprite-replacement PWAD: no headless cuts should be applied.
7. Use a test map with `totalkills == 199` and `totalkills == 200`: gore eligible only under 200. Check gameplay speed on both maps.
8. Try Procedural Falls ON, varied deaths ON, and corpse decomposition ON. Observe death-to-corpse handoff for head reappearance or silhouette artifacts.
9. Try Death Ragdoll ON: no headless masking, but extra bounded blood spray. Test resurrection from an Arch-vile: old masks must not persist after resurrection.
10. Test rapid repeat kills, reload, save/load and new map transitions. Note that headless state is not retained across save/reload in this milestone.
11. Compare HP, pellet hit-registration and ammunition consumption between Classic and Enhanced Gore: values must be identical (with Critical Parts separately fixed to the same setting).
12. Watch for unexpected wall decals on sky/untextured surfaces or fragments clipping through a wall. The existing blood collision code is authoritative.

## Recommended test evidence to return

A screenshot or short video of a Zombieman fatal head shot, the resulting fully settled corpse, one side-view of a Shotgun Guy corpse, and console output with `-goredebug`. Include the `graphics.cfg` entries and whether Procedural Falls / Death Ragdoll were on. This is enough to tune masks and burst sizes with minimal iteration.

## Source integration points

- `linuxdoom-1.10/p_map.c`, `r_state.h`, `r_main.c`: post-damage cosmetic callback.
- `platform/scene3d/blood.cpp`: pre-damage sprite impact capture.
- `platform/scene3d/wounds.cpp`: discharge accumulation, stock-art gate, death decisions, cached per-frame headless masks and lifecycle.
- `platform/scene3d/spray.cpp`: bounded blood/chunk ballistics and rendering.
- `platform/scene3d/body_motion.cpp`: corpse watcher remains active when Gore is enabled.
- `platform/scene3d/scene.cpp`, `internal.h`: callback registration.
- `platform/scene3d.h`, `settings_panel.cpp`, `settings_layout.h`: options and F4 integration.

### Source verification status

- Parsed the uploaded source tree and traced the existing damage/render flow.
- C syntax checks for modified `p_map.c` and `r_main.c` passed (the existing `r_main.c` missing declaration warning for unrelated `I_GetTime` was ignored).
- **Not** compiled/linked/tested on the complete SDL3/Vulkan Windows engine: the user requested source implementation only, and third-party/build/test directories were not uploaded.

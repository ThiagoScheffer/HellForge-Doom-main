# Environmental Corpse Reactions 1.0

Accelerated single-player rendering handles hazardous liquid contact for dead monsters, supported ragdolls, and physical gore fragments. Contact uses the existing native flat families from `P_HazardFloorKind`, independently of the setting for environmental damage to living monsters.

The F4 controls are grouped with the death presentation options. The master switch, lava combustion, blood suppression, and fragment reactions default on. Reaction speed defaults to Normal. Acid still uses the existing `acid_corpse_dissolution` preference, which defaults off. Normal-speed corpse durations are six seconds for acid and five for lava. Fragment durations are 1.5 and two seconds.

Ordinary corpses use the native actor support check. A ragdoll checks the solver's spine and hip support and keeps using its cached sector after settling. Textured giblets use the existing sphere sweep. Procedural fragments validate their existing ballistic landing sample against the current floor plane. Losing contact pauses consumption. When a body enters another hazardous material, it retains the progress already made.

The contact reaction reuses the acid corpse palette and collapse presentation, the liquid splash routines, and existing fire, ember, smoke, and particle batches. Lava darkens and collapses the body. Rendering drops fully consumed bodies, and the normal actor remains available to native gameplay. Arch-vile resurrection clears the visual state through the existing lifecycle callback. Direct legacy acid deaths outside contact retain their existing artwork and gameplay rules.

Blood deposits are rejected at the floor boundary, while ordinary wall and ceiling deposits use their normal paths. The rendering loops retire stale floor blood as they encounter it. Corpses under environmental ownership cannot enter ordinary decay or insect presentation. Gameplay feeding rules stay in the native AI.

Reaction state is cosmetic and level-local. Loading a save reconstructs reactions from current support, so progress is reset. The existing material classification recognizes stock animated flat families. WADs that override those flats or define a different liquid are subject to the classifier's existing restrictions. Multiplayer, demo recording, and demo playback keep the pre-existing presentation.

## Verification

Generate a local fixture and run the existing smoke driver with an isolated output directory.

```powershell
python duct-tape-doom-main/tests/ragdoll_arena.py imp build/environmental-corpse-arena.wad
cmake --build build --config Release --target doom doom-wounds-tests doom-giblet-foundation-tests
ctest --test-dir build -C Release -R 'wound-hit-placement|gore-giblet-foundation|gore-giblet-art' --output-on-failure
```

When configured with `DOOM_COMPAT_TEST_IWAD`, CTest generates the arena and runs `environmental-corpse-contact`. The smoke driver also accepts `<DOOM2.WAD> <environmental-corpse-arena.wad> <output-directory> --environmental-corpses` directly. Run it from an isolated directory because the test exercises a real save slot.

The Windows Release engine and relevant test targets compiled. Seven CTest entries passed, including the arena fixture, 20 loaded-map assertions, F4 navigation, decay rules, wound placement, giblet physics, and the giblet atlas checker. The existing corrosive barrel driver also passed 45 assertions. The accelerated hazard arena and a stock MAP01 control with contact reactions disabled both timed out before producing a screenshot. Particle balance and appearance remain unverified.

Each reacting corpse adds a level-local record containing contact, progress, and the last processed tic. Giblets carry the shared reaction state in their existing pool. The body watch and fragment collision loops do the simulation work. A ragdoll can look up the exact sector under up to three candidate support points after its grid reports hazardous contact. The effect submission is capped at 12 corpse sites and four entry splashes per tic. Existing geometry and particle passes submit these effects. Large-map performance was not benchmarked.

## Files changed

| Workspace-relative file | Functions or structures and purpose |
| --- | --- |
| `duct-tape-doom-main/platform/environmental_corpse_reactions.h` | `Reaction`, `advance`, and `speed` share deterministic progress between bodies and fragments. |
| `duct-tape-doom-main/platform/scene3d/internal.h` | `CorpseSurface` and the internal contact, lifecycle, and effect declarations connect the renderer subsystems. |
| `duct-tape-doom-main/platform/scene3d/body_motion.cpp` | `watchBodies`, `tickCorpseReaction`, `corpseSurfaceMedium`, `corpseLifecycleEvent`, and `buildCorpseReactionEffects` own contact, lifetime, and bounded effects. |
| `duct-tape-doom-main/platform/scene3d/ragdoll.cpp` | `ragdollCorpseSurface`, `advanceRagdoll`, and `testRagdollRestFixture` reuse and verify torso and hip support, including a moving resting floor. |
| `duct-tape-doom-main/platform/scene3d/geometry.cpp` | `addThing` and `geometry` draw collapse, charring, and visual completion for sprites, reflected sprites, and ragdoll cards. |
| `duct-tape-doom-main/platform/scene3d/liquids.cpp` | `watchSplashes` hands normal corpse splash tracking over to contact reactions while they own the surface. |
| `duct-tape-doom-main/platform/scene3d/weather.cpp` | `drawCorpseReactionFire` draws using the existing ground fire, ember, and smoke routines. |
| `duct-tape-doom-main/platform/scene3d/blood.cpp` | `collectSevereFall`, `bloodFloorSplat`, `tickBloodFootsteps`, `updateCorpsePools`, and `buildBlood` admit and retire blood according to the contacted floor. |
| `duct-tape-doom-main/platform/scene3d/spray.cpp` | `throwDrop`, `goreBurst`, `sprayKill`, `sprayLanding`, `drip`, `watchSpray`, and `buildSpray` gate corpse blood and consume procedural fragments. |
| `duct-tape-doom-main/platform/scene3d/decoration_ambience.cpp` | Corpse decay admission, insect selection, and critter submission yield to environmental ownership. |
| `duct-tape-doom-main/platform/gore_giblets.h` and `duct-tape-doom-main/platform/gore_giblets.cpp` | `Host`, `Piece`, `Visual`, and `System::step` carry liquid contacts through fragment appearance and pool reuse. |
| `duct-tape-doom-main/platform/scene3d/giblet_scene.cpp` | `Sweep::planes`, `tickGiblets`, and `drawVisual` feed sweep contacts into the giblet simulation and draw the reaction in the original atlas batch. |
| `duct-tape-doom-main/platform/scene3d.h` | `Settings` supplies compatible defaults. |
| `duct-tape-doom-main/platform/settings_panel.cpp` | The existing F4 registry exposes and persists preferences. |
| `launcher/settings-schema.xml` | The generated catalogue keeps launcher controls and backups aligned with F4. |
| `duct-tape-doom-main/CMakeLists.txt` | CTest builds the arena fixture and registers the loaded-map contact test when a local IWAD is configured. |
| `duct-tape-doom-main/tests/critical_smoke.cpp` | `environmentalCorpseIntegration` verifies contact, floor and wall blood, resurrection, and save/load. |
| `duct-tape-doom-main/tests/gore_giblets_foundation_tests.cpp` | `environmentalReactions` verifies state transitions, rates, contact, completion, and pooled fragment reuse. |
| `duct-tape-doom-main/tests/settings_navigation.cpp` | The navigation fixture checks defaults and the native registry's reaction speed and dependency. |
| `Log-Status.md` | Records implementation and verification status. |
| `docs/changes/ENVIRONMENTAL-CORPSE-REACTIONS.md` | Captures behavior, coverage, and performance limits. |

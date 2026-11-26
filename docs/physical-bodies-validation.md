# Physical Bodies 1.0 validation

Build the engine and the existing F4 and wound test targets from the configured Windows build tree.

```powershell
cmake --build build --config Release --target doom doom-settings-navigation-tests doom-wounds-tests
ctest --test-dir build -C Release -R 'f4-director-navigation|wound-hit-placement' --output-on-failure
python duct-tape-doom-main/tests/ragdoll_arena.py zombie physical-bodies-arena.wad
```

The F4 test is registered when CMake is configured with `DOOM_COMPAT_TEST_IWAD`. Load the arena with a DOOM II IWAD and a separate graphics profile. The existing `-ragdolltest ssg:chest` script drives the encounter. Use `-ragdolllog` for solver and card diagnostics. On Windows, launch the newly built engine directly, as the arena script’s automated capture modes use the existing macOS launcher path.

In F4 under **Combat → Deaths**, turn on **Ragdoll deaths** before choosing Physical Bodies. Begin with Flying ragdolls off. Repeat the blast comparisons with it on and use `-ragdolltest rocket:mix`. Record the same actor, map, view, hit tic and render settings for each comparison. The test command keys the existing cosmetic random draw to the map spawn point.

| Case | Exercise | Inspect |
| --- | --- | --- |
| A | Legacy, MAP04 and MAP06 | Record the unassisted solver trace and corpse presentation. |
| B | Natural, same Wall and Close scenarios | Compare torso and hip settling. Confirm `assist 1` follows a fixed-wall slam and expires. |
| C | Cinematic, same scenarios | Compare stronger contact damping. With Flying ragdolls enabled, confirm blast rebound still runs with `assist 0`. |
| D | MAP01 open floor | Compare the per-tic solver traces across the three modes. |
| E | MAP02, MAP03, MAP05, MAP07, and a map with lifts or breakable geometry | Check stairs, ledges, corners, pillars, low barriers, moving support and contact fallback on destructible geometry. |
| F | Pan around front and mirrored bodies and vary view pitch | Check pixel stability, clipping, card placement and the opaque bottom edge against its support. |
| G | Simultaneous deaths with the main view, mirrors and shadows active | Check `ragdollbudget` for at most three ordinary and two reserved builds per tic, visible fallback frames and eventual final builds for deferred bodies. |
| H | Stock humanoid with a carried weapon | Check existing chest and carried-art motion. Secondary arm articulation is deferred. |
| I | Stock IWAD, replacement PWAD, floating monsters and voxel models | Check that appearance remains valid across rigged and fallback paths. |
| J | Horror with all existing cosmetic features enabled | Check fragments, attachments, wall deposits, wetness, audio caps and verified ordinary corpse appearances. |
| K | Scene lighting, dynamic lights, flashlight and palette modes | Check card, fallback sprite and shadow placement. |
| L | Settle corpses, move a lift, resurrect an actor, remove an actor, then change levels | Check support following, corpse ageing on ordinary sprites, removal of generated image references and reset of the per-tic budget. |
| M | Switch Legacy → Natural → Cinematic → Legacy, disable ragdolls or blood, and switch Horror → Enhanced | Check cache replacement and feature independence. |
| N | Repeat the same encounter at fixed resolution, view and settings | Record CPU/GPU frame time, peak and total reconstruction counts, cache hits, deferrals and resource memory. |

The ragdoll can settle away from the gameplay corpse. The displayed card follows the solver and its support. Feeding and resurrection continue to use the gameplay actor. Ordinary corpse appearance staging remains on the ordinary sprite path. The pain-frame relief has no approved matching corpse mask, and stock arm regions are unavailable.

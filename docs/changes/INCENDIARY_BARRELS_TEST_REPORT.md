# Incendiary Barrels v1 test report

Verified on Windows x64 against starting revision `75d5121e0a5b38bf3390ffe5959b9da6e8c8b764`.

| Check | Result |
| --- | --- |
| Configure and native Release build | Passed with `launcher/build.ps1`, the Visual Studio 17 2022 generator, and the local x64 Build Tools. The Windows launcher was rebuilt. |
| GPU shaders | The implementation uses existing passes and shader bindings. Vulkan rendering passed with the existing compiled shaders. |
| Configured regressions | **41/41** Release CTest tests passed. |
| Incendiary integration | **81 checks passed** in `--incendiary`. Includes all four acid/fire combinations, the widening spill on repeated hits, legacy spill radius, safe explosion frame progression, rocket and BFG damage, queued chain waves, moving-floor contact, a closed partition, immunity and armor, attacker and fire-death attribution, and the 32-pool cap. |
| Save and load | Real game saves round-tripped simultaneous AC01/OI01, live oil and residual exposure, and a pending chain wave. A no-OI01 save loaded. Unknown and truncated OI01 cases were checked directly. |
| Settings | Three separate processes exercised F4 apply, restart/read, and live disable. **8 checks passed** across the processes. The persisted preference matched the live setting. |
| Acid compatibility | **45 checks passed** with ordinary collision and another **45** with solid-map collision. Existing Vulkan acid leak and rupture captures passed. |
| Elemental and Pinky regressions | **39** existing elemental/session checks passed. The Pinky chainsaw damage and wound smoke passed with both Vulkan screenshots present. |
| Incendiary Vulkan captures | Eight 640×400 views checked the intact barrel, directional leak, three rupture stages, active oil, fading oil, and extinguished floor. Blood and optional elemental combat settings were disabled. |
| Unsupported sessions | The runtime fixture checked netgame, demo playback, and demo recording gates, including the RNG index. A native MAP23 demo initialization loaded stock FCAN actors with the preference enabled. |
| Native UDP multiplayer | Two localhost peers played stock MAP23 with opposite Incendiary Barrels preferences. Both exited successfully and **341 shared tics matched** with zero state-hash differences. Logs and commands are under `build/incendiary-verification/final/lan`. |
| Large PWAD | Sunder 2308 MAP15 exercised **7,976 active thinkers**, two source barrels, three active pools and 40 oil-contact updates. The separate simultaneous-rupture fixture reached the 32-pool cap. |

Configure used `DOOM_BUILD_SDL=ON`, `DOOM_RENDER3D=ON`, `DOOM_SOUNDFONT=OFF`, `BUILD_TESTING=ON`, `DOOM_TEST_GPU=OFF`, and the static MSVC runtime. The native GPU smoke fixtures ran separately. The updated build and CTest logs are `build/incendiary-visual-update-build.log` and `build/incendiary-visual-update-ctest.log`. Fixture logs and PNGs are under `build/incendiary-verification/final`.

## Reproduce

Generate the controlled fixture using `python duct-tape-doom-main/tests/ragdoll_arena.py imp build/critical-arena.wad`. Run `doom-critical-smoke.exe <DOOM2.WAD> <arena.wad> <existing-output-directory> <mode>` from a separate working directory for each of `--incendiary`, `--visual-incendiary`, `--acid`, `--acid-solid`, `--visual-acid`, `--elemental2`, and `--visual-saw-cut`. For `--incendiary-large`, pass Sunder 2308 as the PWAD. For settings, start with `accelerated 0` in a clean output directory’s `graphics.cfg`. Run `--settings-oil-write`, `--settings-oil-read`, then `--settings-oil-disable` against that same directory.

## Coverage limits

The performance sample measures oil-contact updates, separate from full simulation and rendering. Windows Vulkan was the accelerated runtime tested. Classic software mode has the native barrel burst and damage rules. The full oil film, flames and smoke are available in the accelerated renderer. Audio initialization and the effect path ran with SDL dummy audio, without a listening check on physical output. The no-OI01 save was produced by the current writer with the feature Off. The native demo check exercises the beginning of playback, and recording was covered by the session gate check.

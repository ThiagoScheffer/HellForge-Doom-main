# Corrosive Barrels v2 test report

Verified on Windows x64 against HELLFORGE starting revision `06c786b614ff403ff43e68034396ce702f0f8b32`.

## Build and test results

| Check | Result |
| --- | --- |
| CMake configure | Passed. CMake 4.3.1-msvc1, Visual Studio 17 2022 generator with the installed Build Tools, x64, MSVC 19.44, Windows SDK 10.0.28000.0. |
| Native Release build | Passed. Built all configured targets with `cmake --build build --config Release --parallel 8`. |
| Shader generation | Passed. `tools/Recompile-Shaders.ps1` regenerated Vulkan SPIR-V and Metal MSL variants. Vulkan was exercised below. Metal runtime was not exercised on Windows. |
| Regression suite | **37/37 passed**, Release CTest, 4.91 seconds. |
| Real game, classic collision | **45 checks passed** in `doom-critical-smoke --acid`. |
| Real game, solid-map collision | **45 checks passed** in `doom-critical-smoke --acid-solid`. |
| Accelerated visuals | Passed in `--visual-acid` on Vulkan. Verified the setting loaded while both blood flags were off. Captured leak and rupture frames at 640×400. |
| Launcher catalogue | Generated 369 descriptors. Corrosive Barrels is a Combat checkbox. |

Configuration flags were `DOOM_BUILD_SDL=ON`, `DOOM_RENDER3D=ON`, `DOOM_SOUNDFONT=OFF`, `CMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded`, `BUILD_TESTING=ON`, an empty `DOOM_GAME_DATA`, `DOOM_TEST_GPU=OFF`, and all three monster profiling and benchmark options off. The opt-in Vulkan fixture was run separately. MSBuild reported non-fatal warnings from existing rendering, audio and fixture code.

The gameplay fixture checks default off and config loading, the unchanged blast damage, increasing leak size, leak expansion and refresh, stopping droplets on death, the four-pool rupture shape, duplicate rupture protection, ground and airborne exposure, the 17-tic interval, overlap without stacking, suit protection, armor, god mode, invulnerability, expiration, death without fire corpse treatment, the global pool cap, actor removal, map cleanup and multiplayer/demo gates. It also round-trips an owned leak and an immediate post-rupture save with live exposures through the real save and load entry points. The AC01 checks cover no-trailer saves, disabled restore, a bad owner ID, an unsupported framed version and a truncated footer.

## Reproducing the fixtures

Use a legally installed DOOM II IWAD and the local geometry-only test arena. To generate the arena, run `python duct-tape-doom-main/tests/ragdoll_arena.py imp build/critical-arena.wad`. Launch `doom-critical-smoke.exe <iwad> <arena> <existing-output-directory> --acid` from an isolated working directory. Repeat with `--acid-solid` and `--visual-acid` using fresh output directories. The fixture writes settings, saves and capture evidence locally.

The last run is recorded locally under `build/acid-v2-496b97eb956b417495db5df865b8e534`. The two Vulkan captures are `visual-acid/acid-leak.png` and `visual-acid/acid-rupture.png`. Native build and CTest logs are `build/acid-v2-native-build.log` and `build/acid-v2-ctest.log`.

## Remaining Windows coverage

The fixture uses stock actors and a controlled test arena. Manual navigation through F4 and restart persistence, arbitrarily authored Boom maps, varied moving floors and excavated solid geometry still need broader play coverage. The no-AC01 save is generated through the current engine's legacy layout. No save from a separately installed historical build was used. A malformed trailer test does not establish full-file corruption recovery. Multiplayer and demo gates were checked by setting the engine flags in the fixture. No live peer session or demo was run. No large-map FPS claim is made.

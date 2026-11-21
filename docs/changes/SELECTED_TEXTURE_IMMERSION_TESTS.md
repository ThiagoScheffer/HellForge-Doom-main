# Selected Texture Immersion v1 verification

## Final artwork set

The Windows x64 Release build and Vulkan/Metal shader generation passed. The launcher embeds the matching engine, verified by SHA-256. All 39 configured CTest checks passed.

The final Doom II gallery rendered 23 selected scenes with eight matching Off and High frames each at 640×400. All selected scenes changed, and the sampled ordinary wall was pixel-identical. The gallery covers the six body panels, the four gates, both eye-switch states, the animated pipe and fire families, all three fountain faces, and all three Doom II skies. The captures and logs are in `build/immersion-finish/`. Vulkan was the runtime backend for these checks.

## Production expansion

`tools/Build-Hellforge.ps1 -RecompileShaders` regenerated both shader targets, built Windows x64 Release, and packaged `dist/DOOM-Hellforge.exe`. The launcher's embedded engine SHA-256 matched `build/Release/doom.exe`. All 37 CTest checks passed. F4 navigation and the two-process persistence test passed, including removal of the legacy `living_cavities` key on save.

The Vulkan gallery ran at 640×400 with the default crisp wall filter. Each room recorded eight matched Off and High frames. All 14 selected surfaces changed. The ordinary wall and animated ceiling control had identical sampled pixels. Both sampled iron rails on `SKINMET1` were pixel-identical while the leather changed. Captures and logs are under `build/immersion-expansion/`.

## Initial v1 verification

## Build and regression tests

| Check | Result |
| --- | --- |
| `tools/Recompile-Shaders.ps1` | Passed with the bundled `glslangValidator` and `spirv-cross`. Vulkan SPIR-V and Metal sources were regenerated in `platform/gpu_shaders.h`. |
| `cmake --build build --config Release --parallel 8` | Passed with Visual Studio x64. MSBuild emitted existing compiler and CMake developer warnings. |
| `ctest --test-dir build -C Release --output-on-failure` | 37 tests passed. |
| `doom-settings-navigation-tests.exe <DOOM.WAD>` | Passed. Checked the Textures menu, defaults, intensity cycling, all four toggles, F4 apply, and the settings writer and reader. |
| `doom-critical-smoke.exe <DOOM2.WAD> <arena.wad> <output> --settings-immersion-write` and `--settings-immersion-read` | Passed across two processes. The first applied and saved through F4. The second restored the master, High intensity and all four independently disabled effects. |

## Vulkan render checks

The local evidence directory is `build/selected-texture-immersion/`. The three-frame contact sheets use tics 25, 60 and 95. Image comparisons exclude the status overlay and weapon where they overlap the region being measured.

| Check | Evidence |
| --- | --- |
| Master Off versus pre-change engine with Living Surfaces Off | The sampled `SKINFACE` wall region was pixel-identical at tic 25 in `map1-baseline-off.png` and `map1-off.png`. |
| All six curated profiles | `map{1,2,3,5,6}-{off,medium}.png` show the five stock wall composites. `floor-{off,medium,low,high}.png` and `flat-ceiling-{off,medium}.png` show the same selected flat on the floor and ceiling. |
| Adjacent artwork | At tic 25, selected portions of `SKINFACE`, `SKSPINE2` and `SKINMET1` changed. The sampled region on each unselected half was pixel-identical. |
| Intensity and four effect controls | In the lit `SFLR6_4` floor crop, the Medium versus Low and High comparisons each changed at least 69,000 pixels at every sampled tic. Turning off Morph, Organic pulse, Wetness or Cavity enhancement individually changed the crop. With all four effects off, the crop differed from the master Off reference by at most 0.0002 mean RGB levels per channel. |
| Time-varying pulse | The signed Medium versus Pulse Off difference changed between tics 25 and 95 by a mean absolute 2.25 RGB levels per channel in the same `SFLR6_4` crop. |
| Surface Detail and Soft Light | `floor-medium.png`, `floor-detail-off.png`, `floor-soft-off.png` and `floor-both-off.png` capture the four combinations while immersion is active. |
| Filtering and resolution | Captured crisp and smooth bilinear at 640×400 and sharp bilinear at 960×600. |
| Animated and ordinary textures | The nonselected `STARTAN3` and animated `NUKAGE1` scene rendered with immersion On and Off across three tics. The same-tic comparisons had very small differences in mean RGB. |
| Texture switching | `doom-perf-smoke.exe` with `-test-switch-toggle` switched 80 textures after the initial bake in the Doom II Switch Gallery with immersion set to High. Captured `switch-toggle/switch.png`. |
| Large PWAD | Sunder 2308 MAP01 loaded, rendered and exited with the feature On and Off. The sampled world crop differed by 0.0095 mean RGB levels per channel. This was a render smoke check with monsters disabled. |
| Blood, acid and window reflection paths | The existing Vulkan smoke drivers produced severe blood fallout and wall impact, acid leak and rupture, and 15 window-glass frames with reflections enabled. Those fixtures run with the immersion default Off. |

Create the geometry-only gallery with `python duct-tape-doom-main/tests/selected_texture_scene.py build/immersion-gallery.wad`. Use Doom II and `-warp 1` through `-warp 7`. The following command captures three frames at 640×400.

```powershell
build/Release/doom.exe -iwad <DOOM2.WAD> -file <gallery.wad> -warp 4 -nomonsters -nodigging -graphics <graphics.cfg> -width 640 -height 400 -renderview 0 0 90 ' -58' -rendertic 24 -renderevery 35 -renderframes 3 -rendersheet 3 -rendercheck <capture.png>
```

Pass a leading space inside the quoted negative pitch because the render-view parser treats a leading hyphen as a new argument. Use a fresh graphics file outside the player's profile to switch cases.

## GPU timing

Used `doom-perf-smoke.exe` on gallery MAP04 at 960×600 on Windows Vulkan. Its GPU column measures submission through fence completion. Two Off and two High runs each recorded four headings from the same point. This fixture sampled the selected floor and ceiling from a horizontal view under its normal lighting.

| Setting | Run 1 median | Run 2 median |
| --- | ---: | ---: |
| Off | 3.30 ms | 3.81 ms |
| High | 3.32 ms | 3.48 ms |

The ranges overlap. This sample is too small to estimate a reliable frame-time difference. Raw per-view measurements are in `gpu-{off,high}-{1,2}/views.csv` in the evidence directory.

Metal shader translation ran on Windows. Metal runtime verification remains pending. The window smoke exercised the shared renderer with reflections enabled. A selected surface seen inside a reflection and a large-PWAD gameplay benchmark with monsters active remain unverified.

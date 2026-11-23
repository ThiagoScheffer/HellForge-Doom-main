# Incendiary Barrels v1 changelog

- Added the five-frame BEXP rupture animation and directional oil spray on weapon impacts. Later hits briefly restart the spray and enlarge the same inert oil spill. The renderer retains its low flames and bounded sparks.

- Added `linuxdoom-1.10/p_incendiary.cpp` and `p_incendiary.h` for stock-actor eligibility, deferred ruptures, spill geometry, timed exposure, and the `OI01` extension.
- Hooked the subsystem into `p_inter.c`, `p_map.c`, `p_mobj.c`, `p_tick.c`, and `g_game.c`. Native radius falloff handles the explosion. Existing corrosive and elemental save trailers retain their relative order.
- Added `platform/scene3d/incendiary.cpp` for clipped oil film, flames and short-lived rupture effects. Registered the renderer and gameplay sources in CMake and connected the effect pass in `platform/scene3d/scene.cpp`.
- Added the new toggle to the F4 option registry, Barrels layout group, launcher schema generator, and generated launcher catalogue.
- Added native integration and Vulkan capture modes in `tests/critical_smoke.cpp` and `tests/incendiary_smoke.inc`. Added isolated persistence checks using the F4 controls.
- Kept the existing Pinky smoke test’s output paths alive until the renderer finishes writing the captures and verified both files in that fixture.

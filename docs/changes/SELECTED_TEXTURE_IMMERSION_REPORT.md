# Selected Texture Immersion v1 implementation report

The feature is available in **F4 → Textures → Selected Texture Immersion**. The master defaults to Off, the intensity defaults to Medium, and Morph, Organic pulse, Wetness, and Cavity enhancement start enabled beneath the master. F4 applies the controls to the running renderer. Saved `living_surfaces` values carry forward from Living Surfaces Lite, and the launcher schema exposes the six controls.

| Files | Change |
| --- | --- |
| `linuxdoom-1.10/r_data.c`, `r_data.h` | Expose the names and clipped bounds of wall artwork patches to the image loader. |
| `platform/scene3d.h`, `platform/scene3d/images.cpp`, `platform/scene3d/scene.cpp` | Cache profile and region on loaded images, then bind the existing surface and camera metadata. |
| `platform/settings_panel.cpp`, `launcher/settings-schema.xml` | Add the F4 group and regenerate the launcher schema. |
| `platform/shaders/living_surfaces.h`, `platform/shaders/world/living_surfaces.glsl` | New shared profile IDs and flags, material coefficients, and region-aware shading. |
| `platform/shaders/world.frag`, `platform/shaders/world/surface_light.glsl`, `platform/shaders/world/sector_light.glsl`, `platform/gpu_shaders.h` | Integrate the effect into the world fragment shader, retain the independent Soft Light flag, and regenerate the Vulkan and Metal variants. |
| `tests/selected_texture_scene.py`, `tests/settings_navigation.cpp` | Generate a geometry-only Doom II test gallery and exercise the controls through the F4 menu. |
| Test code in `platform/settings_menu.cpp` and `tests/settings_persistence.inc` | Apply and save all six settings through F4 in one process, then load and check them in a fresh process. |

The starting workspace contained an uncommitted Living Surfaces Lite implementation. Its broad material matching was replaced with the curated registry. The original settings key and metadata bit positions carry forward. `frameMean.a` keeps the material in bits 0–3 and the worn-plate marker in bit 4, with the profile in bits 5–7. For profiled nonliquids only, `frameMean.rgb` and `cycleMean.r` carry the selected rectangle. Liquid averaging retains those components on liquid surfaces. Camera `map.w` keeps Soft Light in bit 0 and uses bits 1–7 for immersion controls. The uniform layouts, descriptor bindings, and relief channels are unchanged.

The shader applies the selected material's shading after coverage and before lighting and palette conversion. Surface Detail follows its existing flag, while the selected profile can contribute additional relief. Worn plates retain their existing finish. The game clock drives the motion, and the render calculation uses no gameplay RNG. The project and IWAD artwork files need no changes to use the effect.

See [design](SELECTED_TEXTURE_IMMERSION_DESIGN.md) and [verification](SELECTED_TEXTURE_IMMERSION_TESTS.md) for the WAD mapping, limits, and test evidence.

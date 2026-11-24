# Gore giblet textures and engine integration

All 32 files in `duct-tape-doom-main/assets/gibs/` are original replacements exported from two independently prompted ImageGen atlases. No existing sprites were supplied to ImageGen. The export retains every filename and canvas size and uses RGBA PNG with binary alpha. All 32 files decoded successfully, and every file has a distinct SHA-256 digest. The pack totals 15,612 bytes.

| Proposed use | Files | Export notes |
| --- | --- | --- |
| Heavy tissue | `GIB0A0/B0`, `GIB1A0/B0` | A is airborne and B is settled |
| Flesh | `GIB2A0/B0`, `GIB3A0/B0`, `GIB8A0/B0` | Three sizes and silhouettes |
| Viscera | `GIB4A0/B0`, `GIB5A0/B0` | Use the engine's species tint for Cacodemon blue |
| Bone | `GIB6A0/B0`, `GIB7A0/B0`, `GIB9A0/B0` | Pale mineral highlights |
| Surface artwork | `CRS1A0–CRS3A0`, `SGC1A0/B0`, `SGC2A0/B0`, `SGW1A0/B0`, `SGW2A0/B0`, `SQSHA0` | Floor mounds, ceiling effects, wall smears and a hanging strand |

## Engine integration

1. Add `platform/gore_giblets.cpp` to `DOOM_SCENE3D_SOURCES` in `duct-tape-doom-main/CMakeLists.txt`. Package the art through a build-generated atlas so a release build can resolve it regardless of its working directory. SDL already decodes PNGs and `scene3d/images.cpp` uploads palette-index/coverage pairs. Quantize at load time, cache the species tint variants, and store each sprite's opaque bounds and pivot so the different transparent margins do not change its apparent size. Reserve the surface artwork for integration through the current decals.
2. Keep shot qualification, once-per-life state and corpse masks in `scene3d/wounds.cpp`. At `goreBurst()` in `scene3d/spray.cpp`, send a qualified textured fragment burst to one level-owned `giblets::System`. Give each discharge/victim event a stable ID and cosmetic seed. Preserve the approved M1 head fragments and use the current procedural effect if the atlas cannot be loaded. Make the paths exclusive for each burst. Count both active fragment implementations against the existing 64-fragment limit and keep the combined drop budget at 768.
3. Tick the library from the existing spray tick, then clear it on map change, renderer shutdown and entry into disabled game modes. Render `Visual` as a rotated, lit camera-facing quad using the existing textured world batches. Use `variant % pairCount` with the relevant material table and choose B when `resting` is true. Anchor settled art to the floor. Keep blood splats independent of fragment visibility so the existing F4 switches continue to work.
4. Implement `Host::trace` against world geometry and return the first swept contact, including floor and ceiling heights, sky, and liquid. Reuse the engine's wall-span validation. The current `firstSolidWall()` scans every line and allocates a crossing list, which is too costly as the normal path for every piece at 70 Hz. Use a read-only spatial candidate query before collision tests. `TraceHit` and `Impact` need to carry the resolved wall line/side/section or floor/ceiling sector and an event ID. The normal and position currently in those structs are insufficient to address the existing decals or enforce the three-wall-mark limit per burst. Route impact events to `bloodWallSplat()` and `bloodFloorSplat()` with the existing tint and budgets. Bone can land without depositing blood.
5. Verify the pure simulation and the trace adapter before GPU playtesting. Test open floors, near walls, stairs, ceilings, liquids and sky. Check landing pivots, blue Cacodemon debris and stains, F4 gating, the 199/200 map boundary and level reload. Measure worst-case collision work before making the new fragment path the default.

## Artwork generation prompts

Both prompts ran through the built-in ImageGen tool with transparent backgrounds. The output atlases were split into cells and exported at engine texture sizes.

<details>
<summary>Airborne and resting sprite atlas</summary>

> Create one original transparent sprite atlas for a fictional monster game. Exactly 4 columns and 4 rows of isolated 2D top-left-lit handheld pixel-art fantasy monster debris sprites. Each cell equal size on a square canvas, centered silhouette with ample transparent margins and no object crossings. Reading order, adjacent two sprites are airborne and settled shapes of the same material, with the settled shape wider and lower. Row 1: a heavy triangular segmented burgundy tissue wedge with a diagonal pale seam, its compressed scrap, a twin-knob dark crimson tissue knot, its compressed scrap. Row 2: a small amber-pink flesh chip shaped like a pointed seed, its broken chip, a split rose-colored fleshy fan with pronounced stepped edges, its resting fan. Row 3: a compact garnet lobed crescent, its settled crescent, a clustered plum organ-like rosette with small hooked lobe, its flattened rosette. Row 4: a beige and umber angular cartilage hook, its resting hook, a jagged broad ochre bone splinter with a dark fracture stripe, its resting shard. Stylized non-photoreal abstract fantasy monster effects, coherent rich muted earthy colors, hard pixel edges, distinct readable silhouettes, restrained three-to-five tone ramps. No templates, no existing game artwork or characters, no outlines around cells, no grids printed, no lettering, no ground plane, no cast shadows. True transparent alpha in the gutters and around every sprite. Each object should occupy only the inner center two thirds of its designated cell.

</details>

<details>
<summary>Small fragments and surface effects atlas</summary>

> Create one original square 4-column, 4-row transparent pixel-art effects atlas for a fictional monster game. Every cell contains exactly one centered isolated sprite with broad transparent gutters. Row 1: tiny angular flesh chip, flattened tiny fleck of matching material, pointed ivory bone sliver, matching grounded ivory chip. Row 2: dark berry tissue on a ceiling seen from below, comma-shaped suspended berry drip, small pink hanging bead with a serrated seam, matching dangling vertical drip. Row 3: thin dark carmine wall streak with two short prongs, matching wall smear viewed horizontally, narrow mulberry nick, matching small flattened wall mark. Row 4: wide compressed mound of maroon shards with a pale core, elongated flat smear with small dark lobes, widest low staggered tissue silhouette, narrow vertical crimson strand with a branching end. Stylized retro low-resolution pixel art, big clean stepped pixels, compact silhouettes, rich deep wine and pale ivory accents, upper-left highlights, no extra objects, no printed grids, no lettering, no background, no drop shadows. Do not copy any existing game artwork. Keep all objects within their cell and preserve true transparent alpha.

</details>

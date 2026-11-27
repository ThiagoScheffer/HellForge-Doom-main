# Environmental Footprints 1.0

Accelerated Doom rendering can show bootprints after the displayed player leaves a map-authored water, blood, or mud flat. Open **F4 → Atmosphere → Footprints**. The master defaults off, each material defaults on, and persistence defaults to Normal. Classic rendering disables the effect.

| Source | Accepted flat names | Transfer window | Print limit per trail | Normal lifetime |
| --- | --- | ---: | ---: | ---: |
| Water | `WATER`, `FWATER`, `SWATER`, and suffixes 1–4 | 2 s | 8 | 6 s |
| Blood | `BLOOD` and suffixes 1–3 | 2 s | 20 | 45 s |
| Mud | `MUD`, `MUD1`–`MUD8`, `MUD01`–`MUD08` | 3 s | 12 | 25 s |

The persisted keys are `environmental_footprints 0`, `footprint_water 1`, `footprint_blood 1`, `footprint_mud 1`, and `footprint_persistence 1`. Persistence values 0, 1, and 2 apply multipliers of 0.5, 1, and 1.5 to the lifetimes of new environmental prints. Transfer windows and corpse-pool prints keep their original timing. The launcher exposes the choices under Atmosphere and applies the game's eligibility rules at launch.

Still flats are eligible. Water, blood, and mud replace an existing load when contacted, including when the new material is switched off. Mud names are a map-authoring convention and are absent from the local stock Doom II IWAD. NUKAGE, LAVA, liquid slime, damaging sectors, sky floors, and unknown liquid surfaces cannot supply prints. Grass, dirt, and dry metal slime-name flats contribute no pigment. A grounded exit begins the trail and subsequent strides can alternate boots every 20 units. Jumping out of a source discards its load. Unsupported floor contacts and blocked boot placements are skipped.

The renderer keeps one tic-driven tracker and up to 96 shared footprint records. Environmental prints can replace the oldest environmental record. Corpse-pool prints can also replace an old legacy record when the collection contains only legacy tracks. Environmental prints never evict a legacy track. Corpse-pool pickup still has the 200-kill-count gate, freshness, tint, and rain wash. Map-authored prints work with Blood splats and pools turned off and have no population gate. Severe pools use their existing rendering and do not supply boot pigment.

The tracker reads live base-flat IDs and checks physical floor height, clipped BSP floor geometry, walls, and excavation support. Prints follow supported moving floors. The renderer retires prints when their receiving flat changes, and rechecks support when floor height or the excavation mesh revision changes. Boom height-transfer sectors are conservatively excluded. Successful player teleports discard the current trail load while the master is enabled, including short moves. Actor or displayed-player changes discard carried pigment. Map changes, save loads, and renderer shutdown clear the visual state. The master and child switches purge affected prints when settings are applied. Tracks live only in renderer memory.

Water uses the existing straight-alpha translucent world pipeline with depth testing and no depth writes. The world shader blends a tread-shaped darkening layer over the shaded floor and uses its atmospheric fog and light-dependent specular terms. The same blend path is available in the ordinary reflected-world draw. Blood and mud use indexed pigment with stable late-life texel removal. The shared 16 × 32 boot mask keeps the legacy seeds, tread, mirroring, and tint mapping. The image store can add up to nine cached variants, three stages each for water, mud, and blood fallback. Image and palette variants are rebuilt with the map's resource store. There is no extra render pass, scene-color copy, or per-print texture.

## Local verification

| Check | Result | Log |
| --- | --- | --- |
| CMake shader generator with local glslang and SPIRV-Cross | Exit 0, Vulkan SPIR-V and Metal MSL generated in a fresh scratch directory | [Shader generation](../../build/footprint-shaders.log) |
| `cmake --build build --config Release --target doom environmental-footprints-tests doom-settings-navigation-tests --parallel 8` | Exit 0 | [Release build](../../build/footprint-release.log) |
| `ctest --test-dir build -C Release -R "^environmental-footprints-rules$" --output-on-failure` | Exit 0, one focused test | [Native rules](../../build/footprint-rules.log) |
| `doom-settings-navigation-tests.exe <stock Doom II IWAD>` | Exit 0, MAP01 loaded with an isolated native config, 2,282 F4 checks passed | [F4 and configuration](../../build/footprint-f4.log) |
| `launcher/build.ps1 -LauncherOnly -OutputPath build/footprint-package/DOOM-Hellforge.exe` | Exit 0, regenerated catalog embedded in a local scratch package | [Launcher build](../../build/footprint-launcher.log) |

The rules test covers names, stride and time boundaries, source replacement, disabled-source cleaning, airborne exits, identity and teleport resets, and record eviction. The F4 test verifies defaults, independent flags, invalid choices, and registry round-trip. Legacy tint, boot mask, and rain behavior were checked against the existing code by inspection. Actual GPU appearance, excavation and moving-platform acceptance, corpse and acid gameplay regressions, gameplay determinism, and frame timings were not measured. No custom WADs were loaded.

## Doom II manual visual check

From the workspace root, create a separate run directory and pass explicit configuration paths.

```powershell
$engine = (Resolve-Path build/Release/doom.exe).Path
$iwad = (Resolve-Path test-references/DOOM2.WAD).Path
New-Item -ItemType Directory -Force build/footprint-visual-profile | Out-Null
Push-Location build/footprint-visual-profile
try {
    & $engine -iwad $iwad -warp 2 -nomonsters -config doom.cfg -graphics graphics.cfg
} finally {
    Pop-Location
}
```

On **MAP02**, switch the master on and leave Water enabled. The IWAD has a `FWATER1` channel south of the edge from (2592, 3624) to (2720, 3624), near the player start at (2704, 3728). Walk south into water, then north over the edge onto `FLAT5_4`. Keep walking and look back for alternating dark tread that preserves floor detail and fades within six seconds at Normal persistence. Repeat with Blood splats and pools off, then apply Water off and on while any prints remain to check that they stay cleared.

For blood, change the warp to 25. The IWAD has a same-height `BLOOD1` to `GATE2` edge between (-768, 896) and (-832, 896). Walk north from the blood floor across that edge, with global blood off and Blood texture footprints on, and check for temporary pigment. These landmarks were identified from the IWAD directory and map geometry. Visual checks at those locations remain open.

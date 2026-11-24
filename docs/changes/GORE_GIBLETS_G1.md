# HellForge G1 textured giblets

Qualified Gore events now launch material-specific sprites through the native fixed-step simulation. Fragments use A frames in flight and B frames when settled on the floor. Living Mancubus hits can eject heavy tissue, Pinky and Cacodemon eject viscera, and Revenant ejects pale bone that leaves no blood marks. Existing M1 head bursts retain their procedural presentation. M2.1 shot qualification and the later half-health living wounds for Zombieman, Shotgun Guy, Imp and Mancubus retain their original rules.

The collision adapter sweeps the level blockmap and resolves floor, ceiling and wall contacts. It passes verified surface IDs and the fragment's own tint to the existing decal system. Light blood and both fragment paths share the existing budgets of 768 drops, 64 heavy fragments and up to three added wall marks per event. The launcher’s `-solidmaps` mode uses textured giblets where original Doom surfaces can be verified. Bursts originating near a volume cut use the procedural fallback, and pieces that reach an uncertain surface expire without claiming a decal. The standalone volume lab and missing atlas also use procedural fragments.

## Source and assets

- `platform/gore_giblets.{h,cpp}` carries event IDs, surface handles and actual contact points through the simulation.
- `platform/scene3d/giblet_scene.cpp` owns collision, sprite batching, the map-local atlas and the bridge to existing blood marks. `spray.cpp`, `wounds.cpp` and `scene.cpp` route events and manage lifetime. `blood.cpp` scopes tint during decal creation.
- `tools/pack_giblets.py` checks all 32 original PNGs and emits file dimensions, opaque bounds and SHA-256 hashes. It generates `platform/scene3d/giblet_atlas.generated.h`, a checked-in 256 × 114 atlas with ten flight/rest pairs and twelve reserved surface sprites. The build uses the embedded atlas, and the renderer caches its PLAYPAL variants under image key `-0x45000000`. Run `python tools/pack_giblets.py --check` to verify reproducibility after art changes. [Artwork provenance](GORE_GIBLETS_ART.md)
- `tests/gore_giblets_foundation_tests.cpp` exercises deterministic simulation, contact metadata, no blood from bone, invalid values, event deduplication and budgets. `tests/critical_smoke.cpp` adds `--gore-g1` and `--gore-g1-solid` using the stock Doom II palette and real shotgun pellets.

## Verification

Windows Release built successfully. The four focused CTest cases, stock-IWAD wound checks, 46 standard G1 smoke checks, 50 solid-map G1 smoke checks, 14 procedural fallback checks and the 4,699-check native regression suite passed. The stress fixture started with 64 pieces and executed 3,650 sweeps over 35 game tics. The headless update took 0.62 ms on the standard map and 0.83 ms in solid-map mode in that run. The near-wall Cacodemon fixture recorded three extra wall marks, and the atlas used one texture batch per drawing pass.

## Windows playtest

From the repository root, build Release and launch with a separate working directory for logs and local settings.

```powershell
cmake --build .\build --config Release --target doom
$game = (Resolve-Path .\build\Release\doom.exe).Path
$iwad = (Resolve-Path .\test-references\DOOM2.WAD).Path
$run = New-Item -ItemType Directory -Force .\build\g1-playtest
Push-Location $run.FullName
try {
    & $game -iwad $iwad -warp 5 -skill 3 -solidmaps -goredebug 2>&1 | Tee-Object g1.log
} finally {
    Pop-Location
}
```

1. In F4 enable Enhanced Gore, Blood, body destruction, Flying flesh fragments and Enhanced wall blood. On MAP05 weaken a Cacodemon to around half health, then trigger a qualifying close shotgun hit while it survives. Compare a nearby wall with open floor. Look for blue fragments from both front and side, blue stains only on verified surfaces, and flat settled sprites on the floor. Repeat with a fatal hit and a Pinky. Check for `fragmentPath=textured` and `maxEventWall` at or below `3` in the log.
2. On MAP02 verify the existing living wounds at half health for Zombieman, Shotgun Guy and Imp, and the existing head bursts on qualified fatal hits. Use MAP07 for Mancubus living tissue and the existing wound, and MAP10 for pale Revenant bone. Inspect the ordinary death frames for the stock silhouettes.
3. Compare 30, 60 and 120 FPS and watch the A/B pose transition. Toggle Wall Blood and Flying flesh fragments separately, switch to Classic, pause, and reload the map. Repeat beside an edited wall and with `-solidmaps` omitted. Check for sprite-edge bleeding, occlusion and stale marks or sprites after reload.

GPU screenshots and frame-rate comparisons are still pending. Offscreen Vulkan lacked `VK_EXT_headless_surface`, and the hidden Windows Vulkan render probe timed out before capture. The twelve surface sprites are packaged for the optional G1.5 work. G1 uses the existing procedural decal artwork.

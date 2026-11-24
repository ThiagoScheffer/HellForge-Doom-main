# HellForge G1.2 impact gore and explosion aftermath

The starting tree contained M2.1 and G1. G1.1's routine impact tier was absent. It is now part of the existing per-victim shotgun discharge resolver. Confirmed, damaging opaque hits through 128 Doom units can produce routine pieces at full health. From over 128 through 320 units, the same evidence can produce one smaller, short-lived piece. Severe and fatal results replace routine output for the discharge.

Verified human headshot kills request six pieces from one textured emitter. Two of the six use flesh frames, and the other four use the brain-like material made from the original `GIB4/GIB5` art. The Imp and Zombieman use their uncut stock death sprites through the final corpse pose. The Shotgun Guy retains his verified corpse mask. Missing atlas or unsupported geometry can use the existing procedural path. Zombieman and Shotgun Guy torso art stays intact while alive. The Imp and Mancubus still qualify for their verified half-health living wounds and drips.

| Monster | Close routine | Medium | Close major living / fatal | Rocket or barrel surviving / fatal¹ | Material |
| --- | ---: | ---: | --- | --- | --- |
| Zombieman | 2 | 1 | 4 body / 6 verified head | 3 / 6–8 | Red flesh or verified brain-like mix |
| Shotgun Guy | 2 | 1 | 4 body / 6 verified head | 3 / 6–8 | Red flesh or verified brain-like mix |
| Imp | 3 | 1 | 4 / 6 | 4 / 8–10 | Flesh |
| Revenant | 2 | 1 | 3 / 6 | 3 / 8–10 | Pale bone |
| Mancubus | 4 | 1 | 5 / 8 | 5 / 10–12 | Heavy tissue, with viscera on close fatal shotgun hits |
| Pinky | 3 | 1 | 4 / 7 | 4 / 9–11 | Red viscera |
| Cacodemon | 5 | 1 | 6 / 9 | 6 / 11–12 | Blue viscera |

¹ Values are requests before capacity. The upper explosive count applies only when the engine picked an extreme death state.

Rocket collision damage is held until the rocket's radius damage completes. The bounded explosion scopes coalesce direct and radius damage by victim, and each barrel radius invocation gets its own sequence. Only real HP loss qualifies. A surviving victim needs 15 HP of observed damage. Lethal and extreme results get priority, and up to 24 admitted pieces are shared among the event's victims. These hooks read existing damage and death outcomes without modifying actor, save, or damage data.

Dry-floor B poses last two seconds for medium hits and 15–26 seconds for substantial pieces. They use a low upright silhouette with its base above the blood-decal lift. Viscera can leave a wall smear and then fall to the floor. Flight and rest share the 64-piece pool, including procedural fragments, with 24 resting slots and oldest-resting eviction. The existing 768-drop pool and three-wall-mark quota are shared. Major contacts can submit up to three extra floor probes inside a per-event floor quota of four, six, or nine. Each probe starts near a real floor impact and uses the same collision sweep. Gore submissions use existing decal capacity without evicting existing gun marks. Floor pieces and verified floor stains work with Enhanced Wall Blood off. In solid-map mode, pieces can fly near edits, and uncertain contacts end without claiming an original surface.

## Changed files

- `duct-tape-doom-main/linuxdoom-1.10/{p_map.c,p_mobj.c,p_weakpoints.cpp,p_weakpoints.h}` adds cosmetic capture at real damage boundaries.
- `duct-tape-doom-main/platform/scene3d/{wounds.cpp,gore_policy.h,spray.cpp,internal.h,scene.cpp}` selects the presentation, manages per-explosion scopes, and connects renderer lifecycle.
- `duct-tape-doom-main/platform/{gore_giblets.cpp,gore_giblets.h,scene3d/giblet_scene.cpp,scene3d/blood.cpp}` extends the existing pool, grounded rendering, verified contacts and decal admissions.
- `duct-tape-doom-main/platform/settings_panel.cpp` updates F4 help. `launcher/settings-schema.xml` records the resulting source hash.
- `duct-tape-doom-main/tests/{critical_smoke.cpp,gore_giblets_foundation_tests.cpp,wounds.cpp}` covers real damage and visual routing, fixed-step persistence, and tier and anatomy gates.

## Verification

Windows Release built successfully. The four focused CTest cases, the stock Doom II wound checks, the atlas check for all 32 images, and the launcher schema comparison passed. The native fixture passed 80 G1.2 checks in standard mode, 84 in solid-map mode, 15 in procedural fallback mode, and 4,699 checks in the general regression run. Healthy hits produced the close-routine counts in the roster table for all seven types. The distance boundary, zero damage, opaque misses and option gates were also checked.

```powershell
ctest --test-dir build -C Release -R 'gore-giblet-(foundation|art)|wound-(hit-placement|animation-attachment)' --output-on-failure
.\build\Release\doom-wounds-tests.exe .\test-references\DOOM2.WAD
python tools/pack_giblets.py --check
```

The native executable was built in `build/g12-smoke/out` against the Release engine libraries. It ran with `<DOOM2.WAD> <critical-arena.wad> <existing-output-directory>` in an isolated working directory. The extra flags were `--gore-g1`, `--gore-g1-solid`, `--gore-m21`, and no flag for the general regression run. The geometry fixture can be generated with `python duct-tape-doom-main/tests/ragdoll_arena.py imp build/critical-arena.wad`.

| Native fixture | Requested / admitted | Submitted quads | Floor / wall contacts | Floor / wall marks from G1.2 contacts |
| --- | --- | ---: | --- | --- |
| Each verified human headshot | 6 / 6 | 6 grounded | 19 / 0 | 2 / 0, after reserved light-blood floor slots |
| Rocket splash kills Cacodemon | 11 / 11 | 11 blue flight + 11 grounded | 29 / 0 | 6 / 0 |
| Same rocket directly hits and splashes Mancubus | 5 / 5, one coalesced event and 148 HP loss | 5 flight + 5 grounded | 17 / 0 | 4 / 0 |
| Cacodemon beside solid wall | 6 / 6 | 8 across sampled views | 0 / 6 | 0 / 3 |

Two same-tic barrels admitted five pieces each under distinct explosion IDs. Three lethal rocket victims requested 30 pieces and admitted 24. The 35-tic, 64-piece stress run took 0.69 ms for 3,650 fixed-step sweeps in standard mode and 0.78 ms for 3,330 in solid-map mode. The older G1 notes recorded 0.62 ms and 0.83 ms, respectively. The solid-map G1.2 fixture includes earlier blast edits, and these are headless update times rather than rendered frame times.

## Windows playtest

From the repository root, use a separate working directory for the settings and debug log.

```powershell
cmake --build .\build --config Release --target doom
$game = (Resolve-Path .\build\Release\doom.exe).Path
$iwad = (Resolve-Path .\test-references\DOOM2.WAD).Path
$run = New-Item -ItemType Directory -Force .\build\g1-2-playtest
Push-Location $run.FullName
try {
    & $game -iwad $iwad -warp 5 -skill 3 -goredebug 2>&1 | Tee-Object gore-g1-2.log
} finally {
    Pop-Location
}
```

1. Enable **Blood**, **Advanced Gore → Enhanced**, **Flying flesh fragments**, **Body destruction & viscera** and **Enhanced wall blood**. Use stock Doom II sprites and a map with fewer than 200 countable monsters. Add `-solidmaps` on the second pass to compare intact and excavated surfaces.
2. On MAP05, shoot a healthy Cacodemon at close range, then repeat from medium range. Compare blue pieces over dry floor and near an intact wall. Let a fatal burst settle. Check that the pieces remain visible above the stains for around 15 seconds and repeat with Wall Blood off.
3. On MAP02, enable **Head destruction** and turn **Death Ragdoll** off. Use close fatal headshots on an Imp, a Zombieman and a Shotgun Guy. The Imp and Zombieman should reach their stock corpse poses. Check for red textured pieces, a single `brain_like` event for each human, and a head mask on the Shotgun Guy.
4. Use a rocket on a Mancubus, splash a Cacodemon beside the impact, and set off two nearby barrels. Check for a single victim record per explosion, `dedupedDirectSplash=1` when both rocket components damaged one victim, and separate explosion IDs for barrels. Compare open floor, an intact wall, a liquid and an opened passage. Repeat at 30, 60 and 120 FPS and reload to check debris cleanup.

The optional G1 surface sprite artwork stays reserved. This milestone uses the existing procedural decals and runtime-only debris. A Windows Vulkan capture verified that the revised grounded sprites render above blood after a rocket hit. Frame-rate comparisons across gameplay setups still need a playtest. M3 is deferred.

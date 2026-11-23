# HellForge Gore M2

Shotgun and SSG gore resolves once per victim after `P_WeakpointEndShot()`. The renderer uses the health lost during each resolved pellet, including lethal overkill. Pellets that pass through a dead victim and synthetic weakpoint executions at shot end contribute no contacts or damage. The M1 human head majority keeps its original denominator of admitted pellet contacts. New anatomy thresholds use confirmed opaque sprite contacts.

| Monster | Enabled when eligible | Art fallback |
| --- | --- | --- |
| Zombieman | M1 head burst, chest or belly cavity with tissue | M1 heavy impact or tissue with ordinary death art |
| Shotgun Guy | M1 head burst, chest or belly cavity with tissue | M1 heavy impact or tissue with ordinary death art |
| Imp | Head burst, chest or belly cavity with tissue | Flesh or tissue with ordinary death art |
| Revenant | Skull burst and pale bone shards | Bone shards for rib impacts or unsupported skull art |
| Mancubus | Chest or belly cavity with heavy tissue | Heavy tissue with ordinary death art |
| Pinky | Torso-centered viscera | Ordinary corpse geometry |
| Cacodemon | Severe close-range blue viscera with trailing lobes | Ordinary corpse geometry |

Each corpse cut requires a complete matching death sequence. Compared with M1, a replacement in any later frame now prevents the head cut from being applied at all. The Cacodemon requires four opaque contacts, 70 observed damage and a range of at most 96 units. Pinky requires three opaque contacts, a torso concentration and 50 observed damage. Mancubus requires three confirmed hits in one torso region and 60 observed damage. Gore state is runtime-only and cleared on level transitions.

## Verification

- MSVC Release `doom-wounds-tests` and `doom-wound-attachment-tests` rebuilt. CTest passed `wound-hit-placement` and `wound-animation-attachment`. The wounds test covers shot-end timing, duplicate callbacks, removal, level changes, Spectre exclusion, region policy, and range, population and damage boundaries.
- The headless F4 navigation executable passed 1,994 checks against the local Doom II IWAD, including both new options under Combat / Gore and the option registry write/read round trip.
- `doom_core` rebuilt. The changed game renderer units `scene.cpp` and `spray.cpp` compiled individually with the Release game settings.
- Decoded and matched 38 named death patches against the local Doom II IWAD. The human and Imp patches also matched byte-for-byte in the local Doom IWAD. Reviewed static previews of the Imp and Revenant head masks.
- A complete game build and live GPU visual checks remain pending. The frame fingerprints establish artwork identity. In-game framing, effect frequency, and the appearance of procedural falls still need review.

## Windows playtest

Run from the repository root after building the local Release game. The scratch working directory keeps the test graphics profile separate.

```powershell
$game = (Resolve-Path .\build\Release\doom.exe).Path
$iwad = (Resolve-Path .\test-references\DOOM2.WAD).Path
$run = New-Item -ItemType Directory -Force .\build\gore-m2-playtest
Push-Location $run.FullName
try {
    & $game -iwad $iwad -warp 2 -skill 3 -goredebug 2>&1 | Tee-Object gore-m2.log
} finally {
    Pop-Location
}
```

1. In F4, open **Combat / Gore** and enable Enhanced, Head destruction, Body destruction & viscera, Flying flesh fragments, and Enhanced wall blood. Enable Blood and the accelerated renderer. Start with Death Ragdoll off. Use `IDKFA` to obtain both shotguns if needed.
2. On MAP02, use the normal shotgun within 128 units for Zombieman, Shotgun Guy and Imp head and torso tests. Use separate targets for each region. Follow the death animation from front and side with procedural falls on and off. Repeat with Death Ragdoll enabled and look for the particle fallback.
3. Relaunch with `-warp 10` for Revenant skull and rib impacts, `-warp 7` for Mancubus belly and chest, and `-warp 5` for Pinky, nearby Cacodemons and Spectres. Weaken sturdy targets before the qualifying shotgun/SSG discharge. For the Cacodemon, compare a severe shot within 96 units against a similar shot outside it. Compare head versus torso on the Revenant. Pinky and Cacodemon should retain the stock corpse silhouette.
4. Repeat with a pistol, nonlethal hits, diffuse spread, and a replacement sprite PWAD. Check the log for one `GORE M2` event per eligible victim/discharge and `art=uncut` on applicable fallbacks. On replacement art, check both the first death frame and a replacement limited to a later frame. Test voxel presentation, demos, multiplayer and the 199/200 population boundary with the same weapon and settings.
5. Toggle Classic, Blood, Head destruction, Body destruction & viscera, Flying flesh fragments and Enhanced wall blood independently. Repeat with Wound Holes and Critical Parts off and on. Save F4 settings, restart and inspect `gore_body_damage` and `gore_limbs` in the scratch `graphics.cfg`. Confirm that loading a save, resurrection, level change, and renderer shutdown clear prior corpse cuts and pending events.

## Limb and rib mask authoring

Limb loss needs reliable left/right labels on living rotations, including mirrored views, and matching source-texel segments for the death and corpse frames. The existing human rigs do not identify those segments throughout the death animation. Revenant rib cuts need a verified rib-only region that excludes shoulder hardware and still reads correctly when the corpse collapses. Until those masks exist, the limb option is persisted with no cutting path and rib impacts use bone shards.

To author the remaining data, decode the sprite posts from a local IWAD to row-major palette/coverage pairs. Record lump name, width, height, left/top offsets and FNV-1a hash as in `gore_stock.h`. Draw candidate region overlays on each living rotation, record handedness in original patch coordinates and check the mirrored versions. Draw each death-frame segment separately, preview the original-texel cutout against the matching gap, and reject any sequence with an overlapping weapon, ambiguous side, or missing intermediate mask. Diff or export only fingerprints and mask coordinates.

## Source changes

- Engine callbacks and cleanup in `linuxdoom-1.10/{p_enemy.c,p_map.c,p_mobj.c,p_pspr.c,r_main.c,r_state.h}`.
- Policy, stock data, effect resolution and rendering in `platform/scene3d/{gore_policy.h,gore_stock.h,internal.h,scene.cpp,spray.cpp,wounds.cpp}`.
- F4 bindings in `platform/{scene3d.h,settings_layout.h,settings_panel.cpp}`.
- Targeted checks in `tests/{settings_navigation.cpp,wounds.cpp}`.

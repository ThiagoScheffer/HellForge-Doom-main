# HellForge Gore M2.1

Gore resolves once per victim after each player shotgun or SSG discharge. It uses observed pellet health loss, including lethal overkill, and confirms opaque impact pixels before applying new anatomical effects. The renderer uses `mobjinfo.spawnhealth` as the HP reference because actors have no separate maximum-HP field. All effects require Enhanced Gore, Blood, the accelerated renderer, a range of at most 128 units, and fewer than 200 map kill-count monsters. Multiplayer, demos, voxels and Spectres are excluded.

| Monster | Living trigger | Fatal trigger |
| --- | --- | --- |
| Zombieman and Shotgun Guy | At most 50% HP and one confirmed damaging torso contact | M1 head burst with at least one head contact, at least half of the admitted contacts on the head, and a verified death sequence. Other qualifying kills emit red flesh and splatter. The stock M1 splatter still works with Body damage off. |
| Imp | At most 50% HP and one confirmed damaging torso contact | At least one opaque contact and positive damage for tissue. A non-extreme head majority can select the verified head burst. |
| Revenant | At least 30 damage and a confirmed skull or torso contact for pale bone | Same bone threshold. A non-extreme head majority can remove the skull when the full death sequence matches. |
| Mancubus | At most 50% HP and one confirmed damaging torso contact | At least 35 damage and one opaque contact for heavy tissue. |
| Pinky | At most 45% HP, at least 20 damage and a confirmed torso contact | At least 30 damage and a confirmed torso contact for a stronger red accent. |
| Cacodemon | At most 50% HP, at least 20 damage and two opaque contacts, or 30 damage and one opaque contact | At least 25 damage and one opaque contact for a stronger blue accent. |

Zombieman, Shotgun Guy, Imp and Mancubus living injuries and Pinky and Cacodemon predeath bursts are once per life. A qualifying fatal burst can follow. Living cavities use 64 fingerprinted front and quarter patches, including mirrored views. A small opening inside the torso has a bloodied edge and contributes drips when Blood spray is enabled. Unsupported or replacement frames keep their ordinary appearance. The living state clears at death, resurrection, removal and level reset. Head removal uses the existing verified sequences for the four eligible species. Ragdolls, extreme deaths and mismatched sequences retain their normal geometry. There are no fatal torso overlays, rib cuts or limb cuts. Gore state is runtime-only across save/load.

Cacodemon effects previously required death, four opaque contacts, 70 damage and 96-unit range. The new living and fatal gates are independent. Blue cores use the existing species tint, smooth ballistic flight, three lobes and up to two trailing dots. Airborne and landed effects share the existing tint and decal code. Each event admits up to 3 extra wall marks. The shared budgets remain 64 fragments and 768 drops. Heavy bone can rest briefly at a valid floor landing. With `-goredebug`, each captured victim/discharge reports pre/post/reference HP, damage, contacts, distance, style, art and outcome. Reasons include `health_gate`, `damage_gate`, `opaque_gate`, `range_gate`, `feature_off`, `already_emitted` and `emitted`, with `art_unverified` recorded for the art fallback.

## Verification

- Release wound and attachment tests passed. The local Doom II IWAD checks verified the half-health wound, bloodied opening and drip for all four monsters on stock front and mirrored quarter frames, replacement rejection, cleanup on death, frontal Pinky admission, Spectre exclusion and separate Cacodemon live/fatal events.
- Release `doom-critical-smoke --gore-m21` passed 14 checks in each of three runs with the Doom II IWAD. In both the open and near-wall cases it logged a living Cacodemon at 220→200 HP, rejected a second living ejection, then emitted a fatal accent at 24→−1 HP. It checked blue geometry from two viewpoints and wall-mark counts in both layouts. The existing critical integration suite also passed all 4,699 checks.
- F4 navigation and persistence passed 1,987 checks. The regenerated launcher catalogue exposes Body destruction & viscera and omits the unsupported limb control.
- The headless SDL driver reported no supported GPU backend. The smoke checked CPU-generated geometry and wall collision. Live GPU presentation, decal appearance and visual tuning remain for Windows playtesting.

## Windows playtest

Build the Release game. From the repository root, run the following in PowerShell to use an isolated profile and capture the debug log.

```powershell
$game = (Resolve-Path .\build\Release\doom.exe).Path
$iwad = (Resolve-Path .\test-references\DOOM2.WAD).Path
$run = New-Item -ItemType Directory -Force .\build\gore-m2-1-playtest
Push-Location $run.FullName
try {
    & $game -iwad $iwad -warp 5 -skill 3 -goredebug 2>&1 | Tee-Object gore-m2-1.log
} finally {
    Pop-Location
}
```

1. In **F4 → Combat → Gore**, set Enhanced and enable Head destruction, Body destruction & viscera, Flying flesh fragments and Enhanced wall blood. Enable Blood and Blood spray. Turn Wound Holes off initially to isolate the critical overlays.
2. On MAP05, weaken a Cacodemon with a pistol and use a close shotgun hit to cross half health while leaving it alive. Confirm `fatal=0 outcome=caco_blue_viscera reason=emitted`, blue lobes and trail from front and side, and blue marks where pieces hit. Repeat in an open room and beside a wall. Compare against a qualifying fatal hit and repeat with a Pinky aimed high at the shoulders. Both corpse silhouettes should stay stock.
3. Use MAP02 for human and Imp living wounds and MAP07 for the Mancubus. Land a torso shot that leaves the target at half health or lower, pause before the killing hit, and check the opening and blood from the front or an angle. Check the fatal head and torso effects separately. Use MAP10 for Revenant bone color, floor landing and the verified skull result. Toggle Death Ragdoll and try a replacement sprite PWAD.
4. Repeat with Classic or Blood off, then toggle the Gore body, fragment and wall settings individually. Restart to verify `graphics.cfg`, reload a save, and test resurrection and map changes. Compare the 199/200 population boundary on a fixture or in the debugger.

## Modified source

- `platform/scene3d/gore_policy.h` defines the trigger rules. `gore_stock.h` records stock head and living torso landmarks. `wounds.cpp` captures and resolves the discharge, owns the once-per-life state and merges verified masks. `spray.cpp` presents the styles through the existing ballistic and decal budgets. `internal.h` carries the shared declarations.
- `linuxdoom-1.10/{p_map.c,p_inter.c,p_mobj.c,p_enemy.c,r_state.h,r_main.c}` pass observed post-hit HP and clear presentation at death, removal and resurrection.
- `platform/{scene3d.h,settings_panel.cpp,settings_layout.h}` and `launcher/settings-schema.xml` update the settings interface. `launcher/LauncherUpdateTests.cs` checks the embedded Gore options.
- `tests/{wounds.cpp,critical_smoke.cpp,settings_navigation.cpp}` cover the policy, stock artwork, engine discharge and settings.

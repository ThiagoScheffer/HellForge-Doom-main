# Sprite critical parts

Enable **F4 > Combat > Critical parts** for sprite-based human headshots. The option defaults off and is saved as `critical_parts` in graphics settings.

| Profile | Monsters | Critical part | Multiplier |
| --- | --- | --- | --- |
| Human | Zombieman, shotgun guy, chaingunner, Wolfenstein SS | Head | 2x |
| Other | Other monster types | Controlled by the separate enhanced-combat switches | See extended matrix |

Player pistol, chaingun, shotgun and super-shotgun bullets use opaque sprite pixels for these four types. Each pellet is classified separately. Shots through transparent artwork continue toward targets or surfaces behind it; walls, ceilings and floors still block shots. Visible artwork outside the original movement radius or collision height can be hit.

Heads are estimated from the current sprite's silhouette and width, not just the actor's height. Rotations, mirrored artwork and rendered flinches use the same sprite surface as wound placement. An unreliable head estimate produces ordinary damage. The stock standing/walking, attack and pain sample covers 121 detectable head views and seven safe fallbacks; replacement artwork can have different coverage.

Wound holes reuse the classified impact and follow the existing animation attachment. The **Wound holes** option controls their visibility independently. Existing holes remain cosmetic and never make subsequent bullets pass through the monster.

This original switch leaves other monster types, enemy attacks, melee, projectiles, BFG rays and splash damage unchanged. Multiplayer and demo recording/playback bypass critical parts. Autoaim is unchanged; use mouse look for precise targeting. The original human-headshot feature adds no savegame fields or network messages.

Additional demon heads, faces, eyes, brains, weapon vulnerabilities and temporary injuries are now available through three independent, default-off switches. See the [enhanced monster combat matrix and configuration](ENHANCED-MONSTER-COMBAT.md); its optional injury/provenance save trailer is separate from the original human feature.

## Verification

`doom-wounds-tests` covers synthetic head/body classification, transparent pixels, mirrored and posed surfaces, widened sprite bounds, peek origins, voxel/type exclusions and shared wound placement. Existing wound attachment tests remain applicable.

The `doom-critical-smoke` executable uses an owned Doom II IWAD and an isolated test arena to exercise actual shot traversal and damage, including stock frames in eight rotations, wall/ceiling occlusion and mode exclusions. It uses separate settings and a dummy video driver by default:

```powershell
python duct-tape-doom-main/tests/ragdoll_arena.py zombie build/critical-arena.wad
New-Item -ItemType Directory -Force build/critical-smoke
.\build\Release\doom-critical-smoke.exe 'C:\Games\DOOM2.WAD' "$PWD/build/critical-arena.wad" "$PWD/build/critical-smoke"
```

Add `--visual` for a short 640x400 GPU run with four human enemies. It checks damage and saves `critical-before.png` and `critical-after.png` in the output directory, then exits automatically. It does not capture desktop input.

# Monster feedback, bleeding and headshot frenzy

Historical behavior for build 2026.11.9.2. [Build 2026.11.10.0](MONSTER-SURVIVAL-AI.md) supersedes flat bleeding and amber world bursts, strengthens sparks, and adds optional feeding.

Build 2026.11.9.2, 9 November 2026. Extends [the anatomical combat matrix](ENHANCED-MONSTER-COMBAT.md) without changing its damage multipliers.

## F4 ? Combat

| Setting | Default | Behavior |
|---|---|---|
| Monster Metal Sparks | On | Orange door-track spark particles at confidently classified Arachnotron/Spider Mastermind metal-leg impacts. No armor or damage change. Independent of door sparks and weak-point damage switches. |
| Headshot Feedback | Impact Effect | Off, HS Indicator, Impact Effect, Both. Amber world burst and/or 350 ms HS text. HS text works with the crosshair off. |
| Monster Bleeding | Off | Player pistol, chaingun, shotgun and super-shotgun damage can cause bleeding on four monster types. |
| Headshot Frenzy | Off | Close super-shotgun head hits can provoke ten seconds of monster infighting. Requires Enhanced Monster Weak Points. |

These additions require accelerated single-player sprites. Network games, demo recording/playback and voxel-rendered targets bypass them. Bleeding and frenzy are independent of Localized Injury Effects. Disabling a feature cancels its pending statuses; no new gameplay RNG is consumed by disabled effects. Cosmetic particles use the renderer RNG.

## Monster/status matrix

| Monster | Bleeding | Frenzy | New feedback |
|---|---|---|---|
| Existing four human enemies | No | No | Enabled original headshots |
| Imp | Yes | No | Enabled headshots |
| Hell Knight | Yes | Yes | Enabled headshots |
| Baron of Hell | No | Yes | Enabled headshots |
| Mancubus | Yes | Yes | Enabled headshots |
| Pinky | Yes | No | No HS for face hits |
| Spectre | No | No | No HS for face hits |
| Revenant, Arch-vile, Cyberdemon | No | No | Enabled headshots |
| Arachnotron, Spider Mastermind | No | No | Metal-leg sparks; brains do not display HS |
| Cacodemon, Pain Elemental, Lost Soul | No | No | Eyes do not display HS |

Bleeding follows surviving, damaging player bullets/pellets. An enabled and confidently classified anatomical critical guarantees it; ordinary hits independently roll 30% per hit. It deals 2 HP every 35 tics for 210 tics, even while stationary. Walking is 20% slower; gravity, external momentum and attack cadence are preserved. Reapplication refreshes expiry without stacking damage/slow or postponing the next tick. Moving bleeders emit sparse blood droplets and floor splats, subject to the existing blood/spray visibility controls. Hidden blood does not disable the debuff. Tick damage uses normal deaths and original player kill credit, without new critical processing, thrust or repeated pain reactions.

Frenzy needs at least one confirmed head pellet from a single super-shotgun discharge at no more than 128 horizontal map units. There is one 40% roll after all ordinary pellets, only if the target survives. One head pellet is enough; the separate Pinky execution quota does not apply. Success selects the nearest visible living monster within 512 horizontal units, including the same species, using thinker order for distance ties. It retargets if needed and ends early if no eligible visible victim remains. After ten seconds normal player acquisition resumes. Refreshing frenzy never stacks boosts; there are no speed/damage boosts.

Frenzy targets remain authoritative across chase and retaliation. Projectiles retain normal monster ownership and incidental player damage. Projectiles launched during frenzy can damage other monsters of the same species, including Knight/Baron pairs; they retain this capability after the status expires. The shooter remains immune to its own projectile. Turning frenzy off cancels that override too.

## Impact feedback and confidence

Only a confidently classified head hit receiving the enabled anatomical headshot damage bonus produces HS feedback. Body, eye, face and brain hits do not. Lethal hits retain the pre-damage region. Shotgun HUD notifications merge per discharge; nearby world bursts merge and share a bounded particle budget.

Leg sparks support player bullets/pellets and reliably localized direct plasma/rocket/BFG projectile contacts. Splash, unknown regions, misses and brains do not spark. Cosmetic classification never changes collision bounds, alpha penetration, damage or projectile explosions. Conservative stock-art masks and replacement-art fallbacks remain those of the existing anatomy system.

## Graphics configuration

Options: `monster_bleeding 0`, `headshot_frenzy 0`, `monster_metal_sparks 1`, `headshot_feedback 2` (0 off, 1 HS, 2 impact, 3 both).

| Balance key | Default | Accepted range |
|---|---:|---:|
| wp_bleed_chance | 30 | 0?100 percent |
| wp_bleed_duration | 210 | 35?3500 tics |
| wp_bleed_damage | 2 | 1?100 HP per second |
| wp_bleed_slow | 20 | 0?90 percent walking reduction |
| wp_frenzy_chance | 40 | 0?100 percent |
| wp_frenzy_duration | 350 | 1?3500 tics |
| wp_frenzy_range | 128 | 1?1024 map units |
| wp_frenzy_search | 512 | 1?2048 map units |

Tic durations use Doom's 35 Hz clock. Balance keys are config-only and validated by the existing reader. Requested numerical defaults remain unchanged; gameplay balance recommendations require separate playtesting.

## Persistence and verification

Actor-associated records store bleed expiry/next tick/source, frenzy expiry/target and launched-projectile provenance. The length-checked save extension now writes version 2 (84-byte records), retains the existing trailer marker/boundary, and reads version 1 (60-byte records) and saves without an extension. References use serialized actor indices; the native actor structure is unchanged. Death, removal, resurrection and level transitions clear statuses and stale references.

Test results and representative GPU captures are recorded in [the verification report](MONSTER-FEEDBACK-AND-STATUSES-TESTS.md). Remaining limits: sprite anatomy is conservative; uncertain projectile contacts do not produce sparks. Automated arenas and bounded GPU fixtures do not replace extended live-map balance or long-duration performance testing.

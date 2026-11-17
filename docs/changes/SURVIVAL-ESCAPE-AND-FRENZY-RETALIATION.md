---
build: "2026.11.15.6 (native rebuild; launcher package unchanged)"
updated_utc: "2026-11-17T08:22:12.396206+00:00"
status: "implemented; focused rule and real-core checks passed; visual playtest pending"
parent: "MONSTER-SURVIVAL-AI.md"
tests: "../../duct-tape-doom-main/tests/survival_update.cpp"
index: "../README.md"
---

# Survival escape, Arch-vile hunting and frenzy retaliation

Enable **F4 -> Combat -> Monster Feeding** for the survival behavior. The switch
remains default off and independent of bleeding/critical parts. Existing single-player,
sprite-target and demo/network restrictions remain in force.

## Cacodemon and Pinky

At or below 30% maximum health, a predator gets a nominal **1.2x walking multiplier**
while approaching food or searching. Native integer movement rounding still applies.
Gravity, external momentum, attack frames and damage are unchanged. Feeding still
stops at 60% maximum health; it is earned recovery rather than passive regeneration.

The predator searches for up to **10 seconds without a successful meal**. Actual
healing restarts that window; merely finding a target does not. Reachable settled
biological corpses remain preferred, followed by the existing small living-prey list.
Visible danger near a meal makes that candidate unsafe; safer locations are preferred.
Nearby visible players and a recorded attacking actor trigger bounded local retreat.
Damage interrupts eating and returns the predator to searching instead of immediately
abandoning survival. A Cacodemon still descends before eating a ground corpse.

A failed window releases the feeding target, restores a valid opponent and primes
the next native attack opportunity with `MF_JUSTHIT`. A ten-second failure cooldown
keeps it fighting rather than immediately searching again. This adds no damage bonus.
Blind floaters keep their separate corpse-recovery/random-fire behavior when no food
exists, even with ordinary feeding disabled.

## Arch-vile

An Arch-vile at or below **50% health** can hunt living biological prey through the
same feeding controller without requiring bleeding or Advanced Bleed Reactions.
It keeps its native walking speed. At a clear, native-validated approach within
**96 units**, it drains **24 HP** using normal damage/death processing. Healing is
bounded by actual HP removed, the existing 5% per-bite healing cap and the 60%
recovery ceiling. Empty or invulnerable donors cannot grant health.

Eligible live stock prey includes humans, Imps, Pinkies/Spectres, Cacodemons,
Revenants, Hell Knights, Mancubi and Pain Elementals. Players, other Arch-viles,
Barons, bosses, mechanical actors and Lost Souls are excluded. Native pain artwork
provides the short siphon action; it does not launch an ordinary Arch-vile blast
at the donor. Existing siphon feedback is reused on earned healing.

Arch-vile feeding never consumes corpses. Its normal resurrection action retains
priority; starting survival cannot interrupt an existing attack/heal/pain sequence.
The older bleed-triggered passive siphon is suppressed while active feeding owns
the actor, avoiding two recovery paths for the same action.

## Mancubus frenzy fix

Previously, only missiles launched during frenzy carried the same-species immunity
exception. A victim's normal retaliation missiles therefore could not damage the
original attacker, even while the victim kept attacking it.

Confirmed, surviving same-species hits now record the damaging aggressor. The
current hostile pair can damage each other after frenzy expiry. A launched
retaliation missile also stores its exact intended opponent, preserving that
contact through later retargeting. The native missile collision gate consults this
pair/provenance before applying ordinary same-species immunity. Shooter immunity,
missile ownership, ordinary damage and unrelated same-species immunity remain intact.
The native Knight/Baron family rule is supported alongside exact species matches.

Frenzy expiry retains an established hostile opponent instead of forcibly clearing
it; subsequent ordinary AI can still acquire another target normally. Turning off
new frenzy triggers does not make an already-established duel invulnerable again.
Death, removal, resurrection, respawn cleanup and map transitions release relevant
references. This is current-opponent bookkeeping, not an unbounded history of every
past aggressor or a global species-wide friendly-fire switch.

## Tuning

Existing feeding keys retain their meanings, including `wp_feed_start 30`,
`wp_feed_stop 60`, `wp_feed_pursuit 105`, radius/search/bite/healing settings.
The per-target pursuit remains three seconds by default and can retry within the
overall window. New validated graphics keys are:

| Key | Default | Allowed range |
|---|---:|---:|
| `wp_feed_window` | 350 tics (10 s) | 35–1050 |
| `wp_feed_speed` | 120% | 100–150 |
| `wp_feed_failure_cooldown` | 350 tics (10 s) | 35–3500 |
| `wp_feed_vile_start` | 50% | 1–50 |
| `wp_feed_vile_damage` | 24 HP | 1–100 |
| `wp_feed_vile_reach` | 96 units | 32–128 |

New searches still share the four-search-per-tic budget, 256 thinker-visit limit,
eight route-candidate limit and periodic search interval. Successful live bites
reuse their surviving prey; corpse completion returns to the budgeted search path.
Retreat tries at most five directions away from a threat, or eight directions while
searching without one. This is local collision-aware steering, not a maze pathfinder
or general projectile-dodging AI. The bounded scan can miss food outside its candidate
window; a missed/unreachable meal correctly falls back to combat after the timeout.

## Persistence and focused verification

The WP01 trailer writes **V7, 236-byte records**. It preserves the V6 220-byte prefix
and appends the remaining search deadline, danger reference, current hostile opponent
and retaliation-missile opponent. References use serialized actor indices, not pointers.
Readers retain V1–V6 support; older pursuits receive a bounded initial search window.
V7 restores the precise remaining window, including targetless searching. Raw actor
layout and normal death/boss processing are unchanged. Original older executables
cannot restore the new V7 status fields.

Built Release targets: `doom`, `doom-weakpoints-tests`, `doom-survival-update-tests`.
The combat-rule binary passed 571 checks, including old-format reading and default-off
exclusions. The focused real-core fixture passed:

- Actual Mancubus projectile contact, expiry, return fire and damage in both directions.
- Opponent-only exception, shooter/unrelated-species protection and native save/load.
- Existing-duel damage after frenzy is switched off and hostile-reference removal.
- Actual Cacodemon/Pinky retreat movement, search speed and ten-second failure behavior.
- Native corpse approach/animation/consumption and bounded recovery for both predators.
- Native save/load of targetless search without resetting its remaining deadline.
- Actual Arch-vile prey damage/healing with bleeding disabled, and feeding cancellation.

Commands, using the existing tiny disconnected-room fixture:

```powershell
cmake --build build --config Release --target doom-survival-update-tests doom-weakpoints-tests doom
.\build\Release\doom-weakpoints-tests.exe
# From build/survival-update (isolated cfg/save files):
..\Release\doom-survival-update-tests.exe ..\..\test-references\DOOM2.WAD ..\encounter-director\fixture.wad
```

Logs are under `build/survival-update`. No broad CTest suite, GPU captures or extended
WAD playthrough was run. Visual steering, crowded-map performance and animation
presentation still need in-game review. The launcher/distribution was not rebuilt;
use `Build-Hellforge-V2.bat` to package the updated native engine when ready.

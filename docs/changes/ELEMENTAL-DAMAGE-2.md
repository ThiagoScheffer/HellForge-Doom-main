# Elemental Damage 2.0, colored combat notifications and Speedrun HUD

```yaml
project: DOOM: HELLFORGE
build: 2026.11.15.1
updated_utc: 2026-11-14T12:03:23Z
status: implemented; focused verification passed
baseline_commit: d4de4c0b09d0f73fa29e1655d32f1068f4824ffc
index: ../README.md
tests: ../../build/elemental2/
```

Implemented directly in main, preserving the owner-tested [elemental/reaction baseline](ELEMENTAL-COMBAT.md). The pre-existing modified SDL/Gradle lock file was left untouched and is excluded from the release source manifest. No new dependencies, AI throttling, renderer damage writes or raw actor/save-body layout changes were introduced.

## Controls and exact damage

**F4 > Combat** adds **Elemental Damage Over Time** (`elemental_damage_over_time`) and **Colored Combat Notifications** (`combat_event_messages`). Both default **off**. Existing **Elemental Combat FX** remains independently configurable; it never enables damaging burns automatically.

| Real confirmed attack | Damaging burn targets | Chance per qualifying surviving hit | Duration | Tick damage | Uninterrupted total |
| --- | --- | ---: | ---: | ---: | ---: |
| Imp `MT_TROOPSHOT` | Zombieman, shotgun guy, chaingunner | 15% | 210 tics / 6 seconds | 3 HP each second | 18 HP |
| Mancubus `MT_FATSHOT` | Those three humans, Imp, Pinky, Cacodemon | 10% | 140 tics / 4 seconds | 5 HP each second | 20 HP |
| Player plasma rifle with recorded `WP_PLASMA_DIRECT` provenance | Ordinary shootable sprite monsters with `MF_COUNTKILL`, plus Lost Souls; excludes players, missiles, scenery and arena-protected actors | 12% | 175 tics / 5 seconds | 4 HP each second | 20 HP |

These use the existing Doom byte-RNG convention (`byte * 100 / 256 < chance`), including its quantization. There is exactly one damaging elemental stream per target. No tick occurs on application: first tick is 35 tics later, and a fresh uninterrupted status includes its final tick at 140/175/210 tics. Successful same-element reapplication refreshes duration and updates ownership/profile without stacking streams or postponing the next existing tick. A different element replaces the old status and starts a fresh 35-tic phase. Refreshes can extend the overall burn beyond one uninterrupted profile duration; they cannot bank extra immediate ticks.

Direct projectile damage, ammo, critical multipliers and firing behavior are unchanged. Imp melee, Arachnotron projectiles, BFG, scenery hits, ignored damage and cosmetic Arch-vile flames do not acquire these damaging profiles. Actual Arch-vile direct/radius fire retains its previous cosmetic behavior; this version does not add Vile fire DoT. Existing weapon rules can still kill a target directly before ignition eligibility is evaluated (for example plasma against a Lost Soul).

With DoT off, the previous cosmetic-only Mancubus/Vile/plasma path remains active only when its FX switch is on. With DoT on and full FX off, damaging statuses request minimal bounded orange/blue particles without additional lights. With both enabled, orange flame/smoke and blue energy effects reuse existing renderer particle/flash budgets. A failed damaging fire ignition does not start a misleading full cosmetic fire; plasma can retain its short harmless impact feedback.

### Cacodemon corpses and lifecycle

Cacodemon joins the existing five charred-corpse types. With **Elemental Combat FX on**, direct fatal fire/plasma or death during an active qualifying orange/blue burn retains ordinary death animation and the same corpse actor, with per-actor charcoal/grayscale appearance and a fading colored transition. Other plasma-burnable monster types retain their normal corpse appearance unless already in that six-type charred list.

Eye injuries, feeding, wound attachment, drops, kill counts, boss processing and resurrection keep their existing paths. Resurrection clears all own elemental status and charred appearance. Removal clears references safely; map changes clear all status records. Turning DoT off immediately cancels periodic damage independently of the FX preference. Unsupported modes cannot ignite, render or continue damaging statuses.

## Authoritative damage and attribution

`p_elemental.inc` extends the existing `Runtime` record in `p_weakpoints.cpp`; it does not add a second actor framework. `p_inter.c` confirms damage after armor/invulnerability/other native protections and actual HP subtraction. Existing duplicate renderer damage notifications are not used for ignition or counters.

Periodic damage runs through `P_DamageMobj` from existing actor updates. A scoped simulation flag prevents recursive ignition, critical/vulnerability multiplication, hit-flinch callbacks, pain RNG, knockback and unrelated hit-triggered reactions. Ordinary native damage protections, death actions and ownership remain authoritative. The pre-death element is captured before normal kill cleanup, so the final periodic tick can still produce the correct corpse treatment.

The sidecar retains a source reference and a safely identified player slot. A dead-but-retained monster source remains valid until removal; actual removal nulls the reference. Player ownership can safely fall back to its player slot after actor replacement. Missing monster sources become null rather than stale pointers. No actor pointers are written into save files.

Single-player/sprite exclusions remain; demos, netgames and voxel targets bypass new damaging rules. Disabled DoT consumes no extra gameplay random values. Cosmetic-only randomness remains independent. Enabling gameplay ignition intentionally consumes the normal gameplay RNG for eligible probability rolls.

## Combat notification API

`P_HudNotify(kind, text, color, priority, durationTics, eventKey)` is a small C-compatible publisher. `P_CombatMessage` supplies standard monster names/messages, and `P_HudNotices` returns at most three fixed-size records. Records carry typed kind, text, ARGB color, priority, expiration and an optional deduplication key. Text is bounded to 95 bytes and normalized to uppercase ASCII; rendering uses the existing small HUD glyph set.

| Activation | Message | Color |
| --- | --- | --- |
| Confirmed critical/weak-point hit enters bleeding or advances a tier | `CRITICAL BLEEDING - BARON OF HELL` | Yellow `#FFD75A` |
| Actual new infighting frenzy with a valid target | `FRENZY TRIGGERED - MANCUBUS` | Purple `#B78AFF` |
| Successful new floating eye injury | `EYE INJURY - CACODEMON` | Purple `#B78AFF` |
| Successful new Spider eye rage | `EYE RAGE - SPIDER MASTERMIND` | Purple `#B78AFF` |

Ordinary blood effects, persistent bleeding, failed chance rolls and refreshes of an already-active frenzy/eye rage do not publish new activation messages. Default life is 88 simulation tics (about 2.5 seconds). Same-event coalescing and a short same-text throttle limit rapid-hit spam. Lower-priority overflow is rejected; otherwise the lowest-priority/oldest slot is replaced. No unbounded queue or per-actor logging exists.

Notifications occupy the fixed upper-center overlay, separate from the widescreen-docked modern panels. This avoids splitting words at panel docking seams. They do not write `player.message`, pickup notifications or save/load text. Pause freezes their simulation-time lifetime. New map loads and feature disablement clear the queue. The render path only expires/reads the three-record queue; combat publication occurs at successful gameplay event boundaries.

## F4 > SPEEDRUN

The **Enable Speedrun HUD** master (`speedrun_hud`) defaults **off**. Each metric defaults visible behind that switch and has an independent persisted option. The anchor (`speedrun_anchor`) selects upper left/right or lower left/right. Panels avoid the crosshair and bottom status area; upper anchors move below top-docked modern panels. Existing virtual HUD coordinates provide UI/widescreen scaling without a new font/UI dependency.

| Setting key | Metric / lifetime |
| --- | --- |
| `speedrun_attempts` | Attempts in the current map visit, starting at 1. A real map restart or checkpoint load begins one new attempt |
| `speedrun_deaths` | Actual player deaths in the current map visit |
| `speedrun_killstreak` | Actual player-credited monster kills this attempt, including owned DoT; resets on death/new attempt. Posthumous damage still counts toward session damage, but cannot revive a dead attempt's streak |
| `speedrun_attempt_time` | Active simulation tics in the current attempt, including its fatal tic; frozen after death until a new attempt |
| `speedrun_runtime` | Active simulation time across the running process's gameplay session, including world tics while dead |
| `speedrun_damage_dealt` | Session monster HP actually removed by player-owned attacks/DoT; overkill, scenery, ignored damage and monster-owned damage excluded |
| `speedrun_damage_taken` | Session player HP actually lost after armor/protection; counted once from actor health subtraction |

Clocks are 35-Hz simulation gameplay clocks, **not official wall-clock RTA**. They exclude loading, paused time, menus that stop simulation, intermission and photo-mode stepping. Weapon-wheel slow motion counts the simulation tics actually advanced. Attempt timing has one-tic precision even though the label displays centiseconds.

### Session, visit and save semantics

- Counters are collected at damage/load/tic event boundaries, even while the master HUD is hidden. Hiding/showing it never resets history.
- Initial playable map entry is attempt 1. Same-map reloads/restarts increment attempts once through `G_DoLoadLevel`; ordinary saves and F4 changes do not.
- A checkpoint load starts a new attempt while retaining current session attempts/deaths and cumulative damage/runtime. Restoring old actor status does not restore old session history.
- Entering a different map starts a fresh map visit: attempts 1, deaths 0, streak/time 0. Returning to that map later is another fresh visit, not a persistent per-map database.
- Choosing New Game explicitly starts a fresh map visit, including when it selects the same map. Session damage/runtime totals remain accumulated until the engine process exits.
- Loading a save from the title screen follows those same current-process rules. A newly launched engine starts a new session; no global statistics database or persistent cross-launch history is written.
- Death and subsequent restart are separate events: death increments deaths once; the subsequent load increments attempts once. Already-dead actors cannot register another death. Revived monsters can legitimately earn another kill under ordinary engine rules.

The HUD reads cached counters; it never scans monsters. Formatted strings are cached until counter values change. Rasterization uses fixed-size buffers and seven small rows; disabled widgets return before formatting/drawing. Compact and tactical base HUDs remain separate from these controls.

## Saves and integration files

Save trailer **v6** retains the entire 196-byte v5 prefix and adds six 32-bit fields (220-byte records): remaining DoT duration, next-tick phase, normalized element, tick damage, source actor index and optional player slot. Record lengths/counts and source indices are validated; ticks/damage are bounded. A save exactly at the final pending tick preserves that tick. v1/v2/v3/v4/v5 continue loading with safe empty DoT fields; disabled settings cannot rearm them. New saves should be used with this build or newer.

HUD notifications and session counters are deliberately outside saved monster status. No raw actor, ordinary save body or map-cache format changed.

Key files:

- `linuxdoom-1.10/p_elemental.inc`, `p_weakpoints.cpp/.h`: existing status/provenance extension, periodic damage, v6 trailer and lifecycle cleanup.
- New `linuxdoom-1.10/p_combat_hud.inc`: bounded notification API and event-based session counters.
- `p_inter.c`: actual HP accounting and periodic-damage guards; `g_game.c`/`p_tick.c`: attempt/load/active-clock events.
- `platform/scene3d/modern_hud.h`, `scene.cpp`, `impact_particles.inc`: cached widgets, fixed notification composition and bounded minimal/full feedback.
- `scene3d.h`, `settings_panel.cpp`, `settings_menu.cpp`: persisted F4 controls and actual-menu test hooks.
- `tests/elemental2_rules.inc`, `elemental2_smoke.inc`, `elemental2_visual.inc`: additions to existing rule/native/renderer infrastructure. Existing fixture expectations update to v6 and the newly eligible Cacodemon.
- Launcher defaults/version and current README/index reports update to **2026.11.15.1**.

## Focused verification and measured limits

Built normal optimized Release engine and matching native/rule/wound targets using the existing CMake infrastructure, with monster profiling and benchmark build modes off.

- **568 rule assertions passed**, including 100 new checks (one former Cacodemon exclusion is intentionally removed). Coverage includes chance byte boundaries, exact tick totals/final tick, no immediate/duplicate ticks, refresh/replacement, safe ownership/removal/dead-owner attribution, disabled/mode exclusions, v5/v6 loading, final-boundary save phase, minimal FX hints, notification activations/colors/priority/limits, attempts/deaths/clocks/damage/posthumous streak behavior.
- **39 new native assertions passed** using actual DOOM2.WAD and the existing generated arena: real Imp/Mancubus/player-plasma damage paths, exact authoritative ticks, native save/load phase and player attribution, attempt/history retention, Cacodemon death/resurrection cleanup, armor/god protection, paused/stopped-menu clocks and actual player-death freeze and supported native-demo initialization without false attempt registration.
- The prior **138-check elemental native fixture passed**, including 118 SS/chaingunner head rays and five safe frame fallbacks. Existing wound-placement/classification executable passed.
- Launcher persistence/export/import passed **18 checks** with the new controls/metric/anchor keys; existing settings backup checks passed. **19 normal-release package checks passed**, including version/engine hash and default-off controls.
- **Seven new settings assertions in two processes passed**, using actual F4 Combat/SPEEDRUN control events, independent metric/anchor persistence and F10 discard.
- One dense GPU scene completed with **36 simultaneously burning actors**, both elements, a charred Cacodemon and three seeded notification types. Captures cover Classic, Modern compact and Modern tactical HUD styles. After initial captures exposed widescreen notification splitting, notifications were moved to the fixed overlay and recaptured. Captures: `build/elemental2/visual/elemental2-hud-{0,1,2}.png`.

The dense visual fixture freezes/positions actors, uses boosted health and repeated confirmed damage until chance success, and seeds messages for reproducible presentation. It is not a natural full-fight gameplay or full-map performance benchmark.

A focused HUD CPU raster microbenchmark used **nine alternating off/on pairs**, 100 calls per sample, within that scene. Disabled median/p95: **0.002 / 0.003 microseconds**; enabled median/p95: **4.418 / 8.585 microseconds**. Disabled results are at the timer/compiler floor; they do not establish zero engine overhead. Raw samples: `build/elemental2/visual/hud-widget-cost.csv`. This measures only the widget layer, excluding full renderer/GPU/upload costs. No FPS improvement, end-to-end frame-time regression bound or memory improvement is claimed.

Commands/evidence are retained under `build/elemental2/`: `verify.py`, `native-final.py`, `release-build.log`, native logs, isolated settings profiles and renderer samples/captures. Native arguments are `--elemental2`, prior `--elemental`, and `--visual-elemental2`; inputs are the owned `test-references/DOOM2.WAD` and generated `build/settings-arena.wad`. Commercial IWADs are not packaged.

A stock-demo initialization probe found that this owned DOOM2.WAD contains a version-106 demo header, which the existing loader rejects. The passing initialization check instead uses a minimal supported native director-demo fixture (version 223); no IWAD bytes or demo-format acceptance rules were changed. It does not certify a full demo playback.

Testing intentionally stayed focused. No broad alternate-IWAD/WAD campaign, network/demo playthrough, exhaustive voxel/replacement-art matrix, long-duration dense-fight memory run or full enabled/disabled frame benchmark was repeated. Compatibility is guarded in code and unit checks, not certified by an unexecuted matrix. Native headless renderer-unavailable notices and prior uncached-sound warnings remain nonfatal fixture output.

### Remaining manual checks

Playtest subjective flame/blue-glow intensity, compact text at varied resolutions/UI scales, top-docked HUD combinations, all four anchors, sustained natural fights, unusual custom/dehacked actors, save files from external releases and demo/LAN exclusions. New short tests do not replace that coverage. All three new master controls default off, and no unrelated advanced-reaction balance was changed.

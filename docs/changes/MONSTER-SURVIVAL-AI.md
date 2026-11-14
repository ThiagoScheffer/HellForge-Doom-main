# Combat feedback and monster survival AI

```yaml
project: DOOM: HELLFORGE
build: 2026.11.14.1
updated_utc: 2026-11-13T19:16:57Z
status: active
evidence_build: 2026.11.12.1
tests: MONSTER-SURVIVAL-AI-TESTS.md
index: ../README.md
```

Build **2026.11.10.0**, 10 November 2026. Incremental extension of [the existing anatomical combat matrix](ENHANCED-MONSTER-COMBAT.md) and [the previous feedback/status release](MONSTER-FEEDBACK-AND-STATUSES.md).

## Controls and compatibility

F4 / Combat retains all existing switches and the **Off / HS Indicator / Impact Effect / Both** selector. **Headshot Sound** is new and defaults on; Feedback Off suppresses sound as well as visual notifications. **Monster Feeding** is new and defaults off. Monster Metal Sparks remains on; Monster Bleeding remains off. Bleeding, feeding, localized injuries and frenzy have independent switches.

All additions retain single-player, sprite-only eligibility. Voxel-rendered actors, network games and demo recording/playback bypass the new mechanics. Existing human headshots, anatomical damage multipliers, weapon vulnerabilities, frenzy projectile provenance, infighting and world collision remain intact. Cosmetic settings do not consume the gameplay RNG. Hiding blood suppresses blood visuals without disabling damage, slowing or feeding.

## Stronger metal impact and headshot feedback

Arachnotron and Spider Mastermind confirmed metal-leg contacts now emit brighter, larger, faster orange door-track particles and a small local flash. A bullet emits **14 sparks**; each qualifying shotgun pellet can emit **8**, capped at **32 per target/discharge**, **128 per game tic** and the existing global **512-debris** limit. Lifetime is **0.15–0.35 seconds**, with **1.75×** default size. Directions include outward, lateral and downward scatter. Door sparks keep their original tuning.

Sparks can make one short precomputed bounce against valid ordinary sector floors/walls. Floor bounces require matching landing-floor geometry. Arbitrary solid-world meshes retain the ballistic spark effect without an inferred ricochet. Brain hits, unknown regions, splash, misses and unlocalized projectile contacts do not spark. These effects do not grant armor or change damage, alpha collision or penetration.

A confirmed head hit receiving an enabled anatomical bonus now uses a **blood-colored burst**, approximately **1.75×** the previous effect, through the existing blood renderer. Monster palette tint is preserved: Knight/Baron green, other eligible head profiles red unless their established palette says otherwise. Lethal hits use the captured pre-death intersection. HUD HS text remains amber, lasts **350 ms**, and works with the crosshair disabled.

The new headshot confirmation is an **original, procedurally synthesized 120 ms sample**, integrated into the existing sound bank/mixer. It honors SFX volume and mute, preserves ordinary monster sounds, and requests at most one confirmation per discharge. Eye, face, brain, unknown and cosmetic-only contacts cannot trigger it. The standalone sound toggle works independently of blood visibility, but Feedback Off silences it.

Head blood can add at most **three new wall marks per impact** within **64 map units**. Rays use the first solid wall in the spray direction, validate texture and height, reject sky/missing surfaces and check solid-world occlusion. Directional droplets share this wall budget. Existing normal-hit blood has its own unchanged behavior. Wall/floor decal limits and level cleanup remain shared with the original renderer.

## Progressive bleeding

Eligibility remains **Imp, Hell Knight, Mancubus and Pinky**, from confirmed player pistol/chaingun bullets and shotgun/super-shotgun pellets only. Ordinary hits independently roll the existing **30%** chance. Enabled, confidently classified anatomical criticals guarantee activation. Ordinary bleeding works with enhanced weak-point collision disabled. Projectiles, splash, melee and damage ticks cannot activate it.

| Tier | Damage each second | Duration after successful application | Walking reduction | Additional attack opportunities |
|---|---:|---:|---:|---:|
| 1 — Wounded | 2 HP | 6 s | 20% | None |
| 2 — Heavy bleeding | 3 HP | 8 s | 20% | Approximately 15% |
| 3 — Hemorrhage | 4 HP | 10 s | 20% | Approximately 25% |

The first successful application starts tier one. Successful later discharges raise severity by one, capped at tier three. Severity can increase at most once per monster per discharge, even when many pellets activate bleeding. All ordinary per-hit activation rolls are preserved. A successful application refreshes that tier's duration and never postpones the next damage tick.

Aggression grants deterministic extra passes through existing chase/attack-decision gates: 15 or 25 per 100 eligible chase checks. It does not shorten animations, move projectile launch frames, alter damage, accelerate walking or steal targets. Some attacks, such as Pinky melee already attempted whenever in range, have no additional gate to bypass. It is an opportunity modifier, not a guaranteed equivalent increase in total damage output. Native tactical/aggressive movement retains the 20% walking penalty.

Tier two emits heavier droplets and occasional pain vocals; tier three adds more frequent/larger loss and limited pooling under stationary monsters. Vocals are cooldown-limited (3 s / 2 s by default) and never restart an animation. Visual blood uses existing budgets/settings. Tier-three pooling is bounded floor-splat accumulation, not a new fluid simulation. Bleed ticks preserve normal deaths and original player attribution, including after source-body replacement and save/load. Disabling bleeding immediately cancels damage, severity, slowing, aggression and vocal timers.

## Feeding behavior and monster matrix

| Monster/group | Progressive bleed | Feeding predator | Eligible living prey | Eligible corpse |
|---|---|---|---|---|
| Imp | Yes | No | Yes | Yes |
| Zombieman, shotgun guy, chaingunner | No | No | Yes | Yes |
| Wolfenstein SS | No | No | No | Yes |
| Pinky | Yes | Yes | No | Yes |
| Spectre | No | No | No | Yes |
| Cacodemon | No | Yes | No | Yes |
| Hell Knight, Mancubus | Yes | No | No | Yes |
| Revenant, Arch-vile, Pain Elemental | No | No | No | Yes |
| Baron, Cyberdemon, Spider Mastermind | No | No | No | No — bosses |
| Arachnotron | No | No | No | No — mechanical |
| Lost Soul, decorative bodies | No | No | No | No |

A living Pinky or Cacodemon may start feeding at **=30%** of `spawnhealth`, if a valid food source is accessible and no visible living player is within **128 units**. Reachable, grounded, settled biological corpses are preferred; otherwise it selects the nearest eligible living prey within **384 units**. Ties use stable actor order. Search intervals are **35 tics**; work is limited to four new searches per tic, 256 thinker visits and eight route candidates per search.

Approaches use this fork's native collision/headroom/step checks and a clear corridor. They do not select food through blocking walls, monster-blocking boundaries, undersized openings or inaccessible floor changes. This is conservative direct-corridor access, not a new general-purpose maze pathfinder. Pursuit/action timeout is **105 tics (3 s)**; failed/interrupted/finished episodes have a **175-tic (5 s)** cooldown. Spectres are not predators. Cacodemons descend to valid feeding height instead of eating high above a ground corpse.

Existing attack frames provide the feeding action. Living prey receives a normal **16 HP bite**. Healing occurs only after actual damage, capped at both the victim's HP actually lost and **5% of the predator's maximum HP** (integer floor). A completed corpse eating animation heals **10% of maximum HP**, then removes the corpse once through normal actor removal, preventing resurrection/reuse. A corpse produced by a rewarded feeding bite is marked ineligible for additional corpse recovery; that marker persists until removal/resurrection. Concurrent feeders cannot both consume the same body.

Feeding ends at **60% of normal maximum health** and never exceeds normal maximum. It is earned recovery, not passive regeneration. Nearby player threats, damaging hits, invalid/removed food, blocked approaches and timeouts safely cancel or finish the episode. Turning feeding off releases its target/overrides without undoing earned healing. Frenzy is authoritative over feeding, and bleeding aggression cannot steal a feeding/frenzy target. Ordinary infighting is unchanged.

## Configuration and save compatibility

New ordinary graphics keys: `headshot_sound 1`, `monster_feeding 0`. Existing `monster_metal_sparks`, `monster_bleeding`, `headshot_feedback` and frenzy controls retain their names.

All balance values below use the existing validated `wp_` graphics-config reader/writer and remain hidden from the F4 feature list. Durations/cooldowns are tics at 35 Hz. Existing tier-one names retain backward-compatible meaning.

| Key | Default | Accepted range |
|---|---:|---:|
| wp_bleed_chance | 30 | 0–100 |
| wp_bleed_damage / wp_bleed_duration / wp_bleed_slow | 2 / 210 / 20 | 1–100 / 35–3500 / 0–90 |
| wp_bleed_damage_2 / wp_bleed_damage_3 | 3 / 4 | 1–100 |
| wp_bleed_duration_2 / wp_bleed_duration_3 | 280 / 350 | 35–3500 |
| wp_bleed_attack_2 / wp_bleed_attack_3 | 15 / 25 | 0–50 |
| wp_bleed_voice_2 / wp_bleed_voice_3 | 105 / 70 | 35–3500 |
| wp_feed_start / wp_feed_stop | 30 / 60 | 1–50 / 51–100 percent |
| wp_feed_radius / wp_feed_bite | 384 / 16 | 64–1024 / 1–100 |
| wp_feed_live_heal / wp_feed_corpse_heal | 5 / 10 | 1–20 / 1–25 percent |
| wp_feed_search / wp_feed_pursuit / wp_feed_cooldown | 35 / 105 / 175 | 7–350 / 35–350 / 35–3500 |
| wp_feed_threat | 128 | 0–256; 0 disables proximity veto |
| wp_metal_sparks_bullet / wp_metal_sparks_pellet / wp_metal_sparks_cap | 14 / 8 / 32 | 1–32 / 1–16 / 1–64 |
| wp_metal_spark_scale / wp_head_blood_scale | 175 / 175 | 100–300 percent |
| wp_head_wall_reach / wp_head_wall_marks | 64 / 3 | 1–96 / 0–3 |

The shared trailer writes **version 3, 136-byte records**, preserving the existing boundary marker and v1/v2 prefixes. It reads v1 (60-byte), v2 (84-byte) and legacy saves without a trailer. Old flat bleeds become tier one. New state includes tier, discharge identifier, next tick/expiry, original actor/player attribution, vocal/opportunity bookkeeping, feeding phase/target/previous target/timers and no-double-recovery corpse metadata. Actor references use serialized thinker indices and player attribution has a bounded player-slot fallback. The native serialized actor layout is unchanged.

Death, removal, resurrection, respawn and level transitions clear relevant status/reference state. Runtime targets are released before vertical movement can follow removed food. No UI toggle adds gameplay random draws. See [verification, captures, changed-file inventory and limitations](MONSTER-SURVIVAL-AI-TESTS.md).

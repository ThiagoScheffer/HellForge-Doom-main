# Elemental combat and advanced monster reactions

```yaml
project: DOOM: HELLFORGE
build: 2026.11.14.3
updated_utc: 2026-11-14T10:02:57Z
status: implemented; focused rules/native/settings checks and one renderer scene passed
baseline_commit: f77c754c1d0b7c7bd020e10ac250d759ef6ff3f5
index: ../README.md
```

Implemented in the current main checkout, preserving unrelated source and gameplay systems. The two new **F4 > Combat** switches default off and persist through normal apply/save/export/import:

- **Elemental Combat FX** (`elemental_combat_fx`): cosmetic fire/plasma feedback and charred corpses. Burning never adds damage.
- **Advanced Monster Reactions** (`advanced_monster_reactions`): bleeding-based Mancubus stagger, Baron aggression and Arch-vile siphoning. Requires the existing **Monster Bleeding** switch for its underlying status/eligibility. It does not require Eye Injuries, Headshot Frenzy or Localized Injury Effects.

Both retain accelerated single-player sprite restrictions. Network play, demos and voxel targets bypass the expansion. Existing switches retain their own controls. Turning off the corresponding feature clears active expansion state; turning off bleeding clears pending reactions. The normal launcher retains its solid-world backend and default-off performance experiments.

## Audit and final behavior

| Requested system | Audit finding | Final implementation |
| --- | --- | --- |
| Spider/Arachnotron metal legs | Both already had verified sprite masks, cosmetic-only localization and shared door-track sparks | Preserved; existing leg gates and bounded budgets rechecked. No armor or multiplier changes |
| SS and chaingunner heads | Both were already included in the human classifier and independent 2x damage path | Preserved; actual bullet and shotgun-pellet rays verify exactly 2x across supported walking/attack/pain art, rotations and mirroring |
| Mancubus bleeding | Existing progressive bleeding and red blood were present | Adds two visible drops per existing emission, sharing the 96-drop global budget. Damage and health pool remain unchanged |
| Mancubus instability | Missing | Tier 2+ bleeding and a meaningful player damage event (at least 5 final damage) can roll 15%; 14-tic pain reaction, 210-tic minimum cooldown. Attack-state checks avoid introducing a stagger into a live volley; ordinary vanilla pain remains intact |
| Baron bleeding | Green blood rendering existed; progressive-bleed eligibility was missing | Added beside Knight under Monster Bleeding, retaining player bullet/pellet-only triggers and existing tier rules |
| Baron wounded aggression | Missing | Tier 2+ or health at/below 40% starts 280 tics. Walking is 110% of its current speed (including existing bleeding slowing). No repeated overlapping bonus, no refresh while active, no projectile damage change |
| Arch-vile bleeding | Missing | Added to the same existing player bullet/pellet eligibility |
| Arch-vile siphoning | Missing | Bleeding, injured, compatible living Vile can channel for six seconds, up to 12 actual donor HP each second; maximum 72, capped at configured spawn health. Cooldown 700 tics after completion/interruption |
| Fire provenance | No explicit confirmed elemental context | Actual `MT_FATSHOT` projectile contacts; actual Arch-vile direct/radius attack bracket. No inference from selected weapon or attacker type alone |
| Fire ignition | Missing | Independent cosmetic 10% hash chance on damaging fire contacts; one attempt per victim across a Vile direct/radius attack, maximum 256 tracked victims per attack. Survivors burn visibly for 105 tics |
| Player fire feedback | Normal damage flash/death flow existed | Retains ordinary protected damage flash, uses existing universal player-pain audio for actual fatal fire, avoids camera-attached flames. No new sound assets |
| Charred corpses | Missing | Ordinary deaths/corpses remain; dark grayscale immutable sprite variants selected per actor. Brief smoke/embers; resurrection forget clears the status |
| Plasma rifle | Existing explicit player projectile provenance | Short blue energy feedback; eligible direct plasma kills select the same charred corpse with blue transition. BFG remains distinct |

Ignition/charred eligibility is explicitly **Zombieman, Shotgun Guy, Chaingunner, Imp and Pinky**; player fire feedback is handled separately. SS, Spectre, bosses and other monster types do not acquire charred appearance. A qualifying monster killed directly by fire always receives fire-death visuals. Death from another cause while cosmetic burning remains active also chars it. Actual player fire-death audio is fire-specific, not merely death while an old cosmetic status remains active.

Existing weapon/anatomical multipliers, human 2x headshots, ordinary death states, drops, kill accounting, boss splash immunity and encounter triggers remain in their normal pipelines. No burning damage-over-time, fire damage increase, new actor thinkers, raw actor layout change or shared sprite recoloring was added.

### Reactions and siphoning safeguards

Baron attack cadence uses the existing deterministic opportunity accumulator. The additional 10% opportunity is combined using the **larger** existing bleed opportunity, never added to its 15/25% tiers. Existing frenzy suppresses the Baron movement/rage modifier. This is not a guaranteed 10% increase in overall shots or DPS; animations and launch frames remain unchanged.

Siphoning leaves attacks and resurrection separate. Attack, pain and resurrection states are incompatible and interrupt a channel; no normal Arch-vile damaging attack is called for healing visuals. Missing donors provide no healing. The Vile does not exceed its original configured maximum health and takes its normal bleeding damage while healing.

Living donors are explicitly limited to the five small organic types above. They must be alive, shootable, sprite-eligible, within 192 horizontal units and visible. Bosses, mechanics, arena-protected actors and unrecognized death-action chains are excluded. The actual normal damage routine removes donor HP with the Vile as source; only actual removed HP (capped to 12 and the Vile's missing health) is healed. Depleted donors follow ordinary death/kill processing.

Each search visits at most **128 BLOCKMAP entries** and performs at most **16 LOS checks**, with **four searches per simulation tic globally**. It chooses the nearest eligible visible candidate encountered within those limits. Channel transfers/searches run at most once per second; donor pointers do not persist across ticks or saves. Dense blocks may conservatively miss an available donor.

When no living donor is found, a completed ordinary corpse may provide at most 12 HP **once**, via supported actor removal. Unfinished death animations, players, bosses, protected actors and unsupported death actions are excluded. Corpses in normal resurrection reach are reserved. A bounded local guard also reserves bodies near another living Vile; uncertain dense competition rejects consumption. Existing actor removal and renderer lifecycle clear status/wound/body references.

**Tradeoff:** consuming a corpse removes that actor and prevents its later resurrection. Ordinary resurrection has priority in reserved areas, and charred corpses otherwise remain ordinary resurrectable actors. No memory-saving claim is made.

## Integration and persistence

- `p_weakpoints.cpp` / new `p_elemental.inc`: shared actor status table, bounded timers, reactions, event provenance, cosmetic ignition and siphon accounting.
- `p_inter.c`: one new confirmation after actual health subtraction/protection, retaining pre-death burn identity across normal kill cleanup. The pre-existing duplicated renderer damage callbacks are not used for new rules.
- `p_enemy.c`: explicit actual Vile fire-attack context, normal universal player-pain death audio, compatible-state checks and bounded native BLOCKMAP donor search. Existing resurrection cleanup clears charred state.
- `scene3d/impact_particles.inc`, `images.cpp`, `geometry.cpp`, `spray.cpp`: capped colored particles/flashes, immutable palette-derived charred variants, per-actor selection and stronger Mancubus blood feedback.
- `scene3d.h`, `settings_panel.cpp`, `scene.cpp`, `launcher/graphics.cfg`: two independent persisted default-off controls and runtime cancellation.
- `tests/elemental_rules.inc`, `elemental_smoke.inc`, `elemental_visual.inc`: focused boundary, native and renderer fixtures; existing eye/save tests now accept the new trailer version.

Save trailer **v5** retains the entire 160-byte v4 prefix and appends nine 32-bit fields (196-byte record): burn expiry/type, charred flag, stagger expiry/cooldown, Baron expiry and siphon expiry/next tick/cooldown. Entries use the existing serialized actor order and length checks. Loaders continue reading v1/v2/v3/v4; old saves initialize new fields empty. Current disabled settings cannot rearm effects. Open new saves with this build or newer; older engines are not certified to restore v5 statuses.

Charred appearance clears on resurrection and actor removal; map reset clears statuses and attack-local context/budgets. Transient particles carry positions, not actor pointers. Cached derived artwork follows the image/map cache lifecycle. Ragdoll cards use a dark per-actor vertex treatment; they preserve their original captured coloration rather than full grayscale conversion.

Cosmetic ignition uses an independent bounded hash sequence; effects use existing renderer randomness. Disabled gameplay additions consume no additional gameplay random values. Enabled stagger alone uses the existing gameplay RNG convention. No additional fire DOT or new bleeding tier balance is introduced. Fixed initial reaction values are the suggested defaults above; existing documented `wp_bleed_*` keys still control progressive bleeding.

## Focused verification

Normal Release engine, native fixture, rule and wound targets built successfully. Initial build exposed an existing `floorOverhang` call preceding its declaration; adding the missing forward declaration repairs compilation without changing behavior.

- **469 rule checks passed**: 35 new expansion assertions plus existing regressions. Includes eligible bleeding types, stagger cooldown/volley exclusion, non-stacking Baron speed, cosmetic RNG separation, explicit Vile direct/radius context and deduplication, exclusions, finite donor healing, missing donors, cancellation and v4/v5 save handling.
- **138 focused native checks passed** against original DOOM2.WAD and the existing generated arena fixture: **118 bullet/pellet head rays** over 59 detectable SS/chaingunner frame/rotation views, with five conservative fallbacks; both mechanical spark event gates; actual fatal Mancubus/plasma contacts; actual corpse and active-burning/channel save/load; native BLOCKMAP donor transfer; resurrection interruption; finite corpse removal; protected and fatal player fire damage.
- Existing wound classification/placement executable passed.
- Launcher persistence/export/import: **18 checks passed** with both new keys; settings backup checks passed. **16 release-package checks passed**, including matching engine/version/hash and default-off controls.
- **Six settings assertions in two separate processes** verify new defaults, normal save/relaunch, runtime application and immediate disabling with an isolated graphics config.
- **One GPU fixture passed**, captured at `build/elemental-expansion/visual/elemental-combat.png`. Charred corpses, a burning Imp and live healing Vile were inspected in one representative scene. Health/charring were asserted; this is not exhaustive visual certification or a performance benchmark.

Commands and logs: `build/elemental-expansion/verify.py`, `capture.py`, `release-build.log`, `native/elemental.log`, and `settings/`. Build via the existing CMake Release targets `doom`, `doom-critical-smoke`, `doom-weakpoints-tests`, `doom-wounds-tests`. Native fixture argument: `--elemental`; renderer fixture: `--visual-elemental`.

Native fixture setup deliberately freezes/positions some actors and uses boosted health to isolate interactions. An early corpse-transfer fixture allowed `A_Look` to enter an attack, correctly preventing healing; fixing the fixture's idle state made the intended transfer test pass. Headless renderer-unavailable notices and legacy uncached-sound warnings remain. The GPU run could not write its default external lighting-cache directory in the sandbox and correctly baked normally.

## Limits and remaining playtesting

Testing was intentionally restricted to these changes. No full WAD campaign, alternate IWAD, network/demo playthrough, repeated transition stress matrix, dense-fight performance benchmark or memory measurement was run. Compatibility exclusions and shared limits are implemented; no FPS or memory benefit is claimed. New default-off controls preserve the normal route, while enabling existing Monster Bleeding now includes Baron and Arch-vile.

Manual playtesting remains for subjective flame/smoke intensity, blue plasma visibility, ragdoll char appearance, audio feel, destruction-world LOS edge cases, multiple-Vile competition, modded artwork/actions and long encounters. Unsupported death-action chains and crowded searches intentionally decline siphoning rather than consuming uncertain actors. Cosmetic player feedback uses the normal flash and an existing pain sound; no new first-person flame overlay or custom burning audio was introduced. No optional scorch decal or donor energy beam was added.

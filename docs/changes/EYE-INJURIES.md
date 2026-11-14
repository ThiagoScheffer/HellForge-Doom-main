# Eye injuries: blindness and Spider rage

```yaml
project: DOOM: HELLFORGE
build: 2026.11.14.2
updated_utc: 2026-11-14T08:14:27Z
status: implemented; focused native and renderer verification passed
tests: ../../build/eye-injuries/
index: ../README.md
```

Enable **F4 > Combat > Enhanced Monster Weak Points**, then **Eye Injuries** (`eye_injuries`). Eye Injuries defaults **off**. Apply with Esc/F4 or Apply/close; F10 discards drafts. Existing profiles and exported/imported settings preserve the preference. The feature is restricted to single-player sprite targets; demos, netgames and voxel targets bypass it.

## Player behavior

A pistol, chaingun, shotgun or super-shotgun discharge with at least one confirmed damaging visible-eye hit gets **one 15% roll per surviving target**, after normal pellet damage. Lethal shots, projectiles, melee, unknown eyes and already-blind floaters cannot roll. The original target, damage attribution, critical multipliers and death processing remain in place.

| Monster | Effect | End condition |
| --- | --- | --- |
| Cacodemon | Wanders without tracking or retaliating against players; attempts a ball attack every 70 tics with random yaw and pitch within ±15 degrees | One actual eligible corpse bite |
| Pain Elemental | Same blindness; attempts normal Lost Soul spawning with normal cap/collision rules. Initial skull charge has no inherited player target; subsequent skull AI is normal | One actual eligible corpse bite |
| Spider Mastermind | Targets the nearest visible living monster within 512 units, including its own species; retargets when necessary. It does not deliberately acquire players during rage | 350 tics (ten seconds); a successful later eye roll refreshes duration |

Blind floaters can recover by feeding even at full health and with **Monster Feeding off**. They use existing corpse eligibility, healing, approach and consumption rules, never living prey. Pain Elemental recovery feeding does not spawn skulls. Blocked/unavailable food leaves the monster blind. Searching reuses the existing bounded feeding budget (four searches per tic, first 256 thinker entries per search, up to eight path candidates, configured radius default 384 units); this is not new navigation or an unlimited corpse search.

Rage persists without a suitable victim until its timer ends. Incidental player hits remain possible and deal **2x outgoing damage**. The raging Spider takes at least **2x incoming damage**; explicit weapon/anatomical multipliers use the **larger multiplier**, never their product. Plasma therefore remains **3x**, not 6x, with Monster Weapon Vulnerabilities enabled. Without that switch the rage floor is 2x. No new bleeding DOT is added. Boss splash immunity remains unchanged. This status does not require Headshot Frenzy.

## Classification, lifecycle and saves

The shared visible sprite intersection supplies an independent eye-contact bit, preserving primary brain/chassis/leg regions. Fingerprint-verified stock landmarks share the glowing-eye artwork definitions and include an opaque-only one-pixel socket allowance. Authored empty views are authoritative: rear, hidden or closed eyes fail safely. Modified artwork with an unmatched identity receives no eye injury. Glowing Eyes and Wound Holes do not need to be enabled for classification.

All timers, targets and projectile marks live in actor sidecars. Save trailer **v4** adds length-checked eye fields to the existing record; v1/v2/v3 saves load with empty eye state. New saves should be opened with this build or newer. Actual native save/load preserves blindness, rage targets and remaining timers. Death/removal/resurrection/respawn/level cleanup uses the existing sidecar lifecycle; removed actors are also cleared from other actors' eye targets. Turning off Eye Injuries or Enhanced Monster Weak Points immediately cancels these effects. Disabled eye rules consume no additional gameplay random values.

Directional Cacodemon shots use the normal missile spawn/collision path and actual source ownership. Blind Pain skulls use the normal spawn cap and position check. Rage-launched projectiles retain their same-species infighting provenance after rage expires; disabling the feature cancels that mark. No raw actor structure, ordinary save body or renderer API format is changed.

## Graphics-config tuning

Percentages are integers; time is game tics (35 per second). Existing config parsing validates and persists these values.

| Key | Default | Accepted range |
| --- | ---: | --- |
| `wp_eye_injury_chance` | 15 | 0–100 |
| `wp_eye_rage_duration` | 350 | 35–3500 |
| `wp_blind_fire_interval` | 70 | 35–350 |
| `wp_blind_fire_pitch` | 15 | 0–30 degrees |
| `wp_eye_rage_incoming` | 200 | 100–800 percent |
| `wp_eye_rage_player_damage` | 200 | 100–800 percent |
| `wp_eye_rage_search` | 512 | 64–2048 map units |

The 15% chance uses the engine's existing byte-RNG percentage convention. Cosmetic eye rendering uses its separate existing controls.

## Focused verification — 2026.11.14.2

Built the normal optimized engine, test targets and launcher with diagnostic performance probes disabled. Only tests affected by this change were run; no full WAD campaign matrix or performance claim is included.

- **434 passing weak-point checks**, including 42 new eye-injury assertions: 15% byte boundaries, per-discharge aggregation, lethal/compatibility exclusions, no reroll while blind, max-only damage, expiry, corpse recovery, feature cancellation and v3/v4 persistence.
- Existing wound placement/classification target passed.
- **41 native checks**: actual stock sprite rays and mirroring, classification with glow/wounds disabled, replacement-art rejection, real Cacodemon/Pain projectile spawning, both recovery bites without normal feeding, central damage processing and actual native save/load.
- **10 F4 persistence assertions across four separate processes**: default off, actual draft/apply, relaunch loading and F10 discard, using an isolated config path.
- Launcher persistence/export/import: **18 checks passed**, including the eye key; settings backup checks passed. **14 normal-release packaging checks passed**, including matching engine hash/version and default-off eye setting.
- Three real GPU scenes completed, confirming live blindness and Spider monster targeting. Captures: `build/eye-injuries/visual/caco-blind-fire.png`, `pain-blind-fire.png`, `spider-eye-rage.png`; stock masks: `build/eye-injuries/native-final/eye-overlay-{14,22,19}.bmp`.

Reproduction: build targets `doom`, `doom-critical-smoke`, `doom-weakpoints-tests`, `doom-wounds-tests` in Release. Run the two unit executables, then:

```powershell
build/Release/doom-critical-smoke.exe test-references/DOOM2.WAD build/settings-arena.wad build/eye-injuries/native-final --eye-injuries
build/Release/doom-critical-smoke.exe test-references/DOOM2.WAD build/settings-arena.wad build/eye-injuries/visual --visual-eye-injuries
```

Use absolute paths if running outside the repository. The generated arena fixture and full runner/logs remain under `build/eye-injuries`; visual fixtures force 100% chance and boosted health to make captures repeatable. Production defaults remain 15% and normal health. Captured FPS is incidental and is not a benchmark.

Coverage limitations: representative stock front/mirrored rays and three renderer scenes, not every animation/rotation or custom WAD. Back/hidden/replacement views fall back conservatively. No full network/demo, campaign or large-map performance matrix was rerun. Existing legacy sound warnings and dummy-renderer notices appear in headless test logs; native gameplay assertions passed. Feeding retains its existing bounded search/path limitations described above.

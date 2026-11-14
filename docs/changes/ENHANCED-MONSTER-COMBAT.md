# Enhanced monster combat

```yaml
project: DOOM: HELLFORGE
build: 2026.11.14.1
updated_utc: 2026-11-13T19:16:57Z
status: active
evidence_build: 2026.11.12.1
tests: ENHANCED-MONSTER-COMBAT-TESTS.md
index: ../README.md
```

The three new **F4 > Combat** switches default off and operate independently of **Critical parts**, which retains the existing four human headshot profiles:

| Switch | Config key | Behavior |
| --- | --- | --- |
| Enhanced Monster Weak Points | `enhanced_weak_points` | New anatomical bullet rules and close shotgun face executions |
| Monster Weapon Vulnerabilities | `monster_weapon_vulnerabilities` | Monster-specific player projectile rules |
| Localized Injury Effects | `localized_injury_effects` | Temporary effects following an enabled damaging rule |

All new rules apply to player weapons in accelerated single-player sprite mode. Voxel-rendered enemies, multiplayer, demos, player targets, enemy attacks and unrelated environmental damage retain ordinary rules. Wound Holes remains an independent visual setting. Existing holes never change collision.

## Implemented matrix

| Monsters | Player bullet/pellet weak point | Plasma Rifle direct | Player rocket direct | BFG |
| --- | --- | --- | --- | --- |
| Zombieman, shotgun guy, chaingunner, SS | Existing 2x head; separate original switch | Unchanged | Unchanged | Unchanged |
| Imp, Revenant, Arch-vile, Hell Knight, Baron | 2x head | Normal | Normal | Normal |
| Mancubus | 1.75x head | 1.75x anywhere | 1.5x anywhere | Normal |
| Cyberdemon | 1.5x head | 1.5x anywhere | 1.5x anywhere | Normal |
| Pinky, Spectre | Shotgun face pellets 1.5x; other bullets normal | Normal | Normal | Normal |
| Cacodemon, Pain Elemental | 1.5x visible eye | 2x anywhere | Normal; confirmed eye has 30% fatal chance | Normal |
| Lost Soul | Normal; existing ballistic collision | Fatal physical contact | Normal | Fatal main-projectile contact or valid spray tracer |
| Arachnotron, Spider Mastermind | Shotgun brain pellets 2x; other bullets normal | 3x anywhere | Brain 1.5x; legs 1.25x; chassis normal | Normal |

Other anatomical regions receive ordinary ballistic damage. Each pellet retains normal damage and pain processing. Use the maximum region/weapon multiplier, never their product. Fractional damage rounds to the nearest integer with halves rounded up; positive ordinary damage stays at least one and large values saturate safely.

Rocket splash remains a separate normal damage event. Cyberdemon and Spider Mastermind splash immunity is preserved. A direct rocket cannot apply both brain and leg direct damage. BFG spray retains its actual ray eligibility; nearby or obscured Lost Souls are not automatically killed.

Pinky/Spectre execution is evaluated once per surviving target at discharge completion. Standard shotgun needs at least three confirmed face pellets within 96 units; super shotgun needs seven within 128 units. Ranges use the engine's horizontal shot travel in map units. These in-range face pellets must constitute at least half of all pellets hitting that target. A lone pellet cannot execute.

Heavy monsters surviving a critical head hit below 10% of their configured spawn health have a 25% chance of disrupting their next ranged attack within three seconds. One horizontal offset up to +/-8 degrees is shared by that attack's volley, with a three-second activation cooldown. Targets and projectile ownership are preserved.

A confirmed mechanical rocket-leg hit has a 30% chance of immobilizing for 35 tics (one second), followed by a 175-tic cooldown. Gravity, attack states, death and boss processing continue. Disabling a corresponding rule cancels its pending injuries.

## Architecture and safe fallback

`p_weakpoints.cpp` owns explicit damage categories, balance, discharge counters, player-projectile provenance, temporary status records and a bounded save trailer. `wounds.cpp` supplies the existing shared sprite-surface intersection and cached anatomical result; gameplay rules do not run inside duplicated cosmetic damage notifications.

`monster_regions.h` uses bounded humanoid silhouettes and fingerprint-verified stock landmarks. Pinky faces follow eye/mouth geometry; floating eyes use visible painted eye landmarks. Mechanical brains follow the connected organic artwork; leg masks trace exterior metallic foot components outside the brain/chassis. Unrecognized non-humanoid replacement art receives no anatomical bonus. Recognizable replacement humanoids may use a conservative silhouette; unreliable views receive ordinary damage.

Additional fingerprint metadata covers fullbright attacks and Arch-vile healing frames. Reviewed Arch-vile skull caps exclude raised hands; spell frames that obscure the head intentionally have no critical head region. Horns, launchers, hidden eyes and uncertain mechanical overlaps fail safely. Some rear/occluded views have no region; coverage is recorded rather than silently treated as a head.

Projectiles keep the engine's actor/world collision. Localization reconstructs a finite sweep through the admitted actor contact volume onto the visible sprite surface. The projectile center must land on verified opaque artwork, with footprint support; overlapping different critical parts is uncertain. Transparent pixels never allow a rocket/plasma projectile to pass through an actor. Unknown anatomy retains whole-monster weapon weaknesses but cannot execute or cause a localized injury.

Status/provenance sidecars do not change the raw native actor record. A versioned, length-checked trailer saves records by thinker save order, including remaining timers, processed-contact flags, in-flight trajectory and shot identity. The new loader strips this trailer before existing destruction/arena loaders run. Previous native saves load with empty new state; their already-flying projectiles retain ordinary behavior until new projectiles are fired. Use the updated executable for new saves containing injury state.

Random effects use the game RNG, with one eligibility roll per effect attempt. No additional random draws occur with the new features disabled. Region masks are cached per image/type and cleared with the image/level lifecycle; they remain cached when cosmetic wounds and flinches are off.

## Config-only balance

These integer keys are saved alongside the existing graphics options. Damage values are percentages (`175` means 1.75x); durations/cooldowns are game tics at 35 tics/second. Invalid/out-of-range values are ignored.

`wp_humanoid_head` applies to the five added humanoid demons. The original four human profiles retain their existing fixed 2x headshots and separate switch.

| Keys | Defaults |
| --- | --- |
| `wp_humanoid_head`, `wp_heavy_head`, `wp_cyber_head` | 200, 175, 150 |
| `wp_face_pellet`, `wp_eye_bullet`, `wp_brain_pellet` | 150, 150, 200 |
| `wp_heavy_plasma`, `wp_cyber_plasma`, `wp_floating_plasma`, `wp_mechanical_plasma` | 175, 150, 200, 300 |
| `wp_heavy_rocket`, `wp_brain_rocket`, `wp_leg_rocket` | 150, 150, 125 |
| `wp_eye_execution_chance`, `wp_aim_chance`, `wp_leg_chance` | 30, 25, 30 |
| `wp_aim_degrees`, `wp_aim_duration`, `wp_aim_cooldown`, `wp_low_health_percent` | 8, 105, 105, 10 |
| `wp_leg_duration`, `wp_leg_cooldown` | 35, 175 |
| `wp_shotgun_range`, `wp_ssg_range`, `wp_shotgun_quota`, `wp_ssg_quota`, `wp_face_majority_percent` | 96, 128, 3, 7, 50 |

Damage percentages accept 100–800; chances and low-health threshold 0–100; aim 0–30 degrees; durations 1–350; cooldowns 1–3500; ranges 1–1024; quotas 1–7/1–20; required majority 1–100.

## Verification and limitations

The original 121 detectable human head views/seven safe fallbacks remain the regression baseline. `doom-weakpoints-tests` covers the balance matrix, non-stacking, numeric bounds, eligibility, execution gates, duplicate contacts, discharge thresholds, injury expiry/cooldowns, config parsing and serialized sidecars. `doom-wounds-tests` covers shared pixel geometry and attachment.

`doom-critical-smoke` exercises the actual engine with an owned Doom II IWAD: live walking/attack/pain/heal rotations, stock region coverage, physical plasma/rocket contacts, BFG direct/tracer eligibility, native shotgun discharges, boss splash immunity, real AI injuries and actual native save/load including the previous format. It writes `region-coverage.csv` and colored PPM overlays. Run it from an isolated artifact directory because its save tests write native save files there.

```powershell
python duct-tape-doom-main/tests/ragdoll_arena.py zombie build/critical-arena.wad
New-Item -ItemType Directory -Force build/critical-smoke | Out-Null
Push-Location build/critical-smoke
& '..\Release\doom-critical-smoke.exe' 'C:\Games\DOOM2.WAD' "$PWD/../critical-arena.wad" "$PWD"
# Short, bounded GPU captures of all thirteen new monster profiles:
& '..\Release\doom-critical-smoke.exe' 'C:\Games\DOOM2.WAD' "$PWD/../critical-arena.wad" "$PWD" --visual-enhanced
Pop-Location
```

From the repository root, `python duct-tape-doom-main/tests/weakpoint_contact_sheet.py build/critical-smoke` creates PNG contact sheets (Pillow required). Red=head, orange=face, green=eye, magenta=brain, blue=legs. `author_monster_regions.py` regenerates fingerprints using an owned IWAD; it does not redistribute sprite pixels.

The nonvisual harness also accepts `--solid` to exercise the launcher's solid-map collision path; it disables terrain damage in that isolated fixture so repeated anatomy samples do not excavate the test floor. The normal and solid runs write cached-hit query timings; the bounded GPU run also writes a same-scene frame-time comparison. See [recorded validation](ENHANCED-MONSTER-COMBAT-TESTS.md).

These are approximate sprite-based anatomical surfaces, not 3D skeleton hitboxes. Conservative mechanical leg masks cover verified exterior leg/foot pixels; ambiguous chassis overlaps and hidden regions remain ordinary hits. Replacement-art anatomy and very oblique/curved projectile contacts can fall back to ordinary localized damage. Large-map and physical LAN performance are not certified by the bounded smoke fixtures; LAN mechanics remain disabled.

No balance defaults were changed from the approved plan. Suggested playtesting focuses on boss plasma time-to-kill, close Pinky execution frequency, and whether mechanical vulnerabilities overshadow ordinary weapon selection; these are review topics, not implemented balance changes.

## Feedback and additional statuses

Build 2026.11.9.2 adds independently controlled metal-leg sparks, selectable headshot confirmation, bullet bleeding and close-super-shotgun frenzy. See [behavior, configuration and monster/status matrix](MONSTER-FEEDBACK-AND-STATUSES.md). The damage matrix above remains unchanged.

Build 2026.11.10.0 extends feedback and statuses with [progressive bleeding and monster feeding](MONSTER-SURVIVAL-AI.md). Existing anatomical/weapon damage multipliers remain unchanged.

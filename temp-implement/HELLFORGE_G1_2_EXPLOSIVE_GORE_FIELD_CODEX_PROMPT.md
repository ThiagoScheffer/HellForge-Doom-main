# CODEX IMPLEMENTATION SPEC — HellForge G1.2: Impact Gore, Explosion Giblets & Environmental Aftermath

**Status:** implementation instructions, NOT a claim that G1.2 is coded, built or visually verified.  
**Baseline:** the **current local HellForge working tree**, including approved M1, M2.1, G1 and any actual G1.1 changes. Do **not** replace it with the older uploaded source snapshots.  
**Prerequisite reference files (if present):** `GORE_M2_1.md`, `GORE_GIBLETS_G1.md`, `GORE_GIBLETS_ART.md`, `HELLFORGE_G1_1_SHOTGUN_GORE_CORRECTION_CODEX.md`.  
**Art:** the project's own original `assets/gibs/` PNGs; no external gore mod files, ZScript or copied textures.  
**Goal:** environmental gore like the *density and distribution* in the supplied reference screenshots, without modifying normal monster animations or introducing gameplay physics.

> **Codex work order:** Inspect current files and git state; identify existing real APIs; implement the work **in stages**, reusing the M2.1 resolver and G1 native giblet system. Prioritize visible impact, physics-correct landing and a lasting but bounded scene of ground debris and surface splashes. Create tests and useful `-goredebug` output. Preserve gameplay and all current protections. Do not start M3 ragdoll/savegame work. If an interface below differs from current code, adapt to the real source and explain the change; do not hallucinate symbols.

---

## 0. Decisions and constraints — source of truth for this task

### Desired outcome

1. **Close Shotgun/SSG (0–128 Doom units):** every *confirmed damaging* opaque hit on one of the seven supported monster types creates a modest, recognizable material-specific giblet reaction even at full health, subject to enabled settings and shared budgets. A miss or shot passing near a monster produces **nothing**.
2. **Medium Shotgun/SSG (129–320 units):** a *smaller optional cosmetic-only* reaction on a significant confirmed hit; no head destruction, torso cavity, or other anatomy changes because of the extended distance. Keep M2.1's **128-unit cutoff for its existing major injury decisions**. The 320-unit limit is a new initial tuning proposal; keep it as a central constant rather than duplicating a magic number.
3. **Critical/severe shotgun hit:** one *upgraded* burst, not a baseline plus a second severe burst. Existing M2.1 low-HP wounds and one-per-life special injuries remain as currently approved.
4. **Fatal shotgun headshot:** preserve the verified M1/M2.1 head-removal mask and route its cosmetic fragment emission through G1's textured giblets. Use red/pink **brain-like tissue** plus small flesh; large pieces can land on dry floors and leave stains on real surfaces. Do **not** draw both new textured fragments and old procedural **heavy** head pieces for the same event. Ordinary light blood droplets may continue.
5. **Genuine explosion damage:** rocket direct impact, rocket splash and verified exploding barrels (including chain reactions) can produce **radial, blast-biased giblets**. Major lethal/existing extreme-death outcomes receive larger bursts; meaningful nonlethal explosive damage gets a smaller response. Do not change Doom damage, blast radius, death-state selection or actual overkill logic.
6. **Ground aftermath:** settled `GIB*B0` sprites remain visible on valid floors for **up to 15–30 seconds** by material/size, unless evicted for capacity. Scatter several surface stains using the **existing** decal system. Existing bullet/shot blood must not be removed.
7. **Wall aftermath:** verified nearby wall hits can create irregular species-colored splashes and small smears. Use real impact surfaces and the existing wall-decal limit; never fake decals through two-sided portals, sky, doors, liquids or missing wall spans.
8. **No new monster sprite animation work:** normal stock deaths and extreme-death frames remain in use. No full-body atomization, new limb cutting, 3D organs, rigid-body actors or savegame serialization of particles.
9. **Original artwork and engine only:** use existing generated G1 atlas and native G1 giblet simulation. The reference screenshots and Droplets/Nash-style mods are **visual inspiration only**, not code/art sources.

### Non-negotiable runtime gates

- Only `MT_POSSESSED`, `MT_SHOTGUY`, `MT_TROOP`, `MT_UNDEAD`, `MT_FATSO`, `MT_SERGEANT`, `MT_HEAD`. Exclude `MT_SHADOWS` (Spectre) and all other types.
- Respect **Enhanced Gore**, **Blood ON**, **Flying flesh fragments ON**, accelerated renderer, and existing demo/network/voxel exclusions. Preserve F4 settings names and saved `graphics.cfg` keys.
- `totalkills < 200` using the **map's total kill-count population**, not current alive monsters.
- Head cuts obey verified stock-art / existing M1-M2.1 rules and independent Head Destruction switch.
- Existing `Body destruction & viscera` controls **major** M2.1 living injury/special effects, not modest baseline impact giblets when Flying Fragments is enabled.
- `Enhanced wall blood OFF` suppresses **extra wall marks**, but should not suppress otherwise eligible flying fragments or legitimate floor presentation when Blood and Fragments remain on.
- Preserve gameplay damage, health, aim, hit registration, spread, ammo, AI, corpse states, resurrection behavior, `P_Random` / gameplay RNG, and determinism where gameplay demands it.

---

## 1. Before editing: audit and eliminate duplicate implementation

Start:

```powershell
# Run in the actual Doom engine source directory.
git status --short
git diff --stat
rg -n "goreBurst|goreCapture|goreAfterHit|GoreShot|goreHeadless|fragmentPath|goredebug|P_WeakpointEndShot" platform linuxdoom-1.10
rg -n "giblets::|gore_giblets|GIB[0-9]|firstSolidWall|bloodWallSplat|bloodFloorSplat" platform tests
rg -n "P_RadiusAttack|P_DamageMobj|P_KillMobj|WP_ROCKET_SPLASH|WP_OTHER" linuxdoom-1.10 platform
```

Inspect *actual live implementations*, especially:

- `platform/scene3d/{wounds.cpp,gore_policy.h,spray.cpp,giblet_scene.cpp,scene.cpp,blood.cpp,internal.h}`
- `platform/gore_giblets.{h,cpp}` (or actual paths)
- `platform/scene3d/giblet_atlas.generated.h`, `assets/gibs/`, `tools/pack_giblets.py`
- `linuxdoom-1.10/{p_map.c,p_inter.c,p_pspr.c,p_enemy.c,p_mobj.c}` and relevant weak-point APIs
- F4: `platform/{scene3d.h,settings_panel.cpp,settings_layout.h}` and launcher schema only if required
- Existing tests: `tests/{critical_smoke.cpp,wounds.cpp,gore_giblets_foundation_tests.cpp}` and current CMake targets.

**Audit outcomes to record before changes:**

| Path | Question | Must find |
|---|---|---|
| Shotgun | Is G1.1 already merged? | One true per-victim **end-of-shot** visual decision |
| M2.1 anatomy | Are low-health gates incorrectly preventing routine giblets? | Separate transient feedback from persistent injury |
| Headshot | Are M1 procedural large fragments still emitted? | One exclusive heavy-fragment owner |
| Explosion | Where can real damage, explosion origin and projectile/barrel identity be captured? | Safe source provenance and one event/victim/explosion |
| G1 simulation | Does `System` already carry event ID and contact surface IDs? | No parallel simulation, sound or decal manager |
| Draw | Are A/B atlas frames and quads visible in the accelerated renderer? | Actual draw counters, valid pivots, ground orientation |
| World | Do collision traces work with and without `-solidmaps`? | Safe visible fallback, never fake marks |
| Budgets | Who counts live flight/rest pieces, old procedural pieces, and decals? | Single global accounting |

If G1.1 has already been implemented, **retain it and extend it**; do not reimplement baseline shotgun emission. If not, fold G1.1's Tier A/B/C logic into this implementation and report that G1.1 was combined with G1.2.

**Diagnostics first:** prove whether missed effects are rejected at qualification, dropped by `emit()`, submitted but invisible, or failing to land. Previous reports passed headless tests but did **not** establish GPU visual correctness. Do not assume the problem is purely thresholds.

---

## 2. Shared visual event architecture — reuse, don't replace

Implement a **single cosmetic response decision** per victim and source event, with two downstream presentations:

```text
Real confirmed monster damage (Shotgun/SSG or genuine explosion)
    -> event provenance + actual damage / post-HP
    -> seven-monster policy + enabled gates
    -> SEVERITY: routine / major living injury / fatal / extreme
    -> ONE per-victim GoreVisualPlan
       |-> existing G1 textured heavy giblets  (one owner)
       |-> existing blood droplets + surface decals (existing owner)
       `-> M1/M2.1 validated anatomy effect ONLY where already approved
    -> real swept surface contacts -> bounded floor/wall aftermath
```

### Suggested C++17 data shapes (REFERENCE — adapt to actual engine types)

```cpp
// Do not paste into a shared header without checking existing definitions.
enum class GoreCause : uint8_t {
    Shotgun, SuperShotgun, RocketDirect, RocketSplash, BarrelExplosion
};
enum class GoreTier : uint8_t { None, Routine, Severe, Fatal, Extreme };
enum class GoreMaterial : uint8_t { Flesh, BrainLike, Viscera, HeavyTissue, Bone };

struct GoreDamageSnapshot {
    uint64_t sourceEventId = 0;   // unique within current level and source instance
    uint32_t victimToken = 0;     // safe level-local actor token, not a saved pointer
    int monsterType = 0;
    GoreCause cause{};
    int observedDamage = 0;       // actual HP loss incl. existing overkill handling
    int hpBefore = 0;
    int hpAfter = 0;
    int confirmedOpaqueContacts = 0; // relevant only to shotgun decision
    float sourceDistance = 0;     // Doom units, for shotgun thresholds
    Vec3 worldImpact{};           // renderer/world units: confirm conversion
    Vec3 outgoingDirection{};     // shot travel / blast push
    Vec3 blastCenter{};           // only for radius-damage sources
    bool genuineExplosion = false;
    bool deathConfirmed = false;
    bool engineExtremeDeath = false; // from real death result; do not guess
};

struct GoreVisualPlan {
    uint64_t visualEventId = 0;   // stable event + victim identity for dedupe
    GoreTier tier = GoreTier::None;
    GoreMaterial material = GoreMaterial::Flesh;
    int desiredGiblets = 0;
    int desiredGroundMarks = 0;  // requests ONLY; actual marks need real surfaces
    int desiredWallMarks = 0;    // <= 3 after shared admission
    bool texturedHeadBurst = false;
    bool radial = false;
    float sizeScale = 1.0f;
    float launchScale = 1.0f;
    uint32_t cosmeticSeed = 1;
};
```

**These identifiers are proposed, not currently verified engine APIs.** Reuse existing M2.1 `GoreShot`, G1 `Burst`, `Impact`, scene coordinates and state rather than introducing redundant structs/maps. Keep only the missing fields. Do not pass raw `mobj_t*` to long-lived particles or save any pointers. Add `eventId` and validated surface contact metadata to `Impact` only if current live code lacks them.

### Exact-once and precedence

- Shotgun: **one visual plan per victim per finished discharge**, after `P_WeakpointEndShot()` / actual current end-of-shot boundary; no giblets per pellet. All real damaging opaque contacts contribute; dead-body pass-through and synthetic weakpoint executions do not.
- Severe or fatal tier **replaces** routine counts for the **same discharge**, never adds a second full burst. Existing M1 light blood droplets may remain if in separate shared budget.
- Explosion: **one visual plan per victim per distinct explosion instance**; rocket direct hit and the immediate radius damage must coalesce for the same projectile/victim. The same explosion may legitimately affect multiple victims.
- Barrel chains are **separate actual explosion instances**. A victim hit by two distinct barrels may receive two responses if both cause real damage and capacity permits; never dedupe them merely because they occur on the same tic.
- Existing major injury once-per-life flags apply to *persistent living wounds/special activations*, not to baseline transient giblets. Repeated later real hits may create smaller bounded responses.
- A fatal event that also triggers head removal must route heavy fragments through G1 **once**; do not emit both procedural and textured heavy pieces.

---

## 3. Shotgun response policy: close, medium, severe, fatal, head

### Initial distances (central constants; tunable)

- Close: `0 <= range <= 128` Doom units.
- Medium: `128 < range <= 320` Doom units, **new cosmetic feedback only**.
- Beyond 320: retain old ordinary blood behavior, no new G1.2 giblets from this feature.

**Significant medium hit:** initially require positive observed damage and at least one confirmed opaque sprite contact; optionally add a small observed-damage floor only after playtest data shows excess emissions. Never use estimated target proximity or a pure cylinder miss as a hit. Do not widen the existing 128-unit M2.1 major-anatomy gate.

### Proposed material and count policy (requested pieces, NOT guaranteed at capacity)

| Monster (Doom actor) | Close routine | Medium | Severe / fatal close | Explosion fatal | Material, color and limitations |
|---|---:|---:|---:|---:|---|
| `MT_POSSESSED` | 2–3 | 0–1 | 4–6 / head 5–8 | 5–8 | Red flesh; **brain-like** head fragments only if verified head burst; no torso cavity |
| `MT_SHOTGUY` | 2–3 | 0–1 | 4–6 / head 5–8 | 5–8 | Same human policy; no torso cavity |
| `MT_TROOP` | 3–4 | 1–2 | 4–6 / head if verified | 5–8 | Red-brown flesh; living low-HP torso wound only under M2.1 |
| `MT_UNDEAD` | 2–3 | 1–2 | 4–6 / skull if verified | 6–10 | Pale bone shards; no generic red visceral pool |
| `MT_FATSO` | 4–5 | 1–2 | 5–8 | 8–12 | Large red tissue / viscera; living low-HP belly wound only |
| `MT_SERGEANT` | 3–5 | 1–2 | 5–8 | 6–10 | Red viscera, lower trajectories; no sprite hole |
| `MT_HEAD` | 4–6 | 1–2 | 6–10 | 8–12 | Blue large viscera + trailing droplets; no sprite hole |

Keep an upper bound of **12 pieces per victim event**. Medium range must have visibly less output, distance and lifetime than close range. These values are proposed starting points, **not measured optimal counts**.

### M2.1 compatibility rules

- Zombieman/Shotgun Guy: **no torso cavities** in G1.2, even if older M2.1 code still contains a living human cavity path; remove/disable that visual path if the latest user-approved rework has not done so. Preserve fatal verified head removal.
- Imp: preserve the M2.1 qualifying **living, low-health** chest/belly wound; clear on death. No fatal torso overlay.
- Revenant: bone shards; verified skull destruction only. No human-style organs, no unverified rib cut.
- Mancubus: keep severe **living** chest/belly damage and drips; normal death artwork, no cut carried through the death sequence.
- Pinky: no sprite hole; low-health visceral accent and stronger fatal pieces.
- Cacodemon: no sprite hole; blue medium/large viscera, robust landing and blue wall/floor stains.
- Spectre / other monsters: unchanged.

### M1 textured head burst — special requirement

1. Confirm that current death was a *verified qualifying* existing head-removal outcome (not merely a pellet near the head).
2. Keep current stock-art fingerprint checks and corpse-mask outcome untouched.
3. Remap the current cosmetic head fragment **heavy** emission to G1. Choose initial art from `GIB4A0/B0`, `GIB5A0/B0` (brain-like, not actual anatomical brain sprites) mixed with `GIB2`/`GIB3` flesh; retain pink-red tint but avoid overbright clipping.
4. Emit 5–8 pieces biased forward from the **world-space head impact**, with some side/upward spread and a subset of gravity-dominated short flights.
5. Stop old procedural heavy fragments for **this** event when textured G1 succeeds. If atlas unavailable or the textured module is unavailable, **fall back exclusively** to the approved procedural M1 visual path; never double-render both.
6. Do not add headless geometry to Revenant/Imp without the already verified M2.1 art policy.

---

## 4. Explosion provenance and cosmetic eligibility — strict correctness

**Do not treat every `WP_OTHER` as an explosion.** Earlier sources distinguish `WP_ROCKET_SPLASH` from other damage, and barrel damage can use other categories. Detect the **real damaging cause and origin** in the current engine.

### Required source mapping

- **Rocket direct:** actual rocket projectile collision / direct damage. Obtain projectile identity and travel/contact position where available.
- **Rocket splash:** `P_RadiusAttack()` or the current real radius-damage path, with the actual explosion center and current rocket instance.
- **Barrel:** actual barrel explosion / radius damage path, including barrels detonated by rocket or another barrel; use the barrel explosion as its **own** event.
- **Other sources:** exclude plasma, pistol, monster fireball, sector damage and generic `WP_OTHER` unless proven to be a real configured explosion. Future explosive sources should be added through explicit cause mapping, not guessed from damage amount.

### Correct source-event grouping

1. Establish a **level-local unique explosion instance key** using engine-provided projectile/barrel identity plus occurrence, or a monotonically increasing cosmetic sequence assigned when an actual explosion starts. Never use `gametic` alone, a raw pointer alone, or an actor position alone.
2. Accumulate a victim's **actual observed damage** for this source instance (direct + radius when both occur), and snapshot the before/after health and confirmed death/extreme state at the appropriate boundary.
3. Finalize per victim **after all damage from that explosion instance has resolved**. For chained barrels, handle nested explosions safely with an explicit bounded context/stack or engine-correct event completion mechanism; do not use a single unguarded global `currentExplosion`.
4. Emit one cosmetic response for each damaged eligible victim; no cosmetic emission for a mere proximity query or a fully immune/no-damage victim.
5. If an actor is removed or changes identity before finalization, use a safe event snapshot, not an invalid pointer. Validate level transitions and resurrection cleanup.
6. The regular death animation and engine's extreme-death decision stay authoritative. Do not force a monster into an extreme death because the gore tier is high.

### Explosion severity policy (initial tuning)

- **Small nonlethal explosion:** require *meaningful real damage* (initial candidate: `>= 15` observed HP damage for regular actors; a separate higher threshold for very large monsters is allowed if tests justify it). Emit a limited radial burst; no persistent anatomical sprite changes from this feature.
- **Lethal explosion:** strong radial burst even when the lethal portion is modest, provided the explosion genuinely caused the death and normal gates allow effects.
- **Existing extreme-death result:** strongest compatible radial fragment burst, **within the same per-event and aggregate budgets**. Do not modify `P_KillMobj` animation selection or overwrite extreme-death gore already on screen; merge visual effects.
- **Direction:** primary vector `normalize(victimImpactPosition - blastCenter)` with deterministic bounded lateral/upward variation; choose safe fallback direction for coincident centers. Avoid sending fragments toward the middle of the blast by mistake.
- **Every confirmed distinct explosion may hit multiple victims**, but limit the *total* new heavy giblet admissions to **24 per explosion**. Distribute fairly, deterministically; prioritize lethal/severe victims and then proximity/observed damage, without gameplay RNG.

If tracking true direct + splash as one event is impractical in the current source, **do not ship a double-emitting workaround**. Isolate the source-event bridge with a focused test and report the real blocker.

---

## 5. Giblet behavior: physically legible, not just more particles

Use the existing `platform/gore_giblets.{h,cpp}` simulation, G1 textured quads, and current world collision adapter. **Do not create a second physics system or a second particle pool.**

### Initial kinematic styles

| Style | Launch | Landing behavior | Material |
|---|---|---|---|
| Shotgun close | Forward cone, modest lateral scatter and lift | Several short flights to nearby floor, few longer flights | Flesh / bone / viscera |
| Shotgun medium | Lower energy, very small pieces | Limited nearby landing; less visible area | Small species-appropriate |
| Head burst | Head-height forward/upward ejection | Brain-like piece(s) settle + red wall/floor marks | Brain-like tissue and flesh |
| Rocket/barrel | Radial away from real blast center with upward lift | Heavy clumps tumble, optionally one small bounce, then settle | Species-specific |

**Geometry and visual checks:**

- Spawn from **actual validated monster impact position** / anatomical head location; for explosive splash, use a position near the victim surface biased away from blast origin, not arbitrary feet/center that hides fragments inside body art.
- Do not allow NaN/zero-length launch vectors, negative radii, invisible alpha, incorrect coordinate conversion, and spawn immediately inside solid walls.
- No tunneling: use G1's swept collision. For nearby highly dynamic or uncertain geometry, show valid bounded airborne effects but suppress invalid surface marks.
- A sprites in flight; correctly matched B sprites on **valid horizontal floor** only. Use opaque bounds/pivots from atlas metadata, avoid z-fighting and always verify floor height/slope representation supported by the engine.
- A fragment contacting ceiling or wall is **not automatically floor-resting**; it may make a mark or expire. No B-frame artwork floating in air.
- Class variation: tiny flesh bounces little, viscera may settle after one short secondary arc if reliable, bone can bounce/rotate more with no red blood stains; Mancubus heavy tissue looks heavier/slower than Imp flesh.
- Keep normal visualization at 30/60/120 FPS visually consistent by running the established fixed-step simulation rather than updating velocity from rendered frame count.

### Persistence without runaway lifetime

- **Target resting lifetime:** 15–30 seconds for substantial floor pieces; smaller flecks may last less. This is runtime-only cosmetic persistence, **not** savegame serialization or unlimited permanent debris.
- **Maximum 64 heavy fragments total**, counting both legacy heavy and G1 textured paths if both can exist. Within that, initial cap **24 resting pieces**; evict oldest resting pieces when needed for new visible airborne bursts. Avoid evicting still-flying pieces except at hard capacity under explicitly tested rules.
- Reuse fixed arrays / bounded pools and existing cosmetic RNG. No malloc/free per fragment frame; no gameplay mobj actors, collision damage or AI effects.
- If actual G1 lifetime/eviction state differs, extend the **same** pool with a small change; do not add a separate resting-debris manager.

---

## 6. Environmental aftermath / Gore Field — presentation, NOT new simulation

**Visual goal:** scattered recognizable giblets, irregular blood pools across a few floor tiles, and big directional splashes on nearby solid walls, similar in *distribution* to the supplied screenshots. Routine hits should be restrained; significant shotgun kills and explosion deaths may create a broader field. **Do not replicate tall vertical blood columns or stretched sprites** visible in some references; those are not the default target.

### Gore Field scope

`Gore Field` is a small **event-to-decal placement policy** layered onto existing G1 contact events and HellForge's **current** blood/decal renderer. It must **not** own a second decal atlas, blood pool, independent physics simulation, or another permanent world-object map.

1. For each admitted source event, classify severity and request a bounded set of blood marks. **Actual valid surface contacts decide placement**; desired marks are not guaranteed rendered marks.
2. Prefer marks produced by **real giblet floor/wall collision**. For severe events, allow a few **additional short, world-validated directional deposits** (near confirmed impact/landing) to create overlapping irregular floor splatters. Each candidate must trace to a valid reachable surface; never project through a wall or across disconnected sectors.
3. Favor varied **scale, angular orientation and offset** over multiplying tiny decals. Use several existing shape variants; floor pooling can be wider with small satellite splatters.
4. Keep verified wall marks concentrated behind/adjacent to the victim in shot direction, or near actual radial impact surfaces for explosives. Correct sector/side/section and species tint must travel with each impact event.
5. The extra wall quota remains **<=3 per victim/event**. Propose **up to 6 additional floor marks for strong fatal events** and **up to 10 for extreme explosion events** only if existing actual shared floor-decal capacity and performance support them; otherwise lower the count. A bounded **total extra marks <= 12 per victim/event** is an outer request ceiling, *not* permission to bypass existing quotas. Across a 24-fragment multi-victim explosion, enforce the real global decal budget before emitting marks.
6. **Surface safety:** skip sky, liquid, void, invalid portal spans, missing textures, and uncertain dynamic geometry; preserve correct blue Cacodemon stains. Bone debris may produce no blood stain at all. Never paint over sky or fake a decal on a two-sided line.
7. Keep decal art separate from physical giblet art. Settled giblets and surface stains may overlap spatially, but they must not duplicate geometry or consume each other's **wrong** budget.
8. Prefer existing procedural blood decals for the first test. The twelve original G1 surface sprites (`CRS*`, `SGC*`, `SGW*`, `SQSHA0`) are a **later optional presentation pass**: wire only when surfaces, alignment and ownership are verified. Avoid a new wall-attachment renderer or permanently hanging geometry in this task.
9. Grounded stains may outlive the 15–30-second pieces via the **existing** decal lifetime/eviction policy. Do not increase decal lifetime or capacity globally without measurement and user approval.

### Proposed event visual density (tunable)

| Event | Physical pieces | Surface response |
|---|---|---|
| Routine close hit | Per-monster Tier A table | 1–2 localized admitted floor/contact marks if geometry allows; existing blood spray |
| Significant medium hit | 0–2 small | Small marks only if valid; no broad field |
| Living severe injury | Per-monster enhanced Tier B | 2–4 localized/landing marks; keep actual M2.1 wound/drip |
| Fatal close body hit | 4–10, species-specific | Wider 3–6 mark cluster, nearby valid wall splashes |
| Verified fatal headshot | 5–8 brain/flesh | Head-height forward wall splash and small floor tissue marks |
| Lethal rocket/barrel | 5–12 per victim; 24 total/explosion | Broad but bounded radial floor field and valid wall marks |
| Extreme explosion death | At most same hard caps | Highest stain density consistent with existing decal budget |

**Optimization:** use larger, varied surface decals and modest numbers of visible pieces, **not hundreds of physical fragments**. Make visible effect strength come from shape, placement, size and lifetime.

---

## 7. Engine integration details and budgets

### Read current classes before patching

The G1 report describes:

- `platform/gore_giblets.{h,cpp}`: event IDs, fragment kinds/tints, fixed-step simulation, collision callbacks and impact data.
- `platform/scene3d/giblet_scene.cpp`: collision adapter, map atlas, textured draw, decal bridge.
- `platform/scene3d/spray.cpp`: current `goreBurst()` / procedural particles; `platform/scene3d/wounds.cpp`: per-shot qualification and living/head masks.
- `platform/scene3d/blood.cpp`: tint-scoped wall/floor decals and shared effect budgets.
- `platform/scene3d/scene.cpp`: scene lifetime, tick / reset / shutdown.
- `tools/pack_giblets.py`: verifies original PNGs and generated 256x114 atlas. Run `python tools/pack_giblets.py --check` when art/source packaging changes.

**Don't infer that an old function signature still exists.** G1 report says some earlier foundation fields have already been extended. The original standalone `gore_giblets.h` snapshot is not proof of the current interface.

### Budget contract (shared, not additive)

| Budget | Hard target / rule |
|---|---|
| Total live heavy fragments | 64 combined, both flight and rest, across legacy/G1 paths |
| Resting fragments | 24 within the 64 slots (initial policy) |
| One victim event | at most 12 heavy pieces |
| One true explosion | at most 24 admitted pieces summed across its victims |
| Existing blood drop budget | 768 total; do not add another 768 |
| Additional wall marks | at most 3 per visual event; obey shared decal cap |
| Additional floor marks | bounded, only within **existing** actual decal budget |
| Map population | advanced gore only when `totalkills < 200` |

Treat budgets as *admission counts*, not requested counts. Report requested, admitted and dropped separately. Implement fair/priority allocation for multi-monster explosions; avoid the first actor in thinker order always consuming the entire pool.

**Performance constraints:** avoid an all-lines collision scan for every giblet at 70 Hz; reuse G1's blockmap spatial candidates and swept tests. No per-frame full-world gore scan, no unbounded actor map or extra GPU atlas uploads when a fragment settles. Use existing batching and atlas caches. Baked textures/sprites should not be recreated per kill.

---

## 8. F4, compatibility, lifecycle and fallback

- Preserve existing **Advanced Gore Enhanced/Classic**, Blood, Blood spray, Head destruction, Body destruction & viscera, Flying flesh fragments, Enhanced wall blood and launcher settings; **do not add settings unless absolutely required**. Update F4 help wording only if user-visible behavior changes materially.
- Classic Gore / Blood OFF: no new textured gore field, extra wall/floor effects or gore impact audio; do not erase existing vanilla monster deaths.
- Flying Fragments OFF: do not spawn G1 heavy pieces; retain existing blood effects according to their own switches. Do not make surface marks that depend on nonexistent fragment contact.
- If the packaged atlas is missing/broken, use the existing **procedural heavy-fragment fallback exclusively**, with current visuals and safety caps. No asset load by current working directory in release builds.
- Do not use `-solidmaps` as a requirement for **airborne** giblets. Where verified surface geometry is unavailable, render a bounded safe airborne effect, but omit unverifiable floor/wall decals or grounded placement.
- Clear all ephemeral actor IDs, event dedupe records, physical giblets and pending impact associations on level transition, new game, renderer shutdown, demo/network mode entry, save/load scene reset and actor removal where relevant. Do not emit again when restoring a save.
- Resurrection: clear living wound/headless associations as established by M2.1; no re-triggered impact burst. Do not save fragment simulation data.
- PWAD replacement art: ordinary fragment feedback may occur based on confirmed real monster hits, but **head masks/living cavity overlays require verified stock-art match**. Never carve unknown artwork.
- Doom original extreme-death animation remains authoritative and independent of this cosmetic visual tier.

---

## 9. Diagnostics: trace real event all the way to visible mark

Enable concise, bounded event logs under `-goredebug`. Do not log each fragment each frame.

**Desired per-event record (format illustrative; use live logger):**

```text
GORE G1.2 id=.. cause=shotgun|ssg|rocket_direct|rocket_splash|barrel
 monster=MT_HEAD victim=.. dist=74 dmg=39 hp=220->181 opaque=5
 tier=routine direction=forward requested=5 admitted=5 path=textured
 atlas=ready headCut=0 impact_origin=.. reason=emitted
GORE G1.2 IMPACT id=.. floorContacts=3 wallContacts=1
 floorMarks=3 wallMarks=1 resting=2 expired=1 drawSubmitted=4
GORE G1.2 EXPLOSION exp=.. victims=3 requested=28 admitted=24
 dedupedDirectSplash=1 budgetRejected=4
```

This is a **multi-stage** process. Do not claim `floorContacts` or `drawSubmitted` happened synchronously at emission; log a bounded later summary keyed to the event ID. Distinguish recorded contacts, accepted sprite draws, *actual* decal admissions and fragments resting/evicted.

Suggested failure reasons: `not_damaging`, `unconfirmed_contact`, `unsupported_type`, `mode_gate`, `feature_off`, `population_gate`, `range_gate`, `source_unverified`, `deduped`, `atlas_fallback`, `budget_exhausted`, `draw_culled`, `collision_uncertain`, `invalid_surface`, `emitted`.

**Diagnostic decision path:**

| Symptom | Inspect |
|---|---|
| Shotgun hit but no burst | real damage/contact, tier policy, end-of-shot timing |
| Rocket splash but no burst | explosive provenance, context lifetime, observed HP loss |
| Two bursts from one rocket | direct/radius event identity and finalization |
| `admitted > 0`, `drawSubmitted = 0` | scene renderer path, atlas, alpha, size, culling, occlusion |
| Giblets in air but none settle | trace geometry, floor contact state, B-frame pivot, lifetime |
| Big red columns | incorrect wall/floor quad orientation or texture UV, not desired visual |
| Wall contacted but no stain | surface ID/section validity, Blood/F4 gate, real mark admission |
| Cacodemon fragments red | mutable tint state or incorrect palette variant, explicit blue tint required |
| High CPU after large rocket | spatial query, extra field traces, pool/slot management |

---

## 10. Implementation sequence — release gates, not a monolithic rewrite

### G1.2-A — Baseline diagnosis and G1.1 alignment

- Determine whether G1.1 is already implemented. Fix any demonstrated G1 qualification/visibility problem before adding explosions.
- Confirm the 7-monster close-hit Tier A path, existing low-health Tier B, fatal Tier C, exact-once resolution and F4 gates.
- Inspect M1 head-heavy fragments and document who currently owns them.
- **Gate:** 7/7 full-health monsters produce a small appropriate response on an actual close damaging shotgun hit in a controlled test. GPU validation remains separate.

### G1.2-B — Headshot brain-like textured pieces

- Route verified human head burst via G1 atlas with a material variant; stop only the duplicate procedural **heavy** fragment path.
- Preserve headless corpse masking and no new death animation.
- Verify red/pink texture, actual head-height origin, forward spread, floor A/B transition and nearby wall stain; use procedural fallback exclusively if necessary.
- **Gate:** one head burst per fatal qualified discharge, 5–8 requested pieces within cap; no double emission and no floating B sprites.

### G1.2-C — Real explosive-source bridge

- Implement explicit rocket direct/splash and barrel provenance; collect actual damage; finalize one victim event per explosion. Handle nested chain explosions and removal safely.
- Add radial/force launch style to existing G1 `Burst` in the smallest compatible API change.
- Enforce per-victim 12 / per-explosion 24 and fair admission.
- **Gate:** rocket direct+splash same victim emits once, barrel chains are distinct, only actually damaged eligible monsters emit, death animations unchanged.

### G1.2-D — Grounded giblet lifetime and field of stains

- Validate A/B, floor normal/pivot, 15–30s retention, oldest-resting eviction and the 24-resting subcap.
- Route real contacts into existing decal system. Implement a small bounded supplementary **surface-validated** field for major events; use existing procedural art first.
- Improve high-intensity wall/floor spread without increasing total live heavy fragments above 64.
- **Gate:** visible debris field on dry floor and real wall marks, with no sky/portal/liquid misuse or stretched sprite artifacts.

### G1.2-E — Optimization, tests, report

- Run available unit/smoke/atlas checks; test resource exhaustion, all switches, reset and 199/200 gate.
- Record source-only test results and give a Windows GPU visual test protocol. **Do not claim live visual success until observed in-game**.
- Provide a short diff/changed-file list, parameters adjusted, summary counters, and exact remaining blockers.

Do not begin M3 ragdoll persistence, further monster support, new art, full-save serialization or extensive renderer refactoring.

---

## 11. Required tests: behavior, visuals and regression

### Automated policy / smoke cases

1. **Seven species, close:** confirmed damaging opaque shotgun hit on a healthy target -> modest species-specific pieces (`admitted > 0` when pool available). No special anatomy. Miss/no damage -> no pieces.
2. **Medium 129–320:** smaller/rarer response; no M2.1 anatomy cut or low-health overlay resulting from extended range. Just beyond 320 -> no G1.2 textured impact response.
3. **Head bursts:** valid zombie/Shotgun Guy fatal headshot -> existing mask plus one textured brain-like burst; unverified sprite -> no cut, safe heavy fallback; no duplicates.
4. **Low health / severe:** preserve Imp/Mancubus *living* wounds, Pinky/Caco low-health accents, Revenant bone outcome; merging rather than stacking full fragments.
5. **Rocket direct + splash:** one rocket strikes a monster directly and splashes same victim -> one event for that victim; total real damage reflected; correct fatal tier.
6. **Rocket two targets:** multiple damaged monsters -> distinct per-victim events sharing one explosion aggregate budget <=24.
7. **Barrel chain:** two actual barrel explosions within one tic -> two distinct event IDs; per-victim dedupe within each, no unintended merger.
8. **Surviving Mancubus splash:** meaningful nonlethal explosion -> heavy tissue without extra anatomical cut; low damage below gate -> no forced major effect.
9. **Cacodemon rocket kill:** large **blue** pieces, blue valid stains; Pinky red viscera; Revenant bone with no inappropriate red gore mark.
10. **Extreme death:** original death state remains exact, no fake disintegration or forced game-state change.
11. **Settings:** Blood OFF, Classic, Fragments OFF, Body destruction OFF, Head OFF, Wall OFF independently; ensure each controls only intended effects.
12. **Modes/map:** 199 vs 200 total kill-count monsters; Spectre, PWAD replacement, voxels, multiplayer, demo, accelerated renderer OFF, level transition, new game, resurrection.
13. **Budgets:** 64 combined heavy live, 24 resting, 12 per victim, 24 aggregate explosion, 768 drops, 3 wall marks/event, bounded floor extras; old resting eviction, no stalled fresh emission.
14. **Surface:** real near wall vs open dry floor, sky, lift/door/portal, floor height changes, water/liquid, ceiling and unknown geometry; no marks on invalid spans, no B sprite under ceiling.
15. **Timing/performance:** run 30/60/120 FPS visual checks for similar flight/lifetime; 64-piece pool under blast stress, trace counts bounded; compare CPU and memory against current G1 baseline.
16. **Regression:** stock M1 headless corpses, M2.1 low-health wound/drip, ordinary gun blood, audio, existing M2/G1 native smoke tests and launcher/F4 config round trip.

### Windows manual GPU playtest protocol

Use an isolated profile and `-goredebug`; existing G1 report uses:

```powershell
cmake --build .\build --config Release --target doom
$game = (Resolve-Path .\build\Release\doom.exe).Path
$iwad = (Resolve-Path .\test-references\DOOM2.WAD).Path
$run = New-Item -ItemType Directory -Force .\build\g1-2-playtest
Push-Location $run.FullName
try {
    & $game -iwad $iwad -warp 5 -skill 3 -goredebug 2>&1 | Tee-Object gore-g1-2.log
} finally {
    Pop-Location
}
```

**Note:** use the actual Doom II IWAD path and game executable. Test both default mode and `-solidmaps` separately; the latter is not a prerequisite to see airborne giblets.

| Scene | Expected, visible acceptance |
|---|---|
| MAP02, close zombie/Shotgun Guy | Small flesh on normal confirmed hit; fatal headshot: masked head + recognizable grounded brain-like pieces, directional wall blood if wall valid |
| MAP02, medium shotgun | Smaller scatter, no new anatomical effect, clear distance difference |
| MAP05, healthy Cacodemon | A few blue pieces on close shotgun hit, before low-health gate |
| MAP05, wounded Cacodemon near valid wall | More blue viscera, blue wall marks and visible floor B sprites |
| MAP05, Pinky open floor | Red viscera land and remain; corpse stays normal |
| MAP07, Mancubus | Heavy tissue on significant hit; rocket fatal burst substantial, no forced sprite hole on death |
| MAP10, Revenant | Pale bone fragments, no generic red viscera, approved skull rules unchanged |
| Rocket/SSG on two eligible monsters | Exactly one per-victim visual response for their actual damages, no duplicated rocket direct+splash |
| Barrel detonation near supported monster | Radial material-correct burst only when actual explosion damage occurs |
| Several rapid explosive deaths | Visible field remains briefly, no frame-collapse or 64-slot deadlock |
| Sky / moving door / portal / liquid | No invalid marks, no floating resting fragments |

**User visual reference:** the supplied screenshots show broad red wall/floor splashes and multiple resting chunks. Use the **third image** as the target density for significant shotgun kills and the denser first/second images as an *upper-bound style reference* for major explosive deaths. Do **not** reproduce implausibly tall vertical blood columns. Screenshots are aesthetic references only, not texture assets.

Collect the game's **real** logs, before/after GPU screenshots and preferably short video for flight/landing checks. Never claim in-game appearance from CPU smoke tests alone.

---

## 12. Completion and handoff requirements

Codex must finish with:

1. **Implementation summary:** paths changed; what stayed unchanged; new event provenance and how duplicates are prevented.
2. **Per-monster table:** close routine / medium / severe / rocket+barrel, material and observed test result.
3. **Source coverage:** which G1.1 requirements were already merged vs implemented now.
4. **Measured counters:** for each test, actual requested/admitted/drawn/floor contacts/wall contacts/real decals, at least in focused fixtures.
5. **Tests actually run:** exact commands, pass/fail output, environment limitations. Distinguish headless CPU verification from live GPU visuals.
6. **Performance:** comparison against existing G1 64-piece test baseline if feasible, not an invented FPS claim.
7. **Remaining issues:** direct/splash identity, invalid surfaces, atlas fallback, unsupported death/head art, graphical artifacts, capacity limits.
8. **Reviewable patch:** `git diff --stat` and concise diffs, no unrelated refactor or art re-generation.
9. **Manual playtest steps:** in F4 turn on Blood, Enhanced Gore, Flying flesh fragments, Body destruction & viscera and Enhanced wall blood; start with normal unmodified stock art and fewer than 200 monsters.

**Successful handoff is source implementation + tests honestly reported. Final visual approval must come from the user's real Windows gameplay testing.**

---

## 13. Suggested Codex launch instruction

> Implement G1.2 from `HELLFORGE_G1_2_EXPLOSIVE_GORE_FIELD_CODEX_PROMPT.md`. Begin by reviewing the current working tree and detecting which G1.1 corrections are already present. Extend **existing** M2.1/G1 code, not the old uploaded snapshots. Add reliable close/medium shotgun fragment feedback; route verified lethal zombie headshots to textured brain-like giblets; support genuine rocket direct/splash and barrel explosion gore with per-victim deduplication; make giblets spread, land and persist using A/B artwork; and add a bounded, geometry-validated environmental Gore Field through the existing decals. Preserve all seven-monster, stock-art, F4, gameplay and performance protections. Implement in stages, add diagnostics/tests, do not edit death animations, do not begin M3, and report precisely what compiled/tested versus what still needs GPU playtesting.

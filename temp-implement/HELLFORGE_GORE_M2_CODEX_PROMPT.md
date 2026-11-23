# CODEX IMPLEMENTATION PROMPT — HellForge Advanced Gore M2

> Paste this entire document into Codex inside the **actual, current** HellForge Doom-engine repository. M1 is already implemented, tested in-game, and approved. This is a **source-code implementation task**, not a proposal or a request for a full release build.

## Role and final objective

Act as a senior C/C++ engineer specializing in id Tech 1, sprite renderers, palette art, and SDL3/Vulkan game presentation. Implement **Gore M2: monster-specific body destruction and viscera**, extending the working Gore M1 implementation. Prioritize *visual consistency and reuse*, bounded resource costs, deterministic cosmetic decisions, and safe compatibility over effect count. Do not produce unrelated refactors, a second gore framework, or unsupported art assumptions. Inspect the real code before editing. Work to completion without stopping for planning approvals; when a feature cannot be safely enabled due to missing artwork validation, implement a correct fail-closed path and clearly report the specific blocker.

### Binding scope: exactly seven monster types

| Doom actor type | Monster | Permitted M2 major outcomes | Explicitly prohibited |
|---|---|---|---|
| `MT_POSSESSED` | Zombieman | M1 head burst; new chest/abdomen wound; **one severed arm OR one severed leg** | No living dismemberment, no simultaneous double-limb loss |
| `MT_SHOTGUY` | Shotgun Guy | Same as Zombieman | Same limitations |
| `MT_TROOP` | Imp | Head burst; chest/abdomen wound; **one severed arm OR one severed leg** | No dismemberment without verified art |
| `MT_UNDEAD` | Revenant | Skull/head destruction, limited ribcage/torso *bone* breakup, bone shards | No fleshy organ emission, no intact severed arms/legs |
| `MT_FATSO` | Mancubus | **Only chest/belly cavity** with heavy tissue/guts | No head/arm/leg severing, no complete torso splitting |
| `MT_SERGEANT` | Pinky Demon | **Viscera and directional tissue burst only** | Never cut holes or remove head/limbs |
| `MT_HEAD` | Cacodemon | **Severe fatal shotgun gut spill**, large organic clusters | Never cut holes or remove head/limbs |

`MT_SHADOWS` (Spectre) is **not** in scope even though it shares some Pinky art. No other actor types, weapons, projectile kills, chainsaw, explosives, or gameplay modifications are in scope. Existing non-gore rendering and normal death sequences must stay intact.

### Hard requirements

1. **Lethal shotgun/SSG events only**, with a player shooter; cosmetic only. Keep the M1 128 Doom-unit range for head/torso/viscera, and start with 96 units for arm/leg severing and the Cacodemon's severe gut spill. Distances are tunable constants, not new F4 sliders.
2. **Exactly one major outcome per victim per shotgun discharge.** Additional bounded droplets/fragments are allowed. Do not choose an anatomical outcome from a single late/early pellet when the rest of that discharge can be measured.
3. Reuse existing `R_BloodHit` → `doom3d::goreCapture()` pre-damage hook, `R_GoreHit` → `doom3d::goreAfterHit()` post-damage hook, `P_WeakpointDischarge()`, the M1 `GoreShot`, `goreBurst()`, `woundMask()`, `maskedRow()`, `woundVertex()`, and `buildSpray()` where appropriate. **No change to damage, health, spread, hit registration, AI, ammo, gameplay RNG, or vanilla gib selection.** New randomness must come from visual-only pathways.
4. Enforce `totalkills < 200`, Blood ON, accelerated renderer, single-player/no demo, no voxel presentation, `MF_NOBLOOD` rejection and safe level lifecycle, exactly as M1. It is **map kill population**, not live actor count.
5. All pixel removal requires **fingerprint-verified, frame-appropriate stock artwork**, and must remain correct through death/fall/corpse animation; if not verified, show only non-destructive effects. Never guess a cut on replacement WAD artwork. Effects with **no sprite cutting** may safely use monster-type checks and existing blood tinting.
6. Preserve M1 behavior for both original monsters and all existing switches. Preserve the stock pinky/Spectre exclusion. Preserve safe fallback on extreme/gib frames, Death Ragdoll, unusual DeHackEd states, death resets, resurrection, map change and unsupported art. Do not promise gore persistence across save/load unless implemented and tested; M1 was runtime-only.
7. **No new art required initially**: procedural blood, flesh, viscera and bone shards; extract original verified sprite texels for leg/arm cutouts if feasible. Do **not** add heavy shaders, texture packs, Doom actors, physics engine, or a new particle pool. If true severed part display is not visually achievable without new IWAD-derived masks, leave that individual effect OFF with diagnostic reason, not a fake blob called a detached leg.

## Source baseline — examine before editing

This source list has been inspected against uploaded M1 code. Read these *current working tree* files before editing (they may have changed after the user's M1 test):

- `platform/scene3d/wounds.cpp`: `GoreShot`, `goreShots`, `goreHeadless`, `goreFrameMasks`, `captureSpriteHit()`, `goreEligible()`, `goreStockDeath()`, `goreCapture()`, `goreAfterHit()`, `goreHeadMask()`, `woundMask()`, `forgetWounds()`, `woundsShutdown()`.
- `platform/scene3d/spray.cpp`: `Drop`, `throwDrop()`, `goreBurst()`, `buildSpray()`, `checkSprayLevel()`, global `maxDrops=768`, M1 global live flesh limit `64`, existing wall/floor/ceiling collision and decals.
- `platform/scene3d/blood.cpp`: `bloodHit()` calls `goreCapture()` before damage; `BloodSourceScope`, `monsterBloodTint()` (Cacodemon is blue), blood decals.
- `platform/scene3d/geometry.cpp`: uses `woundMask()` for sprite rendering.
- `platform/scene3d/death_falls.cpp`, `platform/death_joints.h`, `platform/death_rigs_stock.h`: verified M1 stock human rigs (`POSSH0`, `SPOSH0`); procedural sprite fall geometry and joint helpers. Do not assume rigs for Imp/Revenant.
- `platform/monster_regions.h`, `platform/monster_regions_stock.h`, `platform/sprite_eyes_stock.h`: available stock-fingerprint/region infrastructure; check what each selected monster/frame is actually verified against.
- `platform/scene3d/body_motion.cpp`: forgotten/resurrected corpse cleanup; `platform/scene3d/scene.cpp`: hook registration and unregistration; `platform/scene3d/internal.h`: shared declarations.
- `linuxdoom-1.10/p_map.c`, `r_state.h`, `r_main.c`: current M1 damage callbacks; `linuxdoom-1.10/p_pspr.c`: shotguns explicitly call `P_WeakpointBeginShot()`, loop pellets, then `P_WeakpointEndShot()`; `p_weakpoints.cpp`: end-shot clears `shotSource` and `shotId`.
- `platform/scene3d.h`, `platform/settings_panel.cpp`, `platform/settings_layout.h`: M1 saved F4 Gore options (`goreLevel`, `goreHeads`, `goreFragments`, `goreWallBlood`) and layout.

Start by checking actual names, types and current code. Do not reapply the older M1 patch. Avoid CMake changes if using existing compiled source units. For source-only delivery, there is no requirement to build the entire Windows game in Codex; run cheap syntax/unit checks where available and clearly state what was not built.

## Architectural correction — finalize AFTER the shotgun pellet loop

M1 currently emits in `goreAfterHit()` as soon as the lethal pellet is seen. That makes percentages skew toward pellets processed before death. M2 needs one final *cosmetic* decision from the fully accumulated discharge.

Implement a minimal end-of-shot renderer callback, preserving all existing hooks:

1. Continue collecting per-victim hit positions, head/chest/belly/arm/leg counts, direction, range, and visual surface information in `goreCapture()`; use actual sprite-space opaque hits and validated anatomical regions, **not only the Doom collision cylinder**. Capture data while the monster is alive; do not invent contacts afterward.
2. In `goreAfterHit()`, mark a confirmed lethal result and snapshot any necessary immutable properties (type, damage/severity, death-state information), **but do not emit** the M2 event immediately. Preserve stock M1 head rules and a safe fallback when a shot-end hook is unavailable.
3. Add a minimal callback such as `void (*R_GoreShotEnd)(mobj_t *shooter, uint64_t discharge)` alongside `R_GoreHit` in `r_state.h` / `r_main.c`; register/unregister it in `scene.cpp`. In the two shotgun functions of `p_pspr.c`, copy the discharge ID **before** `P_WeakpointEndShot()`, and call the new callback **after** `P_WeakpointEndShot()` (which clears the source and ID). This also allows any additional end-of-shot weakpoint processing to finish. Do not duplicate the hook across individual pellets or put a new hook into normal pistol/chaingun flow.
4. At end-shot, finalize each matching victim **once**, after checking that the actor pointer is still valid and belongs to the current level; do not dereference stale pointers after actor removal. A safe alternative is to snapshot enough immutable event data on death and resolve only while the actor is still in a tracked set. De-duplicate discharge/actor pairs, bound stale-record lifetime, and clear on `forgetWounds()`, resurrection and shutdown. Do not rely on raw pointer identity alone across level restarts.
5. If adding actual applied damage to the post-hit callback is necessary, extend its signature and call site consistently in `r_state.h`, `r_main.c`, `p_map.c`, `scene.cpp` and `internal.h`; avoid guessing severe damage from the victim's remaining HP alone. The severity metric must be explained and traceable. If M1 approved behavior changes, make that explicit in the change report.

### Policy data structures — use this as an implementation starting point

The following is **reference code** to adapt into `wounds.cpp` or a small existing shared header; preserve exact engine types and include dependencies. Do not create a general rule engine.

```cpp
// Inside doom3d (with the appropriate engine headers available).
enum class GoreRegion : uint8_t {
    Unknown, Head, Chest, Belly, LeftArm, RightArm, LeftLeg, RightLeg
};
enum class GoreOutcome : uint8_t {
    None, HeadBurst, ChestCavity, BellyCavity,
    ArmSever, LegSever, SkullBurst, RibBreak,
    PinkyViscera, CacoViscera
};

enum : uint32_t {
    G_HEAD = 1u << 0, G_TORSO = 1u << 1, G_ARMS = 1u << 2,
    G_LEGS = 1u << 3, G_VISCERA = 1u << 4, G_BONE = 1u << 5
};
struct GoreProfile {
    int actorType;
    uint32_t capabilities;
    float maxRange;
};
static constexpr GoreProfile kGoreProfiles[] = {
    {MT_POSSESSED, G_HEAD|G_TORSO|G_ARMS|G_LEGS,   128.f},
    {MT_SHOTGUY,   G_HEAD|G_TORSO|G_ARMS|G_LEGS,   128.f},
    {MT_TROOP,     G_HEAD|G_TORSO|G_ARMS|G_LEGS,   128.f},
    {MT_UNDEAD,    G_HEAD|G_TORSO|G_BONE,          128.f},
    {MT_FATSO,     G_TORSO|G_VISCERA,              128.f},
    {MT_SERGEANT,  G_VISCERA,                      128.f},
    {MT_HEAD,      G_VISCERA,                       96.f}
};
static const GoreProfile *goreProfile(int type) {
    for (const GoreProfile &p : kGoreProfiles)
        if (p.actorType == type) return &p;
    return nullptr;
}
static bool dominates(int regionContacts, int allContacts,
                      int minContacts, int percent) {
    return allContacts > 0 && regionContacts >= minContacts &&
           regionContacts * 100 >= allContacts * percent;
}
static bool needsSpriteCut(GoreOutcome out) {
    switch (out) {
    case GoreOutcome::HeadBurst:
    case GoreOutcome::ChestCavity:
    case GoreOutcome::BellyCavity:
    case GoreOutcome::ArmSever:
    case GoreOutcome::LegSever:
    case GoreOutcome::SkullBurst:
    case GoreOutcome::RibBreak: return true;
    default: return false;
    }
}
// Use exactly one major outcome. Apply feature toggles, artist-verified masks,
// distance, hit distribution and extreme-death checks BEFORE final selection.
// PinkyViscera and CacoViscera must NEVER enter the sprite-mask renderer.
```

Do **not** apply `goreHeadMask()` to a torso or limb as if it were a different-sized head. Replace `goreHeadless` with a compact per-corpse outcome record (including side for severed limbs and stable intensity/seed if needed). Preserve headless handling for M1 corpses. Avoid storing large image masks per actor.

### Suggested region thresholds (initial tuning, not proven gameplay values)

- Preserve existing M1 head qualification for Zombieman and Shotgun Guy: **at least one confirmed head contact and head contacts ≥ 50% of captured contacts**, lethal, verified, not extreme, Death Ragdoll OFF.
- Imp head/skull Revenant: head contacts ≥ 50%; require proper stock anatomy verification for actual removal.
- Human/Imp chest or belly: at least **2** region contacts and ≥ **40%** of captured sprite contacts, within 128 units. One cavity maximum; do not remove carried weapons.
- Human/Imp arm or lower leg: at least **2** verified contacts and ≥ **40%** of captured sprite contacts, within **96** units; one left OR right side; require reliable handedness/flip mapping and a detached segment that looks like a limb, not generic meat.
- Revenant ribcage: at least **2** contacts and ≥ **40%**; use bone shards, not organ debris; skull burst has higher priority when both qualify.
- Mancubus chest/belly: at least **3** confirmed close-region hits and substantial applied damage, starting with **60** summed effective damage as a *tunable prototype threshold*. Cavity must be bounded to actual torso art; no head/limb cutting.
- Pinky: severe lethal torso-centered shotgun discharge; initial threshold **≥3** confirmed contacts and **≥50** effective damage within 128 units; **viscera only**. Do not let silhouette classification accidentally select the mouth as a torso cavity.
- Cacodemon: severe lethal **close** shotgun/SSG discharge; initial threshold **≥4** confirmed hits, **≥70** effective damage, within **96** units; **viscera only**, blue blood tint as in `monsterBloodTint(MT_HEAD)`. Distinct heavier/clumped effect, not a reused human head burst.
- Mixed/ambiguous hits: either stock M1 heavy-impact feedback or standard death. Never force anatomically impossible cuts. Favor fewer, higher-quality occurrences.

Tune constants *after* in-game tests. A hit's contribution to severity must use actual resolved gameplay information captured read-only; do not calculate damage or call damage functions yourself. Explicitly exclude mixed hits with insufficient regional confidence. Decide how weakpoint-induced end-shot damage affects the cosmetic eligibility without changing those gameplay rules.

### Concrete shot-end hook pattern (adapt signatures/includes to the live repository)

These are **small reference edits**, not instructions to overwrite existing functions. The key is calling the end-of-shot hook when `P_WeakpointEndShot()` has finished but without relying on its cleared global shot ID:

```c
/* linuxdoom-1.10/r_state.h, near R_GoreHit (ensure uint64_t is declared): */
extern void (*R_GoreShotEnd)(mobj_t *shooter, uint64_t discharge);

/* linuxdoom-1.10/r_main.c, alongside other callback definitions: */
void (*R_GoreShotEnd)(mobj_t *shooter, uint64_t discharge) = NULL;

/* p_pspr.c, in A_FireShotgun and A_FireShotgun2, after ALL pellets: */
const uint64_t goreDischarge = P_WeakpointDischarge(player->mo);
P_WeakpointEndShot();
if (goreDischarge && R_GoreShotEnd)
    R_GoreShotEnd(player->mo, goreDischarge);
```

Register `R_GoreShotEnd = settings.accelerated ? goreFinishShot : nullptr;` in `scene.cpp`, add `void goreFinishShot(mobj_t*, uint64_t);` to `internal.h`, and clear `R_GoreShotEnd` on renderer shutdown. Preserve the existing `R_GoreHit` hook for tracking the first confirmed death, but stop emitting there. If a fatal target is deleted/invalid before the end callback, skip the corpse mask safely; never dereference a stale pointer.

### Concrete resolver sketch (pure C++; adapt after inspecting real hit data)

The table above restricts types and abilities. The resolver below specifies **priority and thresholds** so Codex need not invent a generic decision engine. Here `hits[]` is indexed by `GoreRegion`, and `effectiveDamage` is read-only accumulated **actual** hit damage in the discharge. This returns a *candidate*: validate its **specific** stock frame/mask, side and settings before committing it to corpse state. The emission still uses the original recorded impact point and shot vector.

```cpp
struct GoreDecisionInput {
    int type = -1;
    int contacts = 0, effectiveDamage = 0;
    int hits[8] = {};
    float distance = 0;
    bool lethal = false, extreme = false;
    bool headEnabled = true, bodyEnabled = true, limbsEnabled = true;
};
static int regionHits(const GoreDecisionInput &d, GoreRegion r) {
    return d.hits[static_cast<unsigned>(r)];
}
static GoreOutcome goreCandidate(const GoreDecisionInput &d) {
    const GoreProfile *p = goreProfile(d.type);
    if (!p || !d.lethal || d.contacts == 0 || d.distance > p->maxRange)
        return GoreOutcome::None;
    const int head = regionHits(d, GoreRegion::Head);
    const int chest = regionHits(d, GoreRegion::Chest);
    const int belly = regionHits(d, GoreRegion::Belly);
    const bool headDominant = head > 0 && head * 2 >= d.contacts; // M1 baseline
    const bool chestDominant = dominates(chest,d.contacts,2,40);
    const bool bellyDominant = dominates(belly,d.contacts,2,40);
    const bool armDominant =
        dominates(regionHits(d,GoreRegion::LeftArm),d.contacts,2,40) ||
        dominates(regionHits(d,GoreRegion::RightArm),d.contacts,2,40);
    const bool legDominant =
        dominates(regionHits(d,GoreRegion::LeftLeg),d.contacts,2,40) ||
        dominates(regionHits(d,GoreRegion::RightLeg),d.contacts,2,40);
    // Visual mask outcomes intentionally suppressed on vanilla extreme deaths.
    if (d.type == MT_POSSESSED || d.type == MT_SHOTGUY || d.type == MT_TROOP) {
        if (d.extreme) return GoreOutcome::None;
        if (d.headEnabled && headDominant) return GoreOutcome::HeadBurst;
        if (d.limbsEnabled && d.distance <= 96.f) {
            if (legDominant) return GoreOutcome::LegSever;
            if (armDominant) return GoreOutcome::ArmSever;
        }
        if (d.bodyEnabled) {
            if (chestDominant) return GoreOutcome::ChestCavity;
            if (bellyDominant) return GoreOutcome::BellyCavity;
        }
        return GoreOutcome::None;
    }
    if (d.type == MT_UNDEAD && !d.extreme) {
        if (d.headEnabled && headDominant) return GoreOutcome::SkullBurst;
        if (d.bodyEnabled && (chestDominant || bellyDominant))
            return GoreOutcome::RibBreak;
    }
    if (d.type == MT_FATSO && d.bodyEnabled && !d.extreme &&
        d.effectiveDamage >= 60 && (chest >= 3 || belly >= 3))
        return chest >= belly ? GoreOutcome::ChestCavity : GoreOutcome::BellyCavity;
    if (d.type == MT_SERGEANT && d.bodyEnabled &&
        d.effectiveDamage >= 50 && d.contacts >= 3 &&
        dominates(chest+belly,d.contacts,2,40))
        return GoreOutcome::PinkyViscera;
    if (d.type == MT_HEAD && d.bodyEnabled && d.distance <= 96.f &&
        d.effectiveDamage >= 70 && d.contacts >= 4)
        return GoreOutcome::CacoViscera;
    return GoreOutcome::None;
}
```

**Never combine weak contacts across left and right limbs**: after a limb candidate is returned, choose the individually qualifying side and require its own verified cutout mask. If both sides qualify, prefer the side with more valid contacts, but remove only one. `goreHeads` independently controls Revenant skull removal; `goreBodyDamage` controls rib breaking. For Pinky/Caco no art cut is ever selected. Validate the strongest candidate, then choose a lesser **supported** outcome or revert to M1 heavy-impact particles, never an invalid cut.

## Rendering and anatomy: implement three narrow presentation paths

### Path A — Humanoid cuts: Zombieman, Shotgun Guy, Imp

- Head: reuse the M1 explosion (no intact head model), apply mask to all supported death/corpse frames; no return of head on later frames.
- Chest/belly: create an irregular **bounded** cavity, with dark interior, red/darker edge, minimal wet highlights and directional tissue; avoid a transparent circular punched-card appearance. Don't cut a weapon, arms, head or full body in half.
- Arms/legs: use per-frame validated separation masks to remove **one** suitable segment. Start with below-knee legs and forearm/one arm section, based on available stock rig and pixel occupancy. Use the original, verified sprite texels as a cutout card if feasible, with lightweight spin/fall/bounce and a bounded lifetime/landing mark. No gameplay actor or physical collision for a severed part. Ensure the detached segment moves in the shot direction then falls, and the corpse is visually missing the corresponding piece. If stock masks cannot be verified, emit normal flesh fragments instead and mark this *specific* cut unsupported.
- Imp is **not** pre-validated by `death_rigs_stock.h`; generate a stock-only anatomy profile or an IWAD-derived fingerprint/mask helper. Never silently apply human limb heuristics to Imp.

### Path B — Revenant skeletal breakup

- Head/skull can explode without an intact floating skull object; bone-white/tan shards travel outward. Skull must remain absent across validated frames.
- Ribcage/torso can fracture visibly with irregular gaps or bounded detached bone sections; *do not* apply dark-red human torso wound colors to skeletons. Use existing bone palette or palette-derived off-white/tan and optional restrained blood.
- Use fingerprinted stock frame masks. If the skeleton's death animation cannot support the selected cut, fall back to bone shards without modifying the sprite.

### Path C — Large demons

- Mancubus: localized *chest or belly cavity* (not complete torso demolition) plus larger tissue and organ-like clumps; require verified full death-frame masks for persistent cavity. If the mask isn't validated, keep the guts and blood but **do not cut** the art.
- Pinky (`MT_SERGEANT` only): torso-centered viscera emission on severe lethal shotgun hits. **No wound holes, anatomical removal or changes to corpse geometry.** Remains recognizable; ordinary death animation continues.
- Cacodemon (`MT_HEAD`): large, concentrated gut spill in the shot direction, with visibly heavier pieces and blue-tinted blood using existing species tint. Give 2–3 large clusters **short trailing multi-lobe silhouettes**, arranged along the same ballistic arc to suggest expelled intestine rather than unrelated spheres; keep each cluster lightweight and bounded. **Never apply `goreHeadMask` or body masks.** Use existing gravity and collision. No need to create a worm mesh, complex flexible body, or actor sprites.

### Geometry/mask rules

- `Image::pixels` stores paired **palette index and coverage** bytes; only remove/paint opaque original texels. Keep `woundMask()` and `maskedRow()` as the integration point. Mask `1` removes texels; mask `2–5` controls rim/stain per current renderer.
- The existing M1 `goreFrameMasks` is keyed only by `const Image*`, suitable for a single head cut. M2 **cannot reuse that key unmodified** for multiple outcomes and sides. Use a compact key containing at least `Image*`, outcome, limb side, and any discrete mask variant; share validated cached masks, not allocations per corpse. Preserve merging precedence with regular wounds.
- Stock validation must use actual sprite lump name + dimensions + pixel coverage/palette hash and relevant death/corpse frames. The M1 `goreStockDeath()` currently knows only `POSSH0` and `SPOSH0`; generalize **without** weakening its fail-closed behavior. Existing stock rigs are not universal anatomy masks. Use verified masks/landmarks, not box coordinates arbitrarily intersecting weapons.
- Properly account for rotation, mirrored frames, standing/pain frames, Death Falls deformation, death/corpse transitions, and `lyingOf()` fallback. Avoid changing atlas/shader architecture. Check `Death Ragdoll` explicitly: if it cannot carry cuts, suppress mask outcomes and only show bounded effects (as M1 does).
- Remove all renderer-local state on resurrection and level changes; do not expand savegame format in M2 unless there is a separately justified and tested need. Decomposition must not reintroduce removed geometry.

## Effects: update `spray.cpp` without expanding pools

Extend the existing M1 `Drop` presentation with a compact visual class (e.g. `Blood`, `Flesh`, `HeavyTissue`, `OrganClump`, `BoneShard`) rather than a second particle simulation. Reuse `throwDrop()` and its collision, blood decals, gravity and palette tint. Use small multi-block silhouettes in `buildSpray()`; avoid texture allocations and additional render passes. Preserve regular blood/drop behavior; don't change normal hit spray globally.

Budget targets (shared, not cumulative per category):

| Resource | M2 initial maximum |
|---|---|
| All live flying **gore** fragments combined | **64**, including M1 flesh |
| Existing complete blood/drop vector | **768** (do not enlarge) |
| Ordinary head burst | **≤6** large fragment events (M1 baseline) |
| Human/Imp torso outcome | **≤5** tissue pieces |
| Severed-limb outcome | **1** verified cutout + **≤4** small pieces |
| Revenant bone outcome | **≤6** bone shards |
| Mancubus visceral outcome | **≤7** pieces (mix of small/heavy) |
| Pinky/Cacodemon visceral outcome | **≤6 / ≤8** pieces respectively |
| Extra wall decal attempts/event | Existing M1 maximum **3** actual wall marks; reuse decal budgets |
| Simultaneously visible detached limb cutouts | **≤8**, or fewer if needed for stable performance |

These are design ceilings, **not** measured performance guarantees. Keep all new paths disabled at `totalkills >= 200`, even if only one monster is alive. Avoid per-frame image segmentation, per-frame hash calculation for every monster, unbounded actor maps, or new mesh buffers per death.

Use actual `BloodSourceScope`/`bloodColor()` species tint so Cacodemon blood is blue. Bone needs an appropriate neutral/pale bone palette; do not automatically tint skeleton bone chips like red blood. Distinguish fragment silhouette/speed/weight, not just particle count. Align launch vectors with shot direction; respect occlusion and existing `firstSolidWall()`/`bloodWallSplat()` validation. No decals on sky/invalid sectors.

## F4 and persistence

Keep M1 exactly: `goreLevel` (Classic/Enhanced), `goreHeads`, `goreFragments`, `goreWallBlood` and their existing persisted option keys. Add ONLY two new toggles, default ON in Enhanced mode:

- `gore_body_damage` — "Body destruction & viscera": humanoid torso cavities, Revenant rib break, Mancubus chest/belly cavity, Pinky/Cacodemon gut spill (dependent on fragment setting for visible chunks).
- `gore_limbs` — "Arm & leg dismemberment": only Zombieman, Shotgun Guy and Imp, and only for verified limb masks.

Suggested `Settings` members: `int goreBodyDamage=1; int goreLimbs=1;` in `platform/scene3d.h`. Register checks in `settings_panel.cpp`, route keys to `{"Combat","Gore"}` in `settings_layout.h`, and verify actual `graphics.cfg` persistence with the existing option registry. Do not introduce one setting per monster or one slider per severity threshold.

When Gore=Classic, Blood OFF, or `totalkills>=200`, all **new** M2 cosmetics are OFF. Existing M1 head behavior and flags must remain unchanged unless the global master is disabled. If `goreFragments=0`, limb/cavity masking must still be logically consistent, but flying pieces are suppressed; if no tangible limb cutout is possible without the fragment setting, explicitly choose the safe/consistent fallback. No hidden enabling of disabled particle effects.

## Implementation sequence (do not skip gates)

**Phase 1 — Unify decision pipeline:** Inspect M1, add shot-end finalization, compact seven-profile table, hit region counts, severity accounting, duplicate prevention, debug reporting and tests of pure policy. Verify existing Zombieman/Shotgun Guy head burst remains stable before altering rendering.

**Phase 2 — Non-cut gore first:** Add distinct procedural tissue/viscera/bone fragments, their bounds and collision behavior. Complete Pinky (viscera only), Cacodemon (severe viscera only), and a safe skeletal fallback for Revenant. Validate `MT_SHADOWS` exclusion and blue Cacodemon blood.

**Phase 3 — Supported body masks:** Extend shared cached mask architecture; implement Zombieman, Shotgun Guy and Imp chest/belly cavities, Revenant skull/rib effects, Mancubus torso cavity. Validate full death/corpse frame coverage; if a type lacks reliable artwork masks, disable cutting for that type and preserve Phase 2 effects.

**Phase 4 — Humanoid severing:** Implement **one** left/right lower-leg or arm sever and detached matching art for Zombieman/Shotgun Guy/Imp. Respect weapon overlaps, flips, fall frames, corpse disappearance/resurrection, and detached part budget. Fail closed individually for any unsupported actor/frame. Don't implement a universal anatomy solver.

**Phase 5 — F4, tests, packaging:** Add two persisted options, run available cheap tests, review regressions/performance gates, produce small modified-file list, source diff/patch and concise manual-test matrix. Do not do a time-consuming complete Windows build unless it is already available and cheap; user will test in VS Code/Windows.

## Required targeted tests / acceptance criteria

1. Zombieman and Shotgun Guy M1 headshots remain unchanged; no two events per discharge. Imp head burst works only when verified. Same for Revenant skull but with bone shards.
2. Aim at head/chest/belly/left arm/right arm/left leg/right leg separately. Qualifying lethal shot chooses **one** correct result; no misclassification of a carried weapon as a limb. Normal nonlethal hits remain normal.
3. Full death-to-corpse sequence from front and side: removed part never returns, holes do not wander or cut unrelated anatomy; procedural falls and classic deaths checked; ragdoll retains safe fallback.
4. Detached limb matches the amputated body region and lands convincingly; no default generic meat blob masquerading as a leg.
5. Revenant bone shards and rib breaks are visually distinct from flesh; Mancubus only chest/belly cavities; Pinky/Cacodemon never show holes or missing anatomy; Cacodemon blue blood. Spectre and all other monsters unchanged.
6. Shotgun and SSG only. Higher range, nonlethal shot, ambiguous spread, unsupported stock/replacement frames, voxel and demo/net games fall back safely; unknown art never gets a cut.
7. `totalkills=199` allows if other gates hold; `totalkills=200` completely blocks M2. No new per-frame unbounded allocations. Enforce common 64 fragment/768 drop/3 wall-mark budgets across all types.
8. With Classic Gore, Blood OFF, `goreFragments=0`, `goreBodyDamage=0`, `goreLimbs=0`, verify switches independently, alongside Wound Holes and Critical Parts off/on. Confirm F4 values persist in `graphics.cfg`.
9. Save/load, resurrection, level transitions, sprite replacement WAD, rapid consecutive deaths and renderer shutdown: no stale pointer, reused mask, leaked effects, or cross-level state. State M1 runtime-only save limitation if still present.
10. No change to damage, shot spread, ammo consumption, map kill counts, existing RNG behavior, death state selection or ordinary monsters. Provide `-goredebug` event line with type, discharge, contacts by region, applied damage, chosen outcome, art validation and fallback reason.

## Deliverables and report format

- Make the source edits **directly in the current VS Code/Codex workspace**, not in a parallel toy project.
- Keep the diff as small as practicable; modify existing modules first. Create a small additional `gore_policy.h` or test only if it genuinely reduces coupling or catches bugs. Don't change CMake solely to add a feature that fits existing units.
- Return: (1) modified files, (2) implemented and per-monster feature matrix, (3) what is verified vs fallback/unverified, (4) unit/syntax tests actually run and their results, (5) exact Windows in-game test sequence and debug command, and (6) issues requiring IWAD-derived masks or visual tuning.
- **Do not claim full visual approval, a successful Windows build, or complete seven-monster dismemberment if it has not happened.** If missing IWAD makes stock-mask validation impossible, implement reusable code and no-cut effects, give the exact automated sprite-fingerprint/mask extraction task needed, and leave unvalidated cuts disabled.
- No generic brainstorming or token-heavy reports. Edit code, keep tested M1 intact, and show concise evidence of actual changes.

**BEGIN IMPLEMENTATION NOW.**

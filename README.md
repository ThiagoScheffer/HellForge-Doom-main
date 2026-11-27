# DOOM: HELLFORGE by Thiago Scheffer

**Classic DOOM. A more physical world. Your rules.**

**Windows x64 · C / C++ / C# · SDL3 · Vulkan · GPLv3**

[Play and configure](#download-and-play) · [Latest features](#whats-new-in-hellforge) · [Combat immersion](#kinetic-horror-and-physical-bodies) · [Feature catalog](#feature-catalog) · [Build from source](#building) · [Developer resources](#developer-resources) · [Credits](#source-and-credits)

![HELLFORGE launcher artwork](launcher/hero.png)

DOOM: HELLFORGE is a personal Windows fork of the classic DOOM source, built on
Tuin-Doom-Enhanced and the DERP / Duct-Tape DOOM renderer. It preserves the
recognizable pixel-art identity of DOOM while expanding its combat presentation,
lighting, destructible environments, monster behavior and the atmosphere of
rooms left behind after a firefight.

Play close to the original game or tune an immersive horror experience through
**F4**. Cosmetic enhancements are separate from optional gameplay-changing rules,
so players can choose the experience they want. The source, native tests and
feature reports are available for developers interested in how the engine works.

> Why? Because we love DOOM. I wanted to build something personal, shaped by
> my own taste. — Thiago Scheffer

**Source update — 28 November 2026.** The latest source includes Smart
Persistent Lighting Cache 2.0 (M1), outdoor fog coverage for sprites and
voxels, clearer lightning bolts, and acid and lava reactions for corpses
and fragments. See [what's new](#whats-new-in-hellforge) for controls and
verification. The documented build identifier is unchanged.

**Documented source baseline: `2026.11.15.6`** · [Build instructions](BUILDING.md) ·
[Documentation](docs/README.md) · [Change history](docs/changes) · [Bug and debugging log](Log-Status.md)

## Download and play

1. Build or obtain `DOOM-Hellforge.exe`.
2. Launch it and select a compatible Doom, Doom II, Final Doom, or Freedoom IWAD.
3. Add WAD/PK3 add-ons in **ADD-ON LOADOUT** and choose the active level WAD in **MAP WADS**. The selected map launches first, followed by the add-ons in load order. Use **PLAY GAME** to start.
4. Use **GAME SETTINGS / TABS** before launching, or **F4** in-game, for graphics, lighting, combat and other preferences. Apply your changes to save them. Use F4 Find to locate an option by name or key.

Launcher quick tools open the last log and graphics config. **CLEAR ALL / FRESH
START** backs up and resets preferences/add-ons while keeping saves and IWADs.

Commercial IWADs are not included. Windows x64, a Vulkan-capable graphics
driver, and .NET Framework 4.5+ are required for the supplied launcher.

The launcher stores extracted resources, backups, crash logs, and its lighting
cache under `%LOCALAPPDATA%\DOOM-HellforgeLauncher`. Game settings and saves
use `%APPDATA%\Local Games\DOOM Hellforge`; screenshots are saved beside the
launcher in `Screenshots`.

## What's new in HELLFORGE

Recent updates improve level reloads, storm visibility, and the way bodies,
fragments, and hazardous liquids interact. F4 controls the disk cache, visible
lightning, and corpse reactions.

| Highlight | What changes in the game |
| :--- | :--- |
| **[Smart Persistent Lighting Cache 2.0](#smart-persistent-lighting-cache-20--m1)** | Compatible lighting stages restore on restart, save load, and map revisit. The loading screen distinguishes cache restoration from baking. |
| **[Universal Atmospheric Fog](#universal-atmospheric-fog--sprite-and-voxel-coverage)** | Outdoor sprites and voxels receive the active haze at their world position. Indoor exposure follows the same sector rules as nearby geometry. |
| **[Lightning Bolt Visibility 1.1](#lightning-bolt-visibility-11)** | Branching bolts stay readable across two pulses and a short fade, including when the diffuse flash subsides. |
| **[Environmental Corpse Reactions](#environmental-corpse-reactions-10)** | Corpses, supported ragdolls, and gore fragments can dissolve in acid or char and collapse in lava. |
| **[Incendiary Barrels](#incendiary-barrels-10)** | Optional destructible burning barrels leak oil when hit and leave damaging flames when ruptured. |
| **[Kinetic Horror](#kinetic-horror-and-physical-bodies)** | More expressive impacts, physical fragments, temporary wall adhesion, soft material movement, wet deposits and positional impact audio. |
| **[Physical Bodies](#physical-bodies-10--environmental-contacts-and-stable-ragdolls)** | Supported monster sprites become physically posed bodies that react to walls and floors, settle and retain their generated pixel-art appearance. |
| **[Deep Wounds & Visceral Giblets](#deep-wounds-and-visceral-giblets)** | Latest developer-tested cosmetic batch: shaded wound presentation and more recognizable fragment artwork, without new gameplay injury rules. |
| **[Living liquid surfaces](#living-liquid-surfaces--acid-and-lava-bubbles)** | Optional bubbles form and pop on selected animated acid and lava floors, with subtle ripples. |
| **[A living environment](#recent-atmospheric-rendering-and-cosmetic-fx-updates)** | Moving skies, soft sun shafts, animated textures, corpse scavengers and organic decay. |
| **[Survival and encounter AI](#latest-survival-and-encounter-updates)** | Optional feeding, retreat, pursuit, infighting and encounter controls, independent of cosmetic gore. |
| **[Modern HUD and launcher](#recent-gameplay-hud-environment-and-ai-updates)** | Configurable run statistics, reusable add-on loadouts, persistent settings and medical recovery options. |

## Kinetic Horror and Physical Bodies

HELLFORGE's combat presentation combines palette-aware sprite artwork, cosmetic
physical simulation, lighting and sound. These systems retain the game's own
damage, monster states and gameplay movement. Existing optional combat rules such
as critical parts and monster weak points remain separate.

### Kinetic Horror 2.0 — physical gore and impact feedback

**Advanced Gore** offers Classic, Enhanced and Horror modes. Enhanced remains the
established default; Horror is an opt-in presentation profile.

- More expressive body reactions and coordinated cosmetic impact feedback.
- Physical giblets that scatter, collide with surfaces and settle. Selected soft
  fragments can temporarily cling to solid walls, flatten, sag and detach.
- Thick surface deposits, existing wall runoff and temporary wet highlights that
  respond to scene lighting before drying.
- Material-specific movement and positional impact sound using the existing
  procedural sound bank.

Use **F4 → Combat** (or Find) to select **Advanced Gore → Horror**. Enable Blood,
Flying flesh fragments and the relevant wall-blood options. Heavy impact reactions,
Sticky wall fragments, Soft fragment deformation, Wet gore deposits and Heavy
impact sounds can be controlled independently; Impact strength ranges from
0.5 to 1.5. Existing prerequisites still apply, including Wet blood shine for wet
deposits and hit/death reactions for heavy body motion.

**Under the hood:** a bounded, fixed-step cosmetic fragment system uses the
existing palette-aware atlas, collision queries and decal manager. Wall anchors
are validated against their supporting geometry. The 64-piece total, 24 combined
stationary pieces, eight attached-piece subcap and 12-piece burst limit are
retained. Advanced Gore is restricted to eligible maps with fewer than 200
kill-count monsters. The limits are resource policies, not measured FPS gains.

[Settings registry](duct-tape-doom-main/platform/settings_panel.cpp) ·
[Fragment simulation interface](duct-tape-doom-main/platform/gore_giblets.h)

### Physical Bodies 1.0 — environmental contacts and stable ragdolls

Ragdoll deaths bring the original supported monster sprites into an articulated
relief representation without requiring replacement 3D models. Natural and
Cinematic modes build on that foundation with more coordinated settling after
supported wall contacts and steadier generated pixel-art images.

| F4 → Combat → Deaths | Purpose |
| :--- | :--- |
| **Ragdoll deaths** | Opt in to rigged sprite physics for supported monsters (off by default). |
| **Flying ragdolls** | Allow the existing cinematic blast movement and wall rebounds (off by default). |
| **Physical Bodies: Legacy** | Retain the established ragdoll presentation (default). |
| **Physical Bodies: Natural** | Enable restrained contact assistance and stable sprite reconstruction. |
| **Physical Bodies: Cinematic** | Stronger contact damping; Flying ragdolls remains a separate option. |
| **Environmental body contacts / Stable ragdoll sprites** | Independently control the two new improvements. |

**M1 — Environmental contacts:** horizontal hip motion is damped briefly after a
validated torso contact with a fixed wall. Open-floor falls and blast rebounds
bypass this additional assistance. The original 11-point solver, four substeps
and six constraint passes remain unchanged.

**M2 — Stable reconstruction:** projected pixel alignment is anchored to the
initial spine reference. Expensive sprite reconstruction is budgeted separately
from physics, allowing three ordinary and two reserved urgent builds per game tic.
Deferred bodies reuse a valid cached image with matching placement, or an ordinary
death sprite while waiting for their first card. Settled images are cached after
successful final reconstruction.

**Acceptance:** the developer reported that M1/M2 gameplay testing passed, with
a substantial improvement in immersion, and committed the source. No new frame-time
benchmark is claimed. The visual corpse can settle away from its underlying
actor; feeding and resurrection still use the gameplay actor. Unsupported artwork,
floating monsters and matching voxel models may use their existing presentation.

[Validation guide and test commands](docs/physical-bodies-validation.md) ·
[Ragdoll implementation](duct-tape-doom-main/platform/scene3d/ragdoll.cpp)

### Deep Wounds and Visceral Giblets

The latest Kinetic Horror 3.0 development focuses on a simpler visual extension
rather than independent skeletal limbs or multiple ragdolls. Deep Wounds is
intended to give damage artwork an opaque, recessed appearance instead of the
transparent openings of classic Wound Holes. Recognizable original pixel-art
fragments add visual variety to the existing material-based giblet system.

The approved design separates cosmetic appearance from simulation: major fatal
appearance variants are applied only after the game has resolved the death, and
featured artwork substitutes for existing admitted giblets rather than expanding
simulation budgets. The existing animations, gameplay damage and physics rules
remain the authority. No additional skeleton is required.

**Development status:** the developer reported that testing of this latest batch
passed. Exact final option keys, asset inventory, supported species and the scope
of the ragdoll appearance bridge have not yet been confirmed in the public source
snapshot used for this README update. In particular, the optional Lost Soul skull
and smoke effect is not listed as a confirmed feature here. The latest local test
acceptance is not a claim of exhaustive compatibility or performance certification.

### Recommended immersion setup

Start with accelerated sprite rendering and enable **Blood splats and pools**,
**Blood spray and trails** and **Wet blood shine**. Choose **Advanced Gore → Horror**
and leave its relevant child effects enabled. Under **Combat → Deaths**, turn on
**Ragdoll deaths** and select **Natural** first. Try **Cinematic** and separately
**Flying ragdolls** to compare more expressive presentation. Keep **Stable ragdoll
sprites** enabled. Use the latest wound presentation options when available in your
build. These are suggested preferences, not changed defaults.

For atmosphere, try the existing Immersive Sky Environment, Living Surfaces,
Decoration Immersion and Corpse Decomposition individually. On very large maps,
start with lighter visual settings and add effects gradually; expensive lighting,
reflections and destruction can still dominate frame time.

### Living liquid surfaces — acid and lava bubbles

Under the existing **Living Surfaces / Texture Immersion** controls, **Acid bubbles**
and **Lava bubbles** add procedural surface activity to nearby animated nukage and
lava floors. Acid uses small forming and popping bubbles; lava has slower molten
bubble motion and subtle glowing rims. Both add ripples using the existing surface
presentation. The options are subordinate to Living Surfaces and are cosmetic;
existing hazardous-floor rules remain unchanged. These settings are present in the
public settings registry. No separate performance benchmark is available.

**Lighting tip:** if Sky light produces uneven indoor transitions, try enabling
Window light alongside it (with Baked static lights where required). This resolved
the developer's observed configuration issue; it is not a claim that every baked
lighting artifact has been eliminated. The current Sky light tooltip also recommends
this combination.

## Physical Environment Lighting 1.0

Choose **F4 → Lighting → Illumination model**. The selection is saved as
`lighting_model` in `graphics.cfg`.

| Model | Interior lighting |
| :--- | :--- |
| **Enhanced** (default) | Keeps the established sector lighting, minimum brightness and room-darkening controls for maximum WAD compatibility. |
| **Source-driven Hybrid** | Keeps 25% of authored interior ambient light, drops the minimum and reduces how much sector brightness limits recognized fixture output. |
| **Physical Environment** | Keeps 5% of authored interior ambient light, drops the minimum and lets recognized powered fixtures emit independently of sector brightness. |

The two source-driven models require accelerated 3D, baked static lights and
emissive textures. Disabling any prerequisite uses Enhanced and preserves your
selection for when all three are enabled again. F4 enforces these dependencies
and disables the Enhanced room-darkening controls while a source-driven mode is
active. The launcher's generated catalogue lists the choices without enforcing
their runtime dependencies. The classic software renderer keeps its existing
lighting. Switching modes refreshes direct light, bounce and the baked sprite
grid when enabled. Map-lighting stages use the effective mode where it changes their inputs.

Outdoor ambient policy and existing sun, sky and window-light algorithms
continue to apply. Outdoor lamp classification still reads the saved
room-darkening settings. Door spill, sector light flow and nighttime window spill
still use authored-light approximations. Flicker and lamp destruction update
direct light. Some indirect light can remain until a full bake refreshes it.
Maps that rely on invisible authored lighting can appear darker. Performance
impact has yet to be established.

### Material light coverage

In Hybrid and Physical, clipped wall spans use the brightness and position of
their visible emissive texels. Narrow floor and ceiling strips can cast light
where they overlap a glowing repeat, including when the full repeat's center
falls outside the sector. BSP pieces share one sample per connected repeat.
Indicator-only point groups reach up to 88 map units and display-only groups
up to 128.
Ordinary fixtures, diffusers, fire, glass and custom masks retain their existing
point radii. Bake-only pools still use the area-light path. Clipped coverage
has a work limit, and full-repeat samples remain available after that limit.

Extinguished emitters retain the nominal data needed to light up again.
Off or destroyed fixtures release live slots immediately in the source-driven
presets. Powered sources still fade as they go out of range. Decorative light
and fixture fog copies use their current power. Existing animated texture
frames update their live output, and permanent emissive material replacements
refresh the direct bake and its dependents.

For custom artwork, place a same-size PNG in `Emissive/walls/NAME.png` or
`Emissive/flats/NAME.png` beside `graphics.cfg`, or point `-emissive` at a
different mask directory. PNG brightness and alpha define emission over opaque
artwork. A valid black mask disables emission. A wrong-size or unreadable
mask falls back to the built-in rules. An explicit mask can also enable an
`OFF` variant, with destroyed instances still taking precedence. Reload the
renderer resources after editing masks. Cache identity follows any changes
to the generated light sources and the resolved mask data used for
stained-glass tracing.

### Smart Persistent Lighting Cache 2.0 — M1

**F4 → Lighting → Baked at level load → Cache map lighting** is on by default.
The disk cache can restore sunlight coverage, direct light, the static sprite
grid, bounce light, ambient occlusion and sky visibility, and sector light
flow. Each stage checks the map, material, and lighting data it actually uses.
Level preparation still builds the geometry, stages GPU uploads, and applies
live changes such as doors and broken lamps.

The first load after upgrading fills the new cache entries. The Windows
launcher and direct engine use
`%LOCALAPPDATA%\DOOM-HellforgeLauncher\Cache\Maps` by default. The cache keeps
its 1 GiB disk budget and its 64 MiB uncompressed limit per entry. Oversized
stages calculate normally while other compatible stages can restore.

In Release/Vulkan tests, E1M1 restored all six stages on death restart with zero
bake dispatches. In Doom II MAP01, ten warm restarts and ten warm save reloads
each restored all six stages every time. The MAP01 warm preparation medians
rose from 705 to 771 ms for restart and from 698 to 757 ms for save reload.
Sunder MAP02 restored five stages and recalculated direct light, whose output
exceeded the entry limit. [Dependency audit, benchmarks, and test results](docs/changes/RELOAD-LIGHTING-CACHE.md#smart-persistent-lighting-cache-20--m1)

## Feature catalog

The broader feature set includes both recent changes and earlier implementation
reports. Historical validation results below retain their original test scope.

| Feature | What players get | Engineering highlights |
| :--- | :--- | :--- |
| **[Speedrun HUD 2.0](#speedrun-hud-20)** | Classic, Colored and Dynamic stats, color-coded health/armor/ammo, key indicators and optional text shadow. | Semantic and state-dependent colors, configurable intensity, existing layout and HUD-only behavior retained. |
| **[Medical Supply Recovery](#medical-supply-recovery-10)** | Unused healing remains on the ground as a small recovery pack, available for later pickups. | Exact integer healing conservation, variable-value packs and partial consumption; classic behavior remains available. |
| **[Corrosive Barrels 2.0](#corrosive-barrels-20--shared-liquid-effects)** | Acid splashes adhere to walls, run downward and coat exposed sprite monsters with dripping green liquid. | Shared blood-liquid physics, decals and procedural textures with independent acid material ownership; cosmetic additions. |
| **[Rain and blood effects](#rain-and-blood-effects)** | Rain lightens and spreads exposed blood while sheltered blood retains its normal appearance. | Existing cosmetic effect uses deposit age and current rain exposure; it does not accumulate rainfall over time. |
| **[Flashlight Monster Awareness](#flashlight-monster-awareness-10)** | Idle monsters can detect the player when directly illuminated, even when facing away. | Reuses the flashlight beam, visibility checks and normal idle-to-chase transition; optional gameplay behavior. |
| **[Imp Fireball Avoidance](#imp-fireball-avoidance-10)** | Imps can skip fireball attempts predicted to be blocked by another living Imp. | Bounded projectile-path queries, normal chase fallback and pre-spawn revalidation; geometry coverage and performance remain unverified. |
| **Immersive Sky Environment** | Moving layered clouds and distant rocky, icy, industrial, or infernal horizons, selectable through F4. | Three parallax horizon layers; WAD-sky-tinted pseudo-volumetric clouds; continuous wind; Off/Auto/manual profiles without changing map geometry or sunlight bakes. |
| **Soft Sun Shafts 2.0** | Smoother, less pixelated beams with gradual edges and depth transitions. | Removes 32-level alpha quantization and ordered dithering in the sunbeam shader; replaces stepped noise motion with continuous drift. |
| **HUD bloom ghosting fix** | HUD text and graphics no longer cast misplaced dark copies across bright emissive surfaces in the tested configuration. | Corrected bloom-mask sampling for the three-layer HUD texture atlas while retaining emissive textures. |
| **Progressive Monster Skin Damage** | Injuries, scratches, and blood increase as supported living monsters lose health. | Three health-driven artwork tiers; stock-sprite identity checks; reusable cached variants; independent of Wound Holes and gameplay bleeding. |
| **Corpse & Decoration Immersion** | Delayed flies, ground worms, and corpse-specific scavengers bring small encounters and biological decorations to life. | Reusable ambience profiles, deterministic selection, existing particle renderer, resurrection cleanup, and a hard cutoff at 200 spawned kill-count monsters. |
| **Corpse Decomposition & Organic Decay** | Selected corpses progressively dry and discolor; at Stage 2 their scavengers and associated blood pool disappear. | Map-wide gameplay-time aging, 2/5/8/10-minute F4 delays, eight stock monster types, cached palette-aware sprite variants, owned-pool cleanup, and the same 200-monster performance gate. |
| 🎯 **[Sprite-based headshots](docs/changes/CRITICAL-PARTS.md)** | 2× human headshots, per-pellet hits, and shots through transparent sprite gaps. | Uses the displayed frame, rotation, mirroring, and silhouette; shares impact coordinates with attached wounds. |
| 👁️ **[Monster weak points](docs/changes/ENHANCED-MONSTER-COMBAT.md)** | Heads, eyes, faces, brains, and mechanical legs have distinct combat rules. Weapon choice matters. | Monster profiles, conservative region classification, projectile provenance, and damage multipliers that do not stack. |
| **[Elemental Damage 2.0, combat notifications & Speedrun HUD](docs/changes/ELEMENTAL-DAMAGE-2.md)** | Optional Imp/Mancubus fire damage, blue plasma burns, charred Cacodemons, colored combat events and session metrics. | One authoritative DoT stream per actor, v6 status saves, bounded event queue and event-based session history that survives checkpoint reloads. |
| **[Elemental combat & injury reactions](docs/changes/ELEMENTAL-COMBAT.md)** | Cosmetic fire, blue plasma burns, charred corpses, Mancubus stagger, wounded Baron aggression and Arch-vile health siphoning. | Confirmed health-loss provenance, bounded local donor queries, immutable corpse artwork variants and versioned actor status saves. |
| 👁️ **[Eye Injuries 2.0](docs/changes/EYE-INJURIES.md)** | Blinded Cacodemons and Pain Elementals show closed, bloodstained eyes until they recover by eating a corpse. Eye hits can also provoke ten seconds of Spider Mastermind rage. | Procedural stock-sprite variants, fingerprint-validated eye masks, suppressed injured-eye glow and shared texture caching. |
| 💥 **[Impact feedback & frenzy](docs/changes/MONSTER-FEEDBACK-AND-STATUSES.md)** | Metal-leg sparks, headshot confirmation, and close super-shotgun hits that can provoke infighting. | Confirmed-impact events, per-discharge bookkeeping, bounded particles, and preserved attacker attribution. |
| **[Survival escape & frenzy retaliation](docs/changes/SURVIVAL-ESCAPE-AND-FRENZY-RETALIATION.md)** | Wounded Cacodemons/Pinkies retreat to feed; Arch-viles hunt living prey; same-species frenzy opponents can retaliate after frenzy ends. | Bounded searches, earned healing, timed combat fallback and opponent-specific missile provenance. |
| 🩸 **[Progressive bleeding & survival AI](docs/changes/MONSTER-SURVIVAL-AI.md)** | Three bleeding tiers; wounded Pinkies and Cacodemons can seek food and recover health. | Timed actor statuses, bounded food searches, collision-aware approaches, lifecycle cleanup, and versioned save extensions. |
| 🗺️ **[Large-WAD compatibility](docs/changes/BIG-WAD-COMPATIBILITY.md)** | Larger textures and maps can load beyond original engine limits. | 32-bit texture offsets, uncompressed XNOD support, checked allocations, blockmap rebuilding, and configurable zone memory. |
| ⚙️ **[Settings that stay saved](docs/changes/F4-SETTINGS-PERSISTENCE.md)** | Applied F4 choices survive quitting, Low-FX launches, and settings export/import. | A comment-tolerant config reader and same-directory temporary writes that preserve the previous file on failure. |
| ↩️ **[Use-to-load after death](docs/changes/DEATH-SAVE-RECOVERY.md)** | Press Use after dying to return to the last save written or loaded in the current session. | An optional death-flow hook into the normal loader, with ordinary map restart when no usable save is available. |
| **[Launcher tools & settings tabs](docs/changes/LAUNCHER-TOOLS-AND-SETTINGS.md)** | New forge icon, animated Arch-vile/fire, log/config shortcuts, backed-up fresh start and tabbed preferences. | 355 F4/balance controls, config-preserving edits and IWAD-sourced animation; saves remain intact. |
| **Launcher map and add-on loadout** | Keep a reusable add-on selection while switching between separately listed map WADs. | Map WADs are detected from map markers; only the selected map is launched before the ordered add-ons. Existing mixed lists migrate automatically. |
| **Organized F4 settings** | Browse settings through 16 categories with focused sub-tabs, or find a setting by name/help text. | Preserves the existing settings registry and persistence; Q/E cycles sub-tabs, F opens Find, and search results edit the same settings draft. |
| **Original Doom monster preview** | The launcher shows a Baron of Hell in the selected place for original Doom and Final Doom IWADs, and an Arch-vile for Doom II. | Uses the selected IWAD's sprite and palette for the preview; this is launcher artwork selection and does not change gameplay actors. |
| **Oku large-map geometry visibility** | Conservatively skips world geometry outside the camera view on large maps. | On the 64-view Oku harness, world preparation went from 8.58 ms to 4.73 ms and generated triangles from 122,036 to 35,068 per view. Full visual parity and moving-door coverage remain open; mirrors, reflections, probes, baking, and uncertain geometry use the existing path. |
| **Moving-sector baked-light relighting** | Reduce long CPU pauses when doors and other moving sectors trigger baked-light updates. | Oku live no-monster traversal improved from 28.07 to 33.22 FPS; median frame time changed 28.147 to 26.477 ms, p99 252.692 to 70.801 ms, and max 337.400 to 77.471 ms in the same harness. Background bake time was unchanged. |
| **[Encounter Director](docs/changes/ENCOUNTER-DIRECTOR.md)** | Independent monster randomization, hidden reinforcements, category-safe item randomization and original-population staging. | Protected progression actors, native placement checks, separate budgets, single-player eligibility and nested F4 tabs. |
| **[Exact solid-world sight acceleration](docs/changes/SOLID-SIGHT-MESH-CACHE.md)** | Reduce repeated visibility geometry work on large monster maps while retaining destruction and activation. | Bounded, invalidated geometry cache and conservative triangle filtering; fresh visibility queries rather than cached sight answers. |
| **[HUD opacity](docs/changes/HUD-BACKGROUND-OPACITY.md) & [Speedrun layout](docs/changes/SPEEDRUN-LAYOUT.md)** | Transparent panel backgrounds, screen-edge X/Y positioning and row/column statistics. | Independent persisted opacity controls, widescreen bounds and text-preserving layout. |
| **Custom Speedrun HUD** | Choose map/run and player stats, arrange them in one row or separate left/right columns, or use only the Speedrun HUD for a cleaner screen. | Independent persistent stat toggles and column anchors; HUD-only mode suppresses standard overlays while preserving menus, automap and weapon wheel. |
| **[Selected Texture Immersion](docs/changes/SELECTED_TEXTURE_IMMERSION_REPORT.md)** | Opt-in motion and material effects bring selected flesh, faces, gates, eye switches, blood pipes, fire walls and Doom II skies to life using original WAD artwork. | Curated patch regions, shader masks and bounded animation; no replacement textures or gameplay changes. Windows/Vulkan gallery checks passed; Metal runtime coverage remains pending. |
| **Natural critical bloodspill** | Critical monsters can produce forceful, irregular blood jets, detached droplets, wall splashes and floor pools. | Bounded spray budget and blood-color profiles; effect remains optional and is separate from the critical-hit notification. |
| **Weapon Discovery** | Optional monster-drop modes replace dropped weapons with ammo, including a scarce-ammo option. | Uses native ammo/skill limits and persists the selected drop tier without changing the save-record layout. [Test report](docs/changes/WEAPON_DISCOVERY_TEST_REPORT.md) |
| **[Smart Persistent Lighting Cache 2.0 — M1](#smart-persistent-lighting-cache-20--m1)** | Reuse compatible static lighting on restart, save load, and map revisit. | Stage-specific keys, checksummed storage, cache-aware loading labels, and corruption recovery. |
| **[Universal Atmospheric Fog](#universal-atmospheric-fog--sprite-and-voxel-coverage)** | Sprites and voxels receive the appropriate outdoor haze at their position. | Shared world-position sector exposure with Vulkan and Metal shader variants. |
| **[Lightning Bolt Visibility 1.1](#lightning-bolt-visibility-11)** | Distant lightning has a readable two-pulse flash and fade. | A ten-tic sky-bolt envelope with independent environmental flash timing. |
| **[Environmental Corpse Reactions](#environmental-corpse-reactions-10)** | Bodies and fragments react to physical contact with acid and lava. | Contact-based progress, hazardous floor blood suppression, and native resurrection support. |
| **[Incendiary Barrels](#incendiary-barrels-10)** | Optional destructible burning barrels leak oil and ignite damaging pools. | Native blast damage, staged chain reactions, and save/load support for active hazards. |
| **[Large moving-platform protection](docs/changes/LARGE-PLATFORM-SHIELD.md)** | Keep breakable surfaces enabled while protecting large floor movers from excavation damage. | Rejects protected cuts before remeshing; small platforms and ordinary surfaces retain destruction. Manual lag confirmation remains pending in the linked report. |
| **[Monster visibility performance](docs/changes/AMBUSH-SIGHT-F4.md)** | Toggle ambush sight reuse from F4 without changing hearing or encounter activation. | Reuses only an unchanged false sight result within one action; no cross-tic LOS cache. |
| **[Conservative sprite filtering](docs/changes/SPRITE-FRUSTUM-FILTERING.md)** | Reduce off-camera monster rendering work when safely outside view. | Artwork and shadow bounds, with fallbacks for mirrors, wounds, voxels and uncertain poses. |
| **[Boom conveyor and switch fixes](docs/changes/ABANDON-PLATFORM-FIX.md)** | Restore affected voodoo conveyor releases and platform switch chains. | Preserves voodoo inertia and carries actors while their footprint still touches the conveyor. |
| **[Safer WAD sight checks](docs/changes/SF2012-SIGHT-AND-DESTRUCTION-FIX.md)** | Avoid the identified SF2012 sight crash; keep wall destruction available. | Handles missing sectors on incorrectly flagged two-sided walls; normal launcher retains solid geometry. |

**Optional population staging:** [Initial monster budgets](docs/changes/INITIAL-MONSTER-POPULATION-BUDGET.md) add count/percentage controls under F4 → Encounter Director → ED Population. Performance mode starts the supplied 4,317-monster WAD with 500 originals when requested, accepting visible absence/pop-in; Compatibility retains eager safety. No FPS benchmark is claimed.

**Start with F4.** Human critical parts, enhanced weak points, weapon
vulnerabilities, injuries, eye injuries, bleeding, feeding, frenzy, and death-save recovery
are optional and default off. Damaging elemental burns, colored notifications and the Speedrun HUD also default off. Immersive Sky, Monster Skin Damage, Decoration Immersion and Corpse Decomposition also default Off. New ambush sight reuse, sprite filtering and idle-search
reuse also default off in the normal build. Sprite combat additions are restricted to
single-player sprite rendering; multiplayer, demos, and voxel targets retain
their documented exclusions. Speedrun HUD 2.0 defaults to Classic colors;
Medical Supply Recovery, Flashlight Monster Awareness and Imp Fireball Avoidance
are opt-in. Corrosive Barrels 2.0 adds subordinate visual
preferences under the existing Corrosive Barrels feature. The recent updates
focus on sprite-based rendering; no new voxel work is included. Horror and
Physical Bodies are optional; enable Ragdoll deaths separately. See the
[recommended immersion setup](#recommended-immersion-setup).

> **Current behavior:** the survival-AI update supersedes the original flat
> bleeding and amber world-impact effects. It adds progressive bleeding,
> blood-colored headshot bursts, headshot sound, and optional feeding.
> The [latest survival update](docs/changes/SURVIVAL-ESCAPE-AND-FRENZY-RETALIATION.md)
> adds escape/search behavior, Arch-vile hunting and persistent hostile-pair
> retaliation. Earlier reports remain linked as implementation history.

### Recent gameplay, HUD, environment and AI updates

These notes describe the current source and its configuration. The validation
scope varies by feature; manual gameplay and performance measurements are not
implied by a successful build or a focused test.

#### Eye Injuries 2.0 — Visible Blindness & Recovery

Blinded Cacodemons and Pain Elementals show a darkened eyelid with localized
blood in their species' colors. Their eye glow returns as soon as they recover
sight. The effect follows the current stock frame and viewing angle and
composes with Monster Skin Damage, Wound Holes and acid contamination.

**F4 → Combat → Injuries → Visible Eye Injuries** defaults on
(`visible_eye_injuries 1`). It requires accelerated sprite rendering, Enhanced
Monster Weak Points and Eye Injuries. The renderer generates the eye variants
from the loaded WAD artwork and palette. Existing saves supply the blindness
state. [Implementation and verification](docs/changes/EYE-INJURIES.md).

#### Speedrun HUD 2.0

- **F4 → HUD → Run stats:** Classic retains the original appearance; Colored adds
  semantic information colors; Dynamic also responds to current player state.
  A color-intensity control blends semantic colors with neutral HUD text, and
  an optional one-pixel text shadow improves readability on bright backgrounds.
- Dynamic health colors distinguish overhealth, healthy, low and critical
  states (above 100 HP, 51–100, 26–50 and 25 or less). Armor distinguishes
  green/blue armor types and no armor; ammunition warnings reflect low supply
  and the equipped weapon's consumption. Map, deaths, killstreak, damage dealt
  and damage taken use distinct semantic colors. Completed secrets/items can
  be highlighted when their map totals are nonzero.
- Optional colored blue, yellow and red key indicators support **Compact**
  (one indicator per color) and **Cards / Skulls** (all six key types).
  Existing stat visibility, Row, Column, Split Columns, placement, background
  opacity and Speedrun HUD-only behavior are retained.
- **Compatibility:** Classic is the default color mode; key indicators and
  text shadow are initially off. The change is presentation-only and does not
  modify run counters, pickup rules or the existing Modern HUD.
- **Verification:** the earlier HUD report records isolated C++17 helper and
  option-registration tests. No new FPS or GPU-memory result is claimed.

#### Medical Supply Recovery 1.0

- Optional medical pickup overflow recovery preserves unused healing when a
  Medikit (25 HP) would bring the player to the normal 100 HP cap. The original
  pickup becomes a small recovery pack using existing Stimpack artwork, with
  the exact unused capacity instead of a fixed 10 HP reward.
- **Example:** a player at 90 HP uses 10 HP from a Medikit and leaves a 15 HP
  recovery pack. A later visit at 92 HP consumes 8 HP and leaves 7 HP. The final
  visit consumes the remaining 7 HP: **10 + 8 + 7 = 25 HP**, with no new healing
  created. At full or above-normal health, the original pickup remains untouched.
- Modes: **Classic** (default), **Medikits only**, and **Medikits and
  Stimpacks**; `graphics.cfg` key: `medikit_overflow` (0/1/2). Existing
  recovery packs retain their remaining value after switching to Classic.
- The implementation uses the normal health-giving path, existing pickup appearance and
  saved per-object metadata without growing the native actor record. Recovery
  packs preserve their value across saves, avoid duplicate item-count
  credit, and leave health bonuses, Soulspheres, Megaspheres and Berserk unchanged.
  New overflow generation is available in supported single-player sessions,
  excluding multiplayer/LAN, demos and Arcade.
- **Verification:** implementation and a passing test were reported by the
  developer. Detailed save/ABI and compatibility test logs were not supplied.
  The mechanic conserves healing but may make maps easier by reducing waste.
  Modified pickup definitions that cannot be transformed safely use classic
  behavior; older executables may not understand newer recovery-pack saves.
  [Implementation notes](docs/changes/MEDICAL-SUPPLY-RECOVERY.md).

#### Corrosive Barrels 2.0 — Shared Liquid Effects

- Acid barrel explosions now produce toxic yellow-green liquid splashes on
  exposed walls and floors. Wall splatter adheres to the existing surface and
  develops downward-running streaks; nearby exposed sprite monsters receive
  localized contamination that follows their movement and can shed droplets.
- The implementation reuses the existing blood spray trajectories,
  gravity, impact detection, wall-relative decals, procedural splatter masks,
  runoff, lighting and depth-tested rendering. Acid is a separate material and
  tint, not ordinary green Baron/Hell Knight blood. New code is limited to
  material-aware filtering, explosion notification and sprite coating where needed.
- Acid effects are independent of the Blood/Blood Spray switches. The
  F4 controls are **Acid Splatters and Wall Runoff** (`acid_splatters`) and
  **Monster Acid Contamination** (`acid_monster_coating`), enabled by default
  under the existing Corrosive Barrels master switch. They require accelerated
  rendering; monster coating also requires Acid Splatters and Wall Runoff.
  No voxel work is included.
- The new splashes and drips are cosmetic: existing acid damage, hazardous
  pools, radiation-suit protection and explosion rules remain authoritative.
  Visual drips do not create additional damaging acid pools. Temporary effect
  records have bounded lifetimes and budgets; these cosmetic records are
  not retained in saves. [Implementation notes](docs/changes/CORROSIVE-BARRELS-SHARED-LIQUID.md).
- **Verification:** the developer reported the feature implemented. A separate
  feature-specific test result and performance capture were not supplied in
  this update. No measured CPU/GPU savings or exhaustive visual coverage is claimed.

#### Incendiary Barrels 1.0

Enable **F4 → Combat → Barrels → Incendiary Barrels** to make stock burning
barrels destructible in single-player. Hits spray oil and widen an inert spill.
A rupture uses the stock five-frame explosion, spills burning oil, and can set
off nearby barrels in later chain waves. Active oil damages players and monsters
on contact and is saved with the game. The accelerated renderer draws the oil
film, flames, sparks, and smoke. The option defaults off and can be used alongside
Corrosive Barrels. [Design](docs/changes/INCENDIARY_BARRELS_DESIGN.md) ·
[Native and Vulkan verification](docs/changes/INCENDIARY_BARRELS_TEST_REPORT.md)

#### Rain and blood effects

- The existing cosmetic rain effect lightens exposed blood, spreads floor
  pools and gradually hides the rendered deposit. It uses the deposit's age
  while rain is currently active, including areas exposed through broken
  windows; sheltered deposits retain their normal appearance.
- This is an age-based rendering effect, not accumulated rainfall exposure:
  the washout appearance is not retained while rain stops. The source does not
  currently register separate Dynamic Rain Blood Washout, Blood Wall Runoff or
  washout-speed settings. Configure rain using **F4 → Atmosphere → Weather → Rain**.

#### Flashlight Monster Awareness 1.0

- Optional **Flashlight Alerts Monsters** lets an idle monster directly
  illuminated by the player's flashlight detect the player, including when
  the monster initially faces away. It plays its usual alert sound and enters
  normal pursuit; it is not forced directly into an attack animation.
- The implementation shares the existing Classic/Modern beam
  geometry, simulation-time player pose, range/cone and visibility checks.
  Solid walls and closed doors block direct illumination. Blind monsters and
  No Target retain their restrictions. Active AI, infighting, normal sight and
  sound detection continue unchanged.
- The new idle detection runs before the negative-search hibernation return
  so a stationary player can rotate the flashlight and wake an eligible
  monster. It does not require a per-frame global monster scan. Switching
  the flashlight off after detection does not cancel normal pursuit.
- Configuration: `flashlight_monster_awareness` (0/1, default 0),
  available in supported single-player accelerated gameplay. Multiplayer,
  demos, Arcade and detached camera modes are excluded. No
  new persistent actor fields, animations or shaders are required.
- **Verification:** a focused real-engine smoke test passed eight checks.
  Manual gameplay, the wider geometry matrix and performance measurements
  remain unverified. [Implementation notes](docs/changes/FLASHLIGHT-MONSTER-AWARENESS.md).

#### Imp Fireball Avoidance 1.0

- Optional **Imp Fireball Avoidance** can prevent an ordinary Imp from wasting a
  player-directed fireball attack when another living Imp reliably obstructs
  its predicted projectile path. Obstructed Imps continue their normal chase
  behavior and become eligible to attack when the path clears.
- The main check runs after the existing ranged-attack
  eligibility decision, before the missile animation. A second check before
  projectile creation handles an obstruction appearing during the animation.
  Melee attacks, ordinary infighting and supported enhanced AI remain available.
- Reuses the engine's projectile dimensions, collision semantics and bounded
  spatial blockmap queries. Vertical separation matters; nearby Imps outside
  the collision path must not prevent firing. Uncertain or unsupported
  trajectories fall back to original behavior. The policy does not alter
  existing fireball damage, speed, collision or same-species immunity.
- Configuration: `imp_fireball_avoidance` (0/1, default 0) under
  **F4 → Monsters → AI**. The policy is renderer-independent and
  restricted to supported single-player sessions. No additional actor save
  fields, new assets or new shaders are required.
- **Optimization intent:** fewer wasted fireballs may reduce projectile
  allocations, thinker and collision work, dynamic lighting and impact FX.
  Bounded queries run only for attack candidates, not every monster every
  frame. Net performance depends on query cost and actual encounter density.
- **Verification and limitations:** the Release game and focused smoke test
  built, and eight focused engine checks passed. The query does not establish
  whether world geometry stops the projectile before the predicted Imp
  collision; complex geometry, solid maps and the wider enhanced-AI matrix
  remain unverified. Manual gameplay and CPU/GPU benchmarks have not been
  performed, and no performance improvement is claimed. Searches are limited
  to 96 blockmap cells and 256 actors; incomplete queries allow the attack.
  [Implementation notes](docs/changes/IMP-FIREBALL-AVOIDANCE.md).

### Latest survival and encounter updates

- **Monster Feeding** remains optional and default off under F4 → Combat.
  Cacodemons and Pinkies at **30% health or less** search/approach food with a
  nominal **20% walking boost**, avoid nearby visible danger and recover through
  actual meals, up to **60% health**. Ten seconds without a successful meal
  returns them to combat with a ten-second failed-search cooldown.
- **Arch-vile survival:** at **50% health or less**, the same feeding option
  enables hunting eligible living biological monsters. A close siphon removes
  **24 HP**; healing is capped by actual damage and the recovery ceiling.
  Players and protected prey are excluded; normal resurrection retains priority.
- **Frenzy retaliation fix:** a Mancubus and its confirmed same-species hostile
  opponent can damage each other after frenzy expires. The Knight/Baron family
  is covered too. This preserves shooter and unrelated same-species immunity.
- **Encounter Director** has one F4 category with **ED Monsters**, **ED
  Reinforcements**, **ED Items**, **ED Population** and **ED Advanced** tabs,
  avoiding the earlier overlapping controls. All four modules default off;
  initial randomization preserves population, reinforcements add bounded actors,
  and population staging controls deferred originals separately.

[Survival behavior, tuning and focused checks](docs/changes/SURVIVAL-ESCAPE-AND-FRENZY-RETALIATION.md)
· [Director controls and progression guards](docs/changes/ENCOUNTER-DIRECTOR.md)

### Recent atmospheric rendering and cosmetic FX updates

The following incremental changes target the **2026.11.15.6 source baseline**.
They do not introduce new gameplay damage or modify classic WAD content.
Corpse Decomposition Milestones 1–2 have since received user-reported Windows
build and gameplay acceptance after their visual-activation and linker repairs.
The relevant user-run verification is distinguished below from source-only tests.

#### Universal Atmospheric Fog — Sprite and Voxel Coverage

Burning City, Blood Hell, Starport, Inferno, and Deimos now apply their outdoor
haze to monsters, decorations, pickups, and other world sprites using the same
position and sector rules as nearby geometry. Deimos keeps its whiteout look.
Vulkan comparisons confirmed the indoor and outdoor separation at a doorway
and checked a Blood Hell voxel. The held weapon and HUD matched the baseline.
The Metal shaders were regenerated. Metal runtime remains unverified, and
separate bloom and flat debris passes have their own fog coverage.
[Verification and remaining coverage](Log-Status.md#universal-atmospheric-fog--sprite-coverage-fix)

#### Lightning Bolt Visibility 1.1

With **Visible distant lightning bolts** enabled in F4 → Atmosphere, eligible
sky bolts use a ten-tic sequence with a bright first pulse, a visible interval,
a second pulse, and a short fade. The bolt has its own timing, so it remains
readable after the diffuse environmental flash dims. Storm activation, thunder,
and ground-strike timing follow the existing storm rules. Focused tests and
Windows/Vulkan captures checked the pulse and fade.
[Storm timing and visibility rules](duct-tape-doom-main/platform/storm_rules.h)

#### Immersive Sky Environment (Milestones A–C)

- **Milestone A:** introduced moving procedural cloud layers over the original WAD
  sky, using the existing sky pipeline and cloud texture.
- **Milestone B:** corrected wind motion that appeared to reverse direction and
  added distinct rocky, icy, industrial-city and infernal horizon treatments.
  The user reported the Milestone B test passed.
- **Milestone C:** added three apparent-depth/parallax skyline layers, sparse
  softly shaded cloud puffs, and episode/map-range automatic presets. Classic
  sky pixels remain the underlying artwork; cloud colors follow the loaded sky.
  This is a *pseudo-volumetric* effect, not raymarched volumetric clouds.
- **F4 → Atmosphere → Air → Immersive sky environment:** Off (default), Auto,
  Rocky mountains, Ice mountains, Inferno or Burning city. The F4 selector
  requires `Settings::immersiveSky`; a separate header repair was supplied for
  older headers missing this field. F4 choices use the normal `graphics.cfg`
  apply/persist mechanism.
- **Compatibility:** Auto assigns **DOOM E1 rocky / E2 icy / E3+ infernal**
  scenes and **DOOM II MAP01–11 rocky / MAP12–20 industrial city /
  MAP21–32 infernal** scenes. These are map-range conventions, **not** a
  fingerprint of replacement sky art. Custom WADs can select a
  manual profile or Off. These effects do not alter level geometry, actor
  collision or baked lighting; glass reflection probes can retain cached sky
  images until their normal refresh.

#### Soft Sun Shafts 2.0 and emissive HUD fix

- **Soft Sun Shafts 2.0:** updated `platform/shaders/mist.frag` to remove Bayer
  dithering and 32-step shaft-opacity quantization, smooth beam edges/endpoints,
  soften intersections against scene depth, and animate low-frequency density
  continuously. Existing mist, sun direction, sun bake and shaft mesh remain.
  **User runtime test: passed and approved.**
- **HUD bloom ghosting:** bright emissive surfaces could display dark, displaced
  copies of HUD text and graphics. The bloom suppression mask in
  `platform/shaders/present.frag` sampled the 320×600 HUD atlas as **two**
  vertical bands even though it contains **three**. Correcting the main and
  widescreen HUD coordinates removed the observed artifact while keeping
  emissive textures enabled. **User runtime test: bug solved.**
  The separate third Speedrun HUD atlas layer was not given a new bloom mask
  by this narrowly scoped repair.
- Both fixes change **shader source**: run `Recompile-Shaders.bat` before
  `Build-Hellforge.bat` so updated shader binaries are embedded in the engine.

#### Progressive Monster Skin Damage

- **F4 → Combat → Combat → Monster skin damage:** independent On/Off setting,
  **Off by default**. Supported living monsters accumulate body-only scratches,
  cuts and blood as their *current health* decreases; this has no gameplay,
  damage-over-time or AI effect.
- Health bands: **76–100%** original sprite, **51–75%** light scratches,
  **26–50%** moderate injuries, **1–25%** severe injuries.
- **Supported monsters (10):** Imp, Baron of Hell, Pinky, Spectre, Mancubus,
  Arch-vile, Cyberdemon, Cacodemon, Hell Knight and Zombieman.
  Anatomy-aware masks protect heads and important non-skin areas such as
  Cyberdemon machinery, Mancubus cannons and the Cacodemon eye and mouth.
- Uses cached, shared damaged **sprite-frame variants** rather than tracking
  individual hit positions or allocating unique textures per monster. Wound
  Holes continue to attach to the *original* artwork. Sprite replacements
  lacking recognized stock fingerprints, voxel rendering, charred variants
  and death artwork are not given these cosmetic injury textures.
- The six-monster expansion raised the **estimated combined CPU/GPU variant
  payload budget from 16 MiB to 32 MiB**. At its limit the renderer falls back
  to unmodified artwork. This is a cache policy, **not a benchmark**.
- Barons and Hell Knights use the existing green-blood treatment. Exact
  Zombieman stock-frame recognition can be generated from a user-owned DOOM
  IWAD; if no verified fingerprints are available, its replacement artwork
  is intentionally not modified.
- **User runtime tests:** the original Imp/Baron/Pinky/Spectre prototype and
  subsequent six-monster expansion were reported as passed and approved.
  These cosmetic health-stage variants do **not** persist onto death sprites.

#### Corpse & Decoration Immersion

- **F4 → Atmosphere → Decoration Immersion:** independent On/Off switch
  (**Off by default**), **Corpse insects** (Off / Flies / Flies and worms), and
  **Decay delay** (Short / Normal / Long). The more unusual ground creatures
  require **Flies and worms** mode.
- The reusable **Decoration Ambience Manager** supports ordinary monster
  corpses, crushed/gibbed or pre-placed remains, impaled bodies and hanging
  biological decorations. Delayed flies can surround suitable bodies;
  crawling worms are restricted to eligible ground-level remains.
- **Ordinary worms:** 1–3 per selected corpse, with a black head, flesh/pink-beige
  segmented body and slow wriggle. A visibility repair increased their chance
  and rendered size; the later visual pass uses tapered, overlapping particle
  blocks for a rounder-looking outline (not curved 3D mesh geometry).
- **Mancubus and Baron:** specialized **dark maroon-red earthworms**, 1–3 per
  selected corpse, with six segments and approximately **1.6–1.7×** the normal
  worm footprint after the latest refinement. Flies remain possible.
- **Revenant:** small bone-colored spiders with eight dark legs; **no flies**.
- **Arch-vile:** crawling fiery red/orange/yellow sea-slug-like scavengers with
  animated frills; **no flies**.
- **Cacodemon:** blue jelly-like scavengers with animated tassels and subtle
  pulsing; the newest palette is darker blue. **No flies.** Their glow is a
  color effect, **not** an added emissive light, bloom pass or shader.
- Other corpses retain ordinary fly/worm behavior; hanging and impaled bodies
  retain their general fly behavior. Special species are selected from known
  monster types; custom sprites using those existing types are **not**
  independently fingerprint-validated by the ambience system.
- Selection is deterministic and does not consume gameplay RNG. Generic flies
  have a **40%** selection chance; the worm visibility fix raised the ordinary
  worm chance to **45%**; special-species ground creatures have a **65%**
  selection chance. Appearance is intentionally not guaranteed per body.
  On Normal delay, flies start after roughly **15–30 seconds** and ground
  creatures after roughly **30–60 seconds**. Short/Long adjust the delays.
- **Performance gate:** automatically disabled when Doom's spawned
  kill-count monster population (`totalkills`) reaches **200 or more**.
  The manager caps tracking at **256 nearby bodies**, **12 active emitters**,
  **48 flies**, **24 ground creatures**, **192 ground-creature render blocks**,
  and **512 DOOM units** for rendering, with nearby scans about once per second.
  There are no new gameplay actors, shaders or save-format changes.
- Revived or removed corpses stop displaying scavengers. The delay begins
  when the corpse is first detected near the player. Ragdolls become eligible
  after they settle. Flies and ground scavengers use the final body position
  and the supporting sector's floor height and lighting.
- **User runtime tests:** corpse flies passed, the initial invisible-worm
  issue was corrected, and the black-headed worm visual update passed.
  Source-level mock tests cover the species-specific creatures. The later M2
  runtime pass also covers Stage 2 scavenger suppression; a separate exhaustive
  visual acceptance report for every individual creature variant is not recorded.

#### Environmental Corpse Reactions 1.0

In accelerated single-player, the death presentation settings let acid and
lava consume corpses, supported ragdolls, and physical gore fragments on contact.
Lava chars and collapses bodies, and acid dissolution uses the existing
**Acid Corpse Dissolution** option, which defaults off. Lava combustion,
hazardous floor blood suppression, and fragment reactions default on. Reaction
speed is configurable. The corpse remains available to native gameplay,
including Arch-vile resurrection, and cosmetic reaction progress is rebuilt
from current contact on save reload.

Native tests covered body and fragment contact, wall versus floor blood,
resurrection, and save/load. In-game particle appearance and large-map
performance remain unverified. [Settings, behavior, and verification](docs/changes/ENVIRONMENTAL-CORPSE-REACTIONS.md)

#### Corpse Decomposition & Organic Decay (Milestones 1–2)

- **F4 → Atmosphere → Decoration Immersion → Corpse decomposition:** an independent
  **Off/On** switch (**Off by default**), with **Decomposition begins after**
  **2, 5 (default), 8 or 10 gameplay minutes**. It works even when flies and
  worms are disabled. Selection is deterministic: normally about **40% of
  eligible corpses**, not necessarily every body on a small test map.
- **Eight supported stock monster types:** Zombieman, Shotgun Guy, Imp, Pinky,
  Mancubus, Cacodemon, Hell Knight and Baron of Hell. **Spectre and Heavy Weapon
  Dude/Chaingunner are intentionally excluded**. Normal terminal corpse states
  qualify, including supported ragdolls after they settle. Gibbed, acid and
  charred corpses retain their own presentation.
- **Map-wide aging:** the age tracker scans about once per second regardless of
  camera distance. With the **2-minute** delay, **Stage 1** starts at **2:00**
  (initial drying), **Stage 2** at **2:45** (partial decomposition), and
  **Stage 3** at **4:00** (final dry appearance). The other delay presets use
  the same **+45-second / +120-second** stage offsets. Pausing does not advance
  the gameplay-time clock. Corpse age is reconstructed after loading a save;
  exact pre-save decomposition progress is not persisted.
- **Palette-aware visual decay:** the renderer derives desaturated, mottled,
  dry-looking variants from the **loaded WAD sprite artwork and PLAYPAL**, with
  stronger visual contrast after the M1 visual-activation repair. Cacodemons
  use a blue-grey drying treatment; Mancubus, Baron and Hell Knight use muted
  warm dried-flesh colors. The original sprite dimensions and transparency are
  preserved; unsupported/replacement artwork and voxel presentations can fall
  back to unchanged visuals.
- **Shared decay budget:** conventional sprite variants and immutable copies of
  fresh ragdoll artwork share an **estimated 8 MiB budget**. The renderer recolors
  the existing ragdoll card in place, preserving its resting pose and silhouette.
  Turning decomposition off restores the original colors. Unsupported artwork
  or exhausted budget can leave a corpse visually unchanged.
- **Permanent ragdoll blood pools:** the pool starts growing once the ragdoll
  has finished moving. It uses the final torso position and its supporting
  floor, including when the body falls onto a different sector.
- **M2 — Stage 2 cleanup:** after a **selected decomposing corpse** enters
  Stage 2, **flies, normal and large red worms, and all special scavengers**
  cease rendering around it. Its **owned blood-pool record is removed** from
  the active list. A marker for that death prevents the pool from reappearing.
  Other bodies' pools, impact splats, wall blood and hanging
  decorations are unaffected. Shared blood textures remain cached, so this
  reduces active decals/particles rather than guaranteeing large GPU-memory
  savings. Corpses not selected for decomposition retain normal ambience.
- **Performance and lifecycle:** decomposition and its cleanup are disabled on
  maps with **200 or more spawned kill-count monsters**. Cosmetic state is
  cleaned up on resurrection, corpse removal and level transitions. No new
  gameplay actors, damage logic, save-format changes or shaders are introduced.
  Existing Monster Skin Damage and Wound Holes retain their own rendering paths.
- **M1 visual-activation fix:** relaxed an overly strict death-presenter gate
  for settled terminal corpses, increased visible drying contrast, and added
  optional **`-decaytest`** (select every eligible corpse) and **`-decaydebug`**
  diagnostic flags. Test flags are **not** normal gameplay settings and must
  reach the engine process to take effect.
- **M2 linker fix:** restored the missing `doom3d::monsterSkinBloodIndex`
  definition in `platform/scene3d/blood.cpp`, resolving reported
  `LNK2019` errors in the engine and smoke-test executables while retaining
  ordinary, Cacodemon-blue and Baron/Hell-Knight-green blood tinting.
- **Verification:** the first three-minute gameplay test exposed an absence
  of visible decomposition; the M1 activation repair was then **reported
  passed in-game**. The M2 expansion initially hit a linker error; after the
  targeted repair, the user reported **all Windows build and game tests passed**.
  Isolated GCC/Clang rule and rendering-path tests also passed. These results
  are not a benchmark or exhaustive certification of all replacement WADs.

### Engineering and verification

The linked feature reports document native gameplay tests, collision and
wound regressions, isolated settings profiles, and save/load checks. Examples include
**571 combat/rule/save checks and a focused real-core survival fixture** for
the latest survival update, **121 detectable human head views with seven safe fallbacks**, **23 large-WAD
maps passing historical loading checks**, and **three live death/Use cycles
restoring saved player state**. These results belong to the builds and test
conditions recorded in their linked reports; loading coverage does not imply
full campaign or performance certification.

The 26 November feature batch retains the individual verification notes above.
For 27 November, Kinetic Horror and Physical Bodies M1/M2 have developer-reported
runtime acceptance; the latest Deep Wounds / giblet batch has developer-reported
passing tests. These newer results are not independently reproduced here, and no
aggregate test count, frame-rate improvement or new packaged build is inferred.

**Explore:** [Combat matrix](docs/changes/ENHANCED-MONSTER-COMBAT.md#implemented-matrix)
· [Survival AI](docs/changes/MONSTER-SURVIVAL-AI.md)
· [Build and test instructions](BUILDING.md)
· [Documentation index](docs/README.md)
· [Bug and debugging log](Log-Status.md)

## Renderer, bundled content, and additional features

HELLFORGE builds on the DERP renderer and the Tuin-Doom-Enhanced fork. This
broader feature set includes inherited systems and integrations as well as
the extensions highlighted above.

### Renderer, lighting, and world presentation

- Optional three-layer Immersive Sky Environment with sky-tinted cloud motion,
  episode/map-based automatic presets and manual F4 scene profiles.
- Soft Sun Shafts 2.0 (continuous alpha and density transitions) and the
  emissive HUD bloom-mask fix for dark duplicated HUD silhouettes.
- Native SDL3 accelerated 3D renderer with dynamic lights, colored materials,
  reflections, liquid surfaces, sky lighting, shadows, AO, caustics, and
  destruction geometry.
- Multithreaded lighting bake, spatial ray traversal, selective relighting,
  reusable geometry, and bounded light/shadow caches.
- Persistent map-lighting cache keyed per stage by resolved map, material and
  lighting inputs, plus the bake algorithm and output layout. F4 controls caching; the launcher
  can clear it. GPU handles, pointers, actors, blood, particles, and live
  effects are never serialized.
- Safe level-transition teardown, worker barriers, first-frame diagnostics,
  crash logs, retained PDB symbols, and Windows minidumps.
- Large-texture and large-WAD compatibility improvements, including expanded
  texture column offsets and configurable native zone memory.
- Procedural acid and lava bubbles under Living Surfaces; see
  [living liquid surfaces](#living-liquid-surfaces--acid-and-lava-bubbles).
- Shared acid liquid effects for corrosive barrel explosions, with wall
  adhesion, runoff and cosmetic sprite contamination.
- Rain lightens and spreads exposed blood using the existing age-based
  rendering effect.

### Weapons and bundled content

- Native Fluid Weapons presentation for fists, pistol, shotgun, super shotgun,
  chaingun, rocket launcher, plasma rifle, BFG, and chainsaw.
- Recoil, reload and support hands, shell/bullet catches, muzzle flashes,
  plasma/BFG effects, weapon fidgets, smooth display interpolation, widescreen
  framing, fist alternation, weapon wheel, and optional fast-pistol behavior.
- Bundled Fluid Weapons, Fluid Weapons SFX, and FluidMonsters packages can be
  enabled independently through F4. Their original notices and credits remain.

### Monsters and combat

- **Kinetic Horror**: stronger cosmetic impact reactions, textured physical
  giblets, wall adhesion, soft deformation, wet deposits and impact sounds.
- **Physical Bodies**: opt-in sprite ragdolls, environmental contact assistance,
  stable generated sprites and bounded reconstruction scheduling.
- **Deep Wounds & Visceral Giblets**: latest developer-tested visual update;
  see [development status](#deep-wounds-and-visceral-giblets) for verification limits.
- Optional health-driven **Monster Skin Damage**, using shared cached sprite
  variants for ten stock monster types and independent of gameplay bleeding.
- Optional **Corpse & Decoration Immersion**: delayed flies, pale worms,
  large dark-red worms, Revenant bone spiders, Arch-vile fire slugs and
  Cacodemon blue jelly creatures, bounded by a small-map performance gate.
- Optional **Corpse Decomposition**: age-driven dried sprite variants for eight
  monster types, with Stage 2 scavenger and owned blood-pool cleanup.
- Optional sprite-pixel critical parts with transparent-pixel misses and
  human headshots.
- Extended monster weak points and weapon vulnerabilities for humanoids,
  floating monsters, Lost Souls, Pinky/Spectre faces, and mechanical brain/leg
  regions, with explicit single-player sprite-only exclusions.
- Headshot impact feedback, metal-leg sparks, progressive bleeding, temporary
  injuries, localized effects, and optional monster frenzy/infighting rules.
- Native FluidMonsters movement for supported ground monsters, overlap recovery,
  door/corner chase fixes, floor/ceiling fitting, and typed notarget controls.
- Optional Flashlight Monster Awareness adds direct illumination to idle
  detection while preserving normal alert and chase behavior.
- Optional Imp Fireball Avoidance reduces wasted same-species-obstructed
  projectile attempts while retaining melee and infighting.

### HUD, input, and game flow

- Classic, compact modern, and tactical HUD layouts with scale and placement
  controls, widescreen anchoring, kills/secrets/items counters, notices, Toasty
  presentation, photo mode, camera director, arcade scoring, and LAN/co-op.
- Independent HUD/Speedrun background opacity down to **0%**, plus Speedrun
  **Column**, **Row**, or **Split Columns** layout and **X/Y offsets** that
  reach the widescreen edges. Split Columns place map/run information on the
  left and player information on the right, with independent player-column
  positioning.
- F4 → HUD → Speedrun and Run stats let players independently show the map name, secrets, items,
  health, armor, ammo, attempts, deaths, killstreak, time and damage metrics.
  **Speedrun HUD only** hides the standard HUD, crosshair and gameplay notices;
  menus, automap and weapon wheel remain usable. Disabled widgets skip their
  formatting and drawing work. This reuses the existing HUD atlas and does not
  remove its GPU allocation, so it is a clean-screen option rather than a
  claimed memory-saving mode.
- Speedrun HUD 2.0 adds Classic/Colored/Dynamic information colors, adjustable
  intensity, optional text shadow, and Compact or Cards / Skulls key indicators.
- Optional Medical Supply Recovery conserves unused Medikit/Stimpack healing
  through variable-value recovery packs left on the ground for later use.
- Local single-player movement options including autorun, jumping, mouse look,
  and limited air steering. Demo, multiplayer, voxel, and unsupported scripted
  content retain their documented restrictions.
- F4 settings persist only when applied through the normal close/apply flow;
  F10 discards unapplied drafts. Settings export/import creates recovery backups.

## Compatibility and limitations

The supported map target is classic DOOM/Boom-style WAD content. This fork does
not implement a general GZDoom ZScript/DECORATE/UDMF runtime, arbitrary custom
shaders, or full voxel-mod scripting. Native voxel rendering does not provide
sprite-specific anatomy or deformation. Multiplayer, demos, and several
renderer-only features intentionally bypass single-player sprite combat rules.

Performance depends on map complexity, hardware, resolution, lighting settings,
and accumulated destruction. The lighting cache skips compatible static bake
work. Level reconstruction, geometry preparation, GPU uploads, and oversized
stages still run where needed.
Decoration ambience and corpse decomposition are automatically disabled at
200 or more spawned kill-count monsters, but this is not a measured FPS
or memory-saving guarantee. Sprite skin injuries are deliberately limited to
recognized stock artwork; unusual PWAD replacements and non-sprite presentations
may remain visually unmodified. Decomposition's cosmetic corpse age is not
preserved across saved-game reloads.

The recent feature work is sprite-focused and does not require voxel models.
Existing upstream renderer capabilities and historical compatibility notes are
retained. The new medical, flashlight and Imp AI options can change gameplay
balance and timing when enabled; these settings default to classic/off
behavior and restrict gameplay-changing features in unsupported sessions.
Shared acid, rain and living-liquid effects are cosmetic. Kinetic Horror and
Physical Bodies are also presentation systems; they do not replace gameplay damage
or actor movement. Advanced Gore uses its existing below-200-monster gate. Sprite
profiles and ragdoll artwork may fall back on unsupported frames or voxel models.
Permanent pools and scavengers follow the resting visual ragdoll. Supported
terminal ragdoll corpses can decompose while retaining their original pose.
No new performance figures or full cross-WAD certification are asserted.

See the [large-WAD compatibility report](docs/changes/BIG-WAD-COMPATIBILITY.md)
and [reload/cache report](docs/changes/RELOAD-LIGHTING-CACHE.md) for measured
limits and verification evidence.

## Building

Install Visual Studio 2022 C++ Build Tools, CMake, and the Windows SDK, then
double-click **Build-Hellforge.bat** in the repository root. Corpse Decomposition
M1/M2 and its blood-linker repair require a native rebuild **but no shader
recompilation**. For shader changes (including Immersive Sky, sun shafts and
HUD bloom masking), run
**Recompile-Shaders.bat** first; required tools are listed in
[BUILDING.md](BUILDING.md). Command-line builds also work:

```powershell
powershell -ExecutionPolicy Bypass -File launcher/build.ps1
```

The default output is `dist/DOOM-Hellforge.exe`. Native tests and optional GPU
checks are documented in [BUILDING.md](BUILDING.md). No commercial IWAD is
bundled with the source or launcher.

**Forced rebuild:** double-click [Build-Hellforge-V2.bat](Build-Hellforge-V2.bat)
to clean/rebuild native code and dependencies, then package the launcher, even
when edited/copied source files have older timestamps than existing binaries.
Use `Build-Hellforge-V2.bat -RecompileShaders` to regenerate shaders too.
Source, settings and the CMake cache are preserved; this takes longer than the
normal incremental build and does not run game tests.

**Stale-binary troubleshooting:** Some engine files were updated without a
correspondingly newer executable being launched. After applying a source patch,
ensure the changed files are rebuilt, embedded shaders are regenerated when
needed, and the launcher runs the newly built executable. A newer source
file alone does not prove its changes are present in a running binary.

## Developer resources

| Area | Entry point |
| :--- | :--- |
| Native engine / renderer | [`duct-tape-doom-main/`](duct-tape-doom-main) |
| F4 settings and feature gates | [`settings_panel.cpp`](duct-tape-doom-main/platform/settings_panel.cpp) |
| Sprite wounds and cosmetic event resolution | [`wounds.cpp`](duct-tape-doom-main/platform/scene3d/wounds.cpp) |
| Physical fragments and rendering | [`gore_giblets.h`](duct-tape-doom-main/platform/gore_giblets.h) · [`giblet_scene.cpp`](duct-tape-doom-main/platform/scene3d/giblet_scene.cpp) |
| Ragdoll simulation and sprite reconstruction | [`ragdoll.cpp`](duct-tape-doom-main/platform/scene3d/ragdoll.cpp) · [`ragdoll_body.cpp`](duct-tape-doom-main/platform/scene3d/ragdoll_body.cpp) |
| Original giblet assets and packer | [`assets/gibs/`](duct-tape-doom-main/assets/gibs) · [`pack_giblets.py`](tools/pack_giblets.py) |
| Launcher | [`launcher/`](launcher) |
| Tests and technical reports | [Build and tests](BUILDING.md) · [Physical Bodies validation](docs/physical-bodies-validation.md) · [Documentation](docs/README.md) · [Change reports](docs/changes) |

The Physical Bodies validation guide includes the native F4 and wound test targets,
seven-map arena generation and a manual validation matrix covering wall contacts,
mirroring, cached cards, lighting, moving supports and lifecycle behavior. Keep
Legacy/Enhanced comparisons as regression baselines when changing the cosmetic
pipeline. If embedded giblet artwork is changed, regenerate it with the existing
`python tools/pack_giblets.py` workflow and validate with `--check`; do not edit
the generated atlas by hand. Shader-source changes require shader regeneration
before packaging. See [Building](#building) for the supported Windows workflow.

Historical reports: [DOOM2 startup crash fix](docs/changes/DOOM2-STARTUP-LIGHT-CACHE-CRASH.md) · [Large platform protection](docs/changes/LARGE-PLATFORM-SHIELD.md) · [E1M8 exit and screen-edge fixes](docs/changes/E1M8-AND-SPEEDRUN-FIXES.md) · [Speedrun position and layout](docs/changes/SPEEDRUN-LAYOUT.md) · [HUD background opacity](docs/changes/HUD-BACKGROUND-OPACITY.md) · [Elemental Damage 2.0 and Speedrun HUD](docs/changes/ELEMENTAL-DAMAGE-2.md) · [Integration notes](docs/changes/MAIN-INTEGRATION-2026-11-14.md)

## Source and credits

HELLFORGE is based on Tuin-Doom-Enhanced, which incorporates Duct-Tape DOOM /
DERP engine and renderer contributions. Attribution for those contributions:

- DERP, the DOOM Engine Remaster Project — https://github.com/robongrui/derp
- Duct-Tape DOOM — https://github.com/robongrui/duct-tape-doom

- [Tuin-Doom-Enhanced source fork](https://github.com/tuin-boop/Tuin-Doom-Enhanced)
- DOOM source by id Software; original notices are retained.
- MR's Fluid Weapons, Fluid Weapons SFX, FluidMonsters, SDL3, Nuked OPL3,
  TinySoundFont, Toasty media, and other dependencies retain their own credits
  and licenses.

See [LICENSE](LICENSE), [licenses/NOTICE.txt](licenses/NOTICE.txt), and
[licenses/THIRD_PARTY.md](licenses/THIRD_PARTY.md).

The DERP notice is preserved in
[duct-tape-doom-main/NOTICE.txt](duct-tape-doom-main/NOTICE.txt); the earlier
Duct-Tape DOOM notice is preserved in `licenses/NOTICE.txt`. These attribution
terms cover their respective upstream contributions. Other components retain
their own licenses, and HELLFORGE retains its own project name.

Launcher builds copy the license texts and both attribution notices into a
`licenses` directory beside the executable. Distribute that directory with
the executable, and provide the corresponding source for the exact build
under GPLv3. Distributing the executable alone omits these accompanying files.

## Project status

- **Severe critical bloodspill.** Enable it in F4 → Combat to see short,
  pressurized jets when damage drops one of seven supported monster types
  into critical health.
  Streams break into falling fragments as pressure drops. Jets leave colored
  splashes on solid walls, and fallout collects in small pools on dry floors.
  The effect needs Blood and Blood Spray and is limited to maps with fewer
  than 200 kill-count monsters. The monster must cross from above 25% of
  spawn health to 25% or less and survive the hit or shotgun discharge.
  A red **SEVERE CRITICAL** notice uses Colored Combat
  Notifications independently of the spray toggle.

Recent source updates for this build family:

- **28 November 2026.** See [what's new](#whats-new-in-hellforge) for M1
  lighting cache, world-sprite fog, lightning visibility, and hazardous
  liquid contact reactions. The reports distinguish Release builds, native
  regressions, Vulkan captures, and pending checks.

- **Recent combat immersion batch (27 November 2026):** Kinetic Horror and
  Physical Bodies M1/M2 received developer-reported gameplay acceptance; the
  latest Deep Wounds / giblet batch also received a passing-test report. See
  [combat immersion](#kinetic-horror-and-physical-bodies) for configuration,
  architecture and the scope of current verification. No new release ID is asserted.

- **Recent development batch (26 November 2026):** see the
  [feature details and verification notes](#recent-gameplay-hud-environment-and-ai-updates)
  for the HUD, medical recovery, acid, flashlight and Imp changes, as well as
  the existing rain effect. The documented build identifier is unchanged.

- **Sunder 2308 MAP01 shutdown.** Closing the game during a background lighting
  update now cancels the remaining work. The in-game probe went from at least
  88 seconds before timeout to a clean exit in 0.17 seconds. [HF-001](Log-Status.md#hf-001--sunder-2308-waits-on-exit)
- **Pinky feeding and retreat.** Pinky faces the direction of its actual step
  and turns toward food before biting. The Windows engine and launcher were
  rebuilt. The in-game visual check is still open. [HF-003](Log-Status.md#hf-003--pinky-faces-the-player-while-moving-away)
- **DOOM II atmosphere profiles.** Episode Atmosphere includes Starport
  duststorms (SKY1), Burning City smoke and ash (SKY2), and Blood Hell rain and
  fog (SKY3). Burning City smoke was reduced to sparse, thin drifting haze
  with soft, masked edges instead of dense plumes filling the view. The F4 menu
  offers manual profiles and a mode that follows the active sky, including on
  custom WADs when selected. [Atmosphere settings](duct-tape-doom-main/platform/settings_panel.cpp)
- **Launcher map/add-on separation.** The launcher now has separate tabs for
  the persistent add-on loadout and selectable map WADs. Adding WADs classifies
  them by map markers, legacy mixed lists migrate on load, and switching maps
  does not replace the add-on selection. The launcher preview selects a Baron
  for original Doom and Final Doom IWADs and an Arch-vile for Doom II; this only
  changes preview artwork. [Launcher source](launcher/Launcher.cs)
- **F4 settings reorganization.** The former flat settings presentation is now
  grouped into 16 categories with relevant sub-tabs and a Find mode. All 312
  unique settings retain their existing keys, ranges, dependencies and saved
  values. Search can match setting names, keys and help text; editing remains
  part of the normal apply/discard flow. The native headless navigation check
  passed 1,910 checks against both Doom and Doom II WADs.
- **Expanded Speedrun HUD.** Secrets, items, health, armor and equipped-weapon
  ammo can each be enabled independently. Split Columns puts map/run statistics
  on the left and player information on the right with separately adjustable
  placement; Row and Column remain available. Speedrun HUD only hides other
  gameplay overlays while keeping menus, automap and the weapon wheel available.
  Settings persist through the normal F4 flow, and native settings/render
  checks cover the layouts and stat toggles.
- **Selected Texture Immersion expansion.** The opt-in shader effect now covers
  selected body and flesh panels, gates, eye switches, bleeding pipes and
  faces, animated fire walls, and Doom II skies. It uses original WAD artwork;
  the approved production pass removed the cavity enhancement that caused
  unwanted dark spots. The Doom II Vulkan gallery rendered 23 selected scenes;
  selected regions changed while sampled unselected artwork stayed unchanged.
  Shader generation and Windows x64 packaging passed. Metal runtime checks
  remain pending. [Design](docs/changes/SELECTED_TEXTURE_IMMERSION_DESIGN.md) ·
  [Verification](docs/changes/SELECTED_TEXTURE_IMMERSION_TESTS.md)
- **Critical bloodspill refinement.** Critical-hit blood was reshaped from a
  uniform burst into irregular pressure streams that fragment and fade, with
  wall splashes and small floor pools. The severe effect remains separately
  gated from the low-health/critical combat message. Monster color mappings and
  effect budgets are documented in the source and combat settings.
- **Weapon Discovery.** F4 can keep Classic monster weapon drops, replace them
  with skill-scaled ammo, or use Scarce Ammo quantities. Focused native checks
  covered quantities, ammo caps, save/load and mode fallbacks. The CTest run
  passed 38/39 in the existing build directory; the one portability test passed
  when rerun from a fresh writable directory. [Report](docs/changes/WEAPON_DISCOVERY_TEST_REPORT.md)
- **Revenant Soul Release.** Added a bounded visual soul-release effect to
  Revenant deaths, with focused native smoke coverage. This is cosmetic and
  does not change weapon damage or monster behavior.
- **Arch-vile fire spawn coordinate.** Corrected `A_VileTarget()` to pass the
  target's Y coordinate as the fire actor's Y position. This is a one-line
  placement fix; Arch-vile AI, damage, sound, timing and rendering are unchanged.

- **Oku map visibility work.** Conservative camera-frustum checks reject
  subsectors and wall geometry before vertex generation, while uncertain cases
  keep the original renderer path. In the 64-view diagnostic comparison, world
  preparation measured 8.58 ms without culling and 4.73 ms with culling;
  generated world triangles fell from 122,036 to 35,068 per view. This is a
  geometry-stage result, not a claim of whole-game FPS improvement. Full image
  parity and moving-door edge-case coverage remain incomplete, so the README
  does not characterize the feature as fully validated.
- **Oku moving-sector relight optimization.** A reusable spatial index narrows
  the baked wall strips checked during relighting, and each job deduplicates
  identical marked regions. In a same-build, uninstrumented 12-second moving,
  no-monster traversal, frame rate changed from 28.07 to 33.22 FPS; median frame
  time changed from 28.147 to 26.477 ms, p95 from 57.625 to 52.393 ms, p99 from
  252.692 to 70.801 ms, and maximum from 337.400 to 77.471 ms. The player
  traveled 4,232 and 4,445 map units respectively, so path lengths were close
  but not identical. Detailed profiling measured relight setup at 201.435 to
  12.193 ms and marking at 185.413 to 0.483 ms; background worker time and GPU
  upload staging did not improve. The optimization preserves exact masks in
  the Oku verification run. [Relight implementation](duct-tape-doom-main/platform/scene3d/relight.cpp)

### Known checks and debugging history

The [bug and debugging log](Log-Status.md) records reproductions, causes,
fixes, verification levels and follow-up checks. Current open validation items:

- Inferno lightning now recognizes the short `TRE1` tree actor correctly, but
  the E3M1 in-game visual check is still open. See [HF-004](Log-Status.md#hf-004--inferno-lightning-targets-only-the-large-tree).
- The Pinky facing correction builds successfully; its retreat, obstacle
  detour and feeding animation still need an in-game visual check. See
  [HF-003](Log-Status.md#hf-003--pinky-faces-the-player-while-moving-away).
- Burning City smoke uses thin, soft-edged haze; inspect the result in-game
  against a bright sky. See [HF-010](Log-Status.md#hf-010--burning-city-smoke-has-rectangular-edges).
- Large-map visibility culling still needs full image-parity and moving-door
  coverage. Its measured geometry-stage result is not whole-game FPS evidence.

Resolved build issues include relocation-stale CMake caches, launcher settings
skipped by fast builds, ambiguous Windows Python discovery, and tracked files
inside `test-references/`; see HF-005 through HF-008 in the log. The Sunder
2308 slow-exit and SDL storm-audio shutdown crash fixes are recorded as verified
there as well.

**README baseline: build 2026.11.15.6.** This identifier is the documented
source baseline; earlier documented improvements were rebuilt and packaged
afterward, but no new release identifier is asserted. The six 26 November feature
updates are described from developer-reported implementation and acceptance;
packaging and exact build identifiers for that batch were not supplied. The sky, soft sun-shaft, HUD bloom,
monster skin-damage, decoration-ambience and Corpse Decomposition M1/M2 updates
above were developed as incremental source patches for this build family.
The user reported successful Windows build and gameplay tests for decomposition
M1 after the visual-activation repair, and for M2 after its missing-symbol linker
repair. These reports establish the acceptance of the tested changes, **not**
comprehensive PWAD compatibility, per-effect GPU-memory savings or FPS results.
No new release/build identifier is asserted solely because a patch package exists.
The 27 November combat immersion update likewise records source development and
reported acceptance rather than a separately numbered binary release.

The survival and retaliation update has focused rules and real engine checks.
See the dated report for its original build coverage and remaining in-game
checks for visual steering, crowded-map performance, and animation.
[Survival and retaliation report](docs/changes/SURVIVAL-ESCAPE-AND-FRENZY-RETALIATION.md)

Build 2026.11.14.1 was an earlier integration milestone. New performance
controls remain optional. Idle-search reuse is restricted to classic geometry;
normal solid-map launches retain destruction. Profiling and benchmark tools
are opt-in build configurations and are compiled out of the normal launcher
engine. Historical reports retain their original builds, dates and coverage
limitations; see the current release notes for the integration checks and
outstanding manual coverage.

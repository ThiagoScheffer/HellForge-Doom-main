/* The F4 graphics options; see settings_panel.h. */
extern "C" {
#include "doomtype.h"
#include "../linuxdoom-1.10/p_weakpoints.h"
#include "m_argv.h"
extern int netgame,demoplayback,demorecording;
}
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include "settings_panel.h"
#include "game_profile.h"

namespace doom3d {
bool validPeekBinding(int code) {
    if(code<=SDL_SCANCODE_UNKNOWN||code>=SDL_SCANCODE_COUNT)return false;
    switch(code){
    case SDL_SCANCODE_ESCAPE:case SDL_SCANCODE_Q:case SDL_SCANCODE_F:
    case SDL_SCANCODE_PAUSE:case SDL_SCANCODE_F1:case SDL_SCANCODE_F2:
    case SDL_SCANCODE_F3:case SDL_SCANCODE_F4:case SDL_SCANCODE_F5:
    case SDL_SCANCODE_F6:case SDL_SCANCODE_F7:case SDL_SCANCODE_F8:
    case SDL_SCANCODE_F9:case SDL_SCANCODE_F10:case SDL_SCANCODE_F11:
    case SDL_SCANCODE_F12:return false;
    default:return *SDL_GetScancodeName((SDL_Scancode)code)!=0;
    }
}
namespace {
const char *solidMapText(const char* mapped,const char* legacy){
    return M_CheckParm((char*)"-solidmaps")?mapped:legacy;
}
const char *effectCost(const char *key) {
    struct Cost {const char *key,*note;};
    static const Cost costs[]={
        {"baked_lights","CPU / LOAD COST: builds static lighting and traces light for items and monsters. Moving sectors can start local re-bakes."},
        {"window_light","CPU / LOAD COST: maps with many openings can create many window emitters, increasing sprite lookup and re-lighting work."},
        {"moving_relight","CPU COST: local updates run in the background; large affected regions can still cause frame-time spikes."},
        {"grid_sprite_light","LOAD / MEMORY COST: precomputes interpolated samples to reduce per-frame sprite-light tracing."},
        {"map_light_cache","DISK CACHE: speeds matching map loads. Per-frame light lookups and moving-sector updates still run."},
        {"chain_collapses","BOUNDED GAMEPLAY COST: one local support check every four tics; at most two secondary generations."},
        {"environment_budget","BOUNDED COST: 64 / 128 / 256 extra dust particles."},
        {"electrical_damage","BOUNDED COST: 16 faults, at most two flashes per tic."},
        {"shore_ripples","HIGH COST: many shoreline particles. Off by default."},
        {"immersive_sky","GPU COST: adds cloud texture fetches and procedural horizon shading for sky pixels."},
        {"decoration_ambience","BOUNDED COST: only maps with fewer than 200 spawned monsters; 12 bodies, 48 flies and 24 worms maximum."},
        {"corpse_decomposition","BOUNDED COST: under 200 map monsters, up to 199 corpse ages and 8 MiB estimated cached sprite variants."},
        {"reflections","HIGH GPU COST: renders the reflected scene again."},
        {"glass_reflections","HIGH GPU COST: another reflected scene when visible."},
        {"msaa","GPU COST rises with sample count and resolution."},
        {"scale","GPU COST rises with resolution. 75%/50% are faster."},
        {"flashlight_shadows","VARIABLE COST: rises with things inside the beam."},
        {"sun_shadows","LOAD-TIME COST: sunlight is baked, not rebuilt every frame."},
        {"decoration_shadows","LOAD-TIME COST: changing this re-bakes lighting."},
        {"wounds","VARIABLE COST: rises with wounded monsters in view."},
        {"monster_skin_damage","ONE-TIME GPU TEXTURE COST: caches recognized stock sprite damage tiers (bounded to 16 MiB)."},
        {"death_falls","VARIABLE COST: rises with simultaneous visible deaths."},
        {"varied_deaths","VARIABLE COST: rises during large fights."},
        {"hit_flinch","LOW TYPICAL COST: rises with monsters reacting at once."},
        {"death_motion","LOW TYPICAL COST: rises with simultaneous deaths."},
        {"embers","BOUNDED COST: nearby particles only, capped at 384."},
        {"glow_edges","GPU COST: additional wall shading near glowing liquids."},
        {"near_miss_sounds","LOW TYPICAL COST: checks enemy bullet paths."},
    };
    for(const auto &cost:costs)if(!strcmp(key,cost.key))return cost.note;
    return nullptr;
}
Option check(const char *key,const char *title,int *flag,const char *tip,OptionCondition enabled=nullptr) {
    Option option{};option.kind=OptionKind::Check;option.key=key;option.title=title;option.cost=effectCost(key);option.tip=tip;option.flag=flag;option.enabled=enabled;return option;
}
Option choice(const char *key,const char *title,int *flag,std::vector<const char*> items,std::vector<int> values,const char *tip,
              OptionCondition enabled=nullptr) {
    Option option{};option.kind=OptionKind::Choice;option.key=key;option.title=title;option.cost=effectCost(key);option.tip=tip;
    option.flag=flag;option.items=items;option.values=values;option.enabled=enabled;return option;
}
Option slider(const char *key,const char *title,float *number,double low,double high,double step,const char *tip,
              OptionCondition enabled=nullptr,std::vector<const char*> labels={}) {
    Option option{};option.kind=OptionKind::Slider;option.key=key;option.title=title;option.cost=effectCost(key);option.tip=tip;
    option.number=number;option.low=low;option.high=high;option.step=step;option.enabled=enabled;option.items=labels;
    return option;
}
bool hasGlass(const Settings&) {return Game_HasEffect(GAME_EFFECT_WINDOW_GLASS)!=0;}
bool hasFlashlight(const Settings&) {return Game_HasEffect(GAME_EFFECT_FLASHLIGHT)!=0;}
bool hasFalloff(const Settings&) {return Game_HasEffect(GAME_EFFECT_NATURAL_FALLOFF)!=0;}
bool hasFreeAim(const Settings &s) {return s.look&&Game_HasEffect(GAME_EFFECT_FREE_AIM)!=0;}
bool hasAuthored(const Settings&) {return Game_HasEffect(GAME_EFFECT_AUTHORED_EPISODES)!=0;}
}

void fitGame(Settings &s) {
    s.deathRagdoll=s.deathGuided=s.deathBlast=0; // Keep the existing, individually toggled death behavior.
    if(!Game_HasEffect(GAME_EFFECT_WINDOW_GLASS))s.windowGlass=0;
    if(!Game_HasEffect(GAME_EFFECT_SHOOTABLE_LIGHTS))s.shootableLights=0;
    if(!Game_HasEffect(GAME_EFFECT_NATURAL_FALLOFF))s.naturalFalloff=0;
}

std::vector<OptionTab> optionTabs(Settings &s) {
    std::vector<OptionTab> tabs={
        {"Display",{
            {"Renderer",{
                check("fullscreen","Fullscreen (F11)",&s.fullscreen,"Use the entire display. F11 also toggles fullscreen during play."),
                choice("accelerated","Renderer",&s.accelerated,{"Accelerated 3D","Classic software"},{1,0},
                    "The 3D renderer draws the world at your window's pixel resolution; classic is the original 320x200 view."),
                choice("scale","Resolution",&s.scale,{"Native (100%)","75%","50%"},{100,75,50},
                    "Lower resolutions lighten the GPU workload."),
                slider("msaa","Anti-aliasing (MSAA)",&s.msaa,0,std::max(1.0,std::log2((double)gpuMaxSamples())),1,
                    "Smooths the stair-steps along the edges of walls, floors and ledges. Texels, sprites and bars stay crisp. "
                    "Each step costs more GPU time.",
                    [](const Settings &){return gpuMaxSamples()>1;},{"Off","2x","4x","8x"}),
                slider("fov","Field of view",&s.fov,60,120,5,"Measured against the classic 4:3 view."),
                check("widescreen","Widescreen",&s.widescreen,"Show more world at the sides rather than stretching it.")}},
            {"View",{
                choice("hud_style","HUD style",&s.hudStyle,{"Classic","Modern compact","Modern tactical"},{0,1,2},"Modern layouts show health, armor, ammo and keys over the full view.",[](const Settings &d){return d.accelerated!=0;}),
                choice("hud_face","Modern HUD face",&s.hudFace,{"Hidden","Left","Center"},{0,1,2},"Doomguy's face keeps its live damage, firing and pickup expressions.",[](const Settings &d){return d.accelerated&&d.hudStyle!=0;}),
                slider("hud_background_opacity","HUD background opacity",&s.hudBackgroundOpacity,0,100,5,
                    "Opacity of modern HUD panel and corner-stat backgrounds. 0% removes the dark boxes while keeping text and indicators visible.",[](const Settings &d){return d.accelerated!=0;}),
                slider("hud_scale","Modern HUD size",&s.hudScale,50,100,5,"Scale the modern HUD from 50 to 100 percent.",[](const Settings &d){return d.accelerated&&d.hudStyle!=0;}),
                choice("hud_layout","Modern HUD layout",&s.hudLayout,{"Bottom corners","Bottom centered","Top corners"},{0,1,2},"Choose where the health, armor, ammo and face panels sit.",[](const Settings &d){return d.accelerated&&d.hudStyle!=0;}),
                check("look","Mouse look",&s.look,"Mouse look tilts the camera; combat keeps DOOM's automatic aiming."),
                check("invert_look","Invert mouse look",&s.invertLook,"Moving the mouse forward tilts the view down, like a flight stick.",
                    [](const Settings &d){return d.look!=0;}),
                check("classic_aim","Classic aim",&s.classicAim,
                    "Bullets aim themselves at a monster in front of you, as in DOOM, and hit it in the middle; with none in front "
                    "they follow the view. Off: they go exactly where you look, so you can miss, and where you hit is where the "
                    "wound and the fall come from. Rockets, plasma and the BFG aim as in DOOM either way.",hasFreeAim),
                check("crosshair","Crosshair",&s.crosshair,"A small crosshair in the middle of the view."),
                check("key_pickup_hands","Key pickup hands",&s.keyPickupHands,"Quickly lower your weapon and show the collected keycard or skull. Firing cancels the animation.",[](const Settings &d){return d.accelerated!=0;}),
                choice("helmet_face","Doomguy portrait",&s.helmetFace,{"Original","Silver helmet","Tan helmet","Silver helmet 2.0","Tan helmet 2.0"},{0,3,4,1,2},"Optional Boxart Doomguy helmets. Silver/Tan use the original artwork; 2.0 has the revised visor and faceplate. Works with classic and modern HUDs; keeps all 42 face states, including damage, turns, god mode and death."),
                check("combat_notices","Combat achievements",&s.combatNotices,"Show close calls, multi-kills, barrel chains, fist kills and perfect clears. Boss, pickup and secret messages stay enabled."),
                check("fps","Show FPS and frame time",&s.fps,"Fullscreen counter: FPS, average frame time and the slowest frame in the last second. FPS also appears in the window title.")}},
            {"Saving",{
                check("quick_confirm","Confirm quicksave and quickload",&s.quickConfirm,
                    "Ask before a quicksave overwrites its slot or a quickload replaces the game in progress. Off: the keys act at once.")}},
            {"Retro look",{
                check("ps2_style","PS2-style filter",&s.ps2Style,"Chunkier surface textures, stepped shading and a subtle warm color finish. Keeps geometry, collision, HUD and menus unchanged. Off restores your normal rendering settings.",[](const Settings &d){return d.accelerated!=0;}),
                check("retro","Retro scanlines",&s.retro,"Dark lines between the rows of the picture, like an old CRT monitor."),
                check("palette","Doom palette colors",&s.palette,"Quantize the final lighting and fog to the current WAD palette.")}}}},
        {"Gameplay",{
            {"Automap / Gameplay",{
                check("enhanced_automap","Enhanced Automap",&s.enhancedAutomap,
                    "Color mapped key doors, mark discovered secret boundaries, and show active key pickups after collecting the Computer Area Map.")}},
            {"Monster performance (experimental)",{
                check("monster_hibernation","Idle monster hibernation",&s.monsterHibernation,"Reuse unchanged negative idle searches while keeping full physics, collision and state timing. Classic geometry only; disabled in digging/solid modes, patched gameplay, respawn, demos, multiplayer and Arcade."),
                check("swarm_debug","Swarm diagnostics",&s.swarmDebug,"Print an aggregate monster summary once per second: dormant, awake, reused idle searches, sound wakes and spawns.")}},
            {"Save recovery",{
                check("death_load_last_save","Load last save on death",&s.deathLoadLastSave,"After dying, press Use to load the last save successfully written or loaded this session. If no usable save is available, restart the map normally. Single player only; disabled in demos and Arcade.")}},
            {"Balance",{
                choice("monster_weapon_drops","Monster Weapon Drops",&s.monsterWeaponDrops,
                    {"Classic","Ammo Instead","Scarce Ammo"},{MONSTER_DROPS_CLASSIC,MONSTER_DROPS_AMMO,MONSTER_DROPS_SCARCE},
                    "Controls future Shotgun Guy and Chaingunner drops. Classic keeps their weapons. Ammo Instead drops ammo and Scarce Ammo drops less. Map pickups stay available. Strict modes can make some PWADs much harder. Single player only. Demos and Arcade use Classic.")}},
            {"Combat and arenas",{
                check("boost_chaingun","Enhanced chaingun",&s.boostChaingun,"50% faster chaingun fire. Backpack bullet capacity increases from 400 to 600; without a backpack it stays 200. Bullets are shared with the pistol. Turning off restores the normal cap and clips excess bullets. Works with classic and Fluid weapons; disabled in Arcade, demos and multiplayer."),
                check("monster_recovery","Stuck monster recovery",&s.monsterRecovery,"Helps monsters after repeated failed movement. Tries small escape steps first. Trapped lockdown enemies can teleport to safe explored ground; ordinary monsters require invalid footing and a nearby visible safe spot. Disabled in Arcade, demos and multiplayer."),
                check("key_arena","Key pickup lockdown arenas",&s.keyArena,"Picking up a new card or skull triggers three ammo-scaled waves totaling 7-25 monsters on explored ground. Announcement finishes before spawning. Maximum two of each heavy type and one of each boss per encounter. Red barrier releases after wave three. After two minutes the lockdown ends automatically and remaining encounter monsters are removed. Single player only; disabled in Arcade and demos."),
                choice("arena_radius","Arena radius",&s.arenaRadius,{"Compact","Standard","Large"},{384,512,768},"Radius in map units: 576 / 768 / 1152. Used for the next lockdown.",[](const Settings &d){return d.keyArena!=0;})}}}},
        {"Environment",{
            {"Effect controls",{
                check("environment_effects","Environmental reactions",&s.environmentEffects,"Master switch for pressure dust, settling dust, debris splashes and electrical sputtering. Collapse gameplay has separate controls under Destruction."),
                choice("environment_budget","Particle budget",&s.effectBudget,{"Low","Balanced","High"},{64,128,256},"Maximum active environmental dust particles. Existing weapon and liquid effects retain their own caps.",[](const Settings &d){return d.environmentEffects!=0;})}},
            {"Blasts and rubble",{
                check("rocket_chunks","Rocket voxel chunks",&s.rocketChunks,"Rocket impacts throw up to six wall-colored tumbling chunks. Cosmetic, with a shared cap of 48 pieces."),
                check("pressure_waves","Blast pressure waves",&s.pressureWaves,"Blasts push loose debris and throw out a ground dust wave. Cosmetic: does not push actors.",[](const Settings &d){return d.environmentEffects!=0;}),
                check("settling_dust","Destruction dust and settling",&s.settlingDust,"Short dust clouds settle into fading specks near damaged surfaces.",[](const Settings &d){return d.environmentEffects!=0;}),
                check("debris_liquids","Debris liquid reactions",&s.debrisLiquids,"Falling rubble makes small liquid-colored splashes. Heavy pieces sink; light flecks linger on the surface.",[](const Settings &d){return d.environmentEffects&&d.splashes;})}},
            {"Wind and airborne dust",{
                check("outdoor_wind","Outdoor wind dust",&s.outdoorWind,"Sparse dust gusts and tiny ground debris under open sky. Sheltered rooms stay clear; rain reduces dust."),
                slider("outdoor_wind_density","Wind dust density",&s.outdoorWindDensity,0.25,2,0.25,"Amount of windblown dust. Particle count stays capped.",[](const Settings &d){return d.outdoorWind!=0;})}},
            {"Leaks and machinery",{
                check("electrical_damage","Damaged machinery sputtering",&s.electricalDamage,"Broken powered panels and lights briefly sputter with sparks and local flashes. Requires Shootable lights and emissive lighting.",[](const Settings &d){return d.environmentEffects&&d.shootableLights&&d.emissive;})}}}},
        {"Effects",{
            {"Monster Effects",{
                check("revenant_soul_release","Revenant Soul Release",&s.revenantSoulRelease,
                    "A faint soul struggles free when a Revenant dies. Requires the accelerated 3D renderer.",
                    [](const Settings &d){return d.accelerated!=0;})}},
            {"Destruction and doors",{
                check("breakable_walls","Breakable surfaces (experimental)",&s.breakableWalls,solidMapText("Rocket and barrel blasts excavate shared wall, floor and ceiling geometry. Off prevents new damage; existing passages remain until restarting the level. New solid-map saves retain damage.","Rocket impacts excavate walls, floors and ceilings. Wall depth stops before the next map boundary. Off restores original surfaces and collision; existing damage returns when enabled.")),
                choice("wall_damage","Wall damage",&s.wallDamage,{"100%","150%","200%","250%","300%","350%","400%","450%","500%"},{100,150,200,250,300,350,400,450,500},solidMapText("Strength of new excavation: 100% is normal; 500% digs up to five times deeper per impact. Does not change damage to actors.","Excavation strength per hit against walls and doors. 100% is normal; 500% adds five times as much damage per hit, so breaches take fewer shots. Applies to new damage only. Monster damage and floor/ceiling craters are unchanged."),[](const Settings &d){return d.breakableWalls!=0;}),
                check("through_walls","Through-wall breaches (experimental)",&s.throughWalls,solidMapText("Allows new impacts to open passages through walls. Off limits future cuts to retain backing before the next room; existing breaches remain. Can bypass level progression.","Repeated rocket hits can breach walls up to 256 map units thick into another room. Requires Breakable surfaces. Outside-map walls stay sealed. Off restores the backing; damage is retained.")),
                check("player_surface_damage","Player weapon surface damage",&s.playerSurfaceDamage,solidMapText("Player bullets and non-rocket projectiles make small chips in solid surfaces. Requires Breakable surfaces. Rockets remain controlled by Breakable surfaces. Actor damage is unchanged.","Bullets and shotgun pellets gradually chip surfaces; plasma makes shallow craters and a direct BFG projectile hit digs harder than a rocket. Requires Breakable surfaces. Rockets retain their existing damage. Actor damage is unchanged."),[](const Settings &d){return d.breakableWalls!=0;}),
                check("monster_surface_damage","Monster projectile damage",&s.monsterSurfaceDamage,solidMapText("Non-rocket monster projectiles make small chips in walls, floors and ceilings on direct hits. Requires Breakable surfaces. Does not change damage to actors.","Monster fireballs, revenant missiles and arachnotron plasma chip walls, floors and ceilings at one eighth of rocket strength on direct hits. Requires Breakable surfaces. Does not change damage to actors or rocket explosions."),[](const Settings &d){return d.breakableWalls!=0;}),
                check("deeper_digging","Deeper digging (up to 4096 units)",&s.deeperDig,solidMapText("Increases the maximum depth of new excavation from 256 to 4096 map units. New tunnels remain enclosed by solid material. Existing cuts are retained when disabled.","Extends wall/door breaches to 4096 units and enclosed floor/ceiling digging to 256 units. Requires Through-wall breaches or Breachable doors plus Breakable surfaces. Only connects to a valid room; outside-map walls stay sealed. Off restores standard limits without erasing damage."),[](const Settings &d){return d.breakableWalls&&(d.throughWalls||d.breachDoors);}),
                check("door_blowout","Third-rocket door blowout",&s.doorBlowout,"Optional extra behaviour: three direct player rocket hits blow out a supported door permanently. Textured panels fly out, leaving torn frame edges. Off preserves normal destruction. Standard manual, key and switch doors are supported, including small and exit-marked doors. Unknown scripted actions are excluded. Existing blowouts remain when disabled.",[](const Settings &d){return d.breakableWalls!=0;}),
                check("key_door_shield","Key-locked door shields",&s.keyDoorShield,"Protect key-locked doors and their surrounding 128-unit frame area from destruction with a red glass shield. Existing damage is not repaired.",[](const Settings &d){return d.breakableWalls!=0;}),
                check("switch_door_shield","Switch-operated door shields",&s.switchDoorShield,"Protect switch and trigger doors and their surrounding 128-unit frame area from destruction with a red glass shield. Locked switch doors use the key shield setting.",[](const Settings &d){return d.breakableWalls!=0;}),
                check("breach_doors","Breachable doors (can break maps)",&s.breachDoors,"WARNING: Allows blasting through doors, including locked doors. Can bypass keys, switches and progression and break map scripts. Off by default. Requires Breakable surfaces. Holes move with the door. Independent of Through-wall breaches.",[](const Settings &d){return d.breakableWalls!=0;}),
                check("door_sparks","Opening door sparks",&s.doorSparks,"Small sparks along the side tracks while a nearby door opens. Cosmetic only.",[](const Settings &d){return d.breakableWalls!=0;})}},
            {"Collapse gameplay",{
                check("chain_collapses","Local chain collapses",&s.chainCollapses,"Unsupported fragments near new damage fall away. Falling fragments can chip another surface below. Limited to two generations; saved geometry and collision stay together. Requires Breakable surfaces and solid-map mode.",[](const Settings &d){return d.breakableWalls!=0;}),
                check("hanging_fragments","Delay before fragments fall",&s.hangingFragments,"Unsupported geometry briefly hangs before collapsing. Off makes it fall immediately. Requires Local chain collapses.",[](const Settings &d){return d.breakableWalls&&d.chainCollapses;}),
                choice("collapse_size","Largest detached fragment",&s.collapseSize,{"Small","Medium","Large"},{4,12,24},"Maximum connected field nodes in a removable fragment. Larger pieces cost more to check. Anchored structures remain intact.",[](const Settings &d){return d.breakableWalls&&d.chainCollapses;})}},
            {"Combat",{
                check("corrosive_barrels","Corrosive Barrels",&s.corrosiveBarrels,
                    "Damaged barrels seep green acid; ruptures splash nearby actors and leave persistent corrosive pools. Monsters and players take 5 HP every 17 tics, with two seconds of residual exposure; Radiation Suits protect the player. Single-player only; disabled in multiplayer and demos. Acid decals require the accelerated 3D renderer. Off preserves classic barrel behavior."),
                check("critical_parts","Critical parts",&s.criticalParts,
                    "Player bullets hit visible human enemy sprite pixels; headshots deal 2x damage. "
                    "Zombiemen, shotgun guys, chaingunners and Wolfenstein SS only. "
                    "Single-player sprites only; voxel models, multiplayer and demos keep normal hits. "
                    "Use mouse look for precise aiming. Wound holes are controlled separately.",
                    [](const Settings &d){return d.accelerated!=0;}),
                check("enhanced_weak_points","Enhanced Monster Weak Points",&s.enhancedWeakPoints,
                    "Anatomical bullet weak points on other Doom monsters. Single-player sprites only; human headshots keep their separate switch.",[](const Settings &d){return d.accelerated!=0;}),
                check("monster_weapon_vulnerabilities","Monster Weapon Vulnerabilities",&s.weaponVulnerabilities,
                    "Monster-specific plasma, rocket and BFG weaknesses. Single-player sprites only; direct hits and splash remain separate.",[](const Settings &d){return d.accelerated!=0;}),
                check("localized_injury_effects","Localized Injury Effects",&s.localizedInjuries,
                    "Temporary heavy head-hit aim disruption and mechanical rocket-leg immobilization. Requires the corresponding weak-point or weapon rule.",[](const Settings &d){return d.accelerated!=0;}),
                check("elemental_damage_over_time","Elemental Damage Over Time",&s.elementalDot,
                    "Actual burns: Imp fire 15% / 6x3 HP; Mancubus fire 10% / 4x5 HP; player plasma 12% / 5x4 HP. One stream per sprite monster; separate from cosmetic FX. Single-player only.",[](const Settings &d){return d.accelerated!=0;}),
                check("combat_event_messages","Colored Combat Notifications",&s.combatMessages,
                    "Red critical health alerts, yellow critical bleeding, and purple frenzy or eye injuries. Three short messages maximum.",[](const Settings &d){return d.accelerated!=0;}),
                check("severe_critical_bloodspill","Severe critical bloodspill",&s.severeBloodspill,
                    "Selected monsters spill blood when damage leaves them alive at 25% health or less. Requires blood and blood spray, and fewer than 200 kill-count monsters.",
                    [](const Settings &d){return d.accelerated&&d.blood&&d.bloodSpray;}),
                check("elemental_combat_fx","Elemental Combat FX",&s.elementalCombatFX,
                    "Cosmetic Mancubus/Arch-vile ignition, plasma feedback and charred corpses. No burn damage. Single-player sprites only.",[](const Settings &d){return d.accelerated!=0;}),
                check("advanced_monster_reactions","Advanced Monster Reactions",&s.advancedMonsterReactions,
                    "With Monster Bleeding: Mancubus stagger, wounded Baron aggression and Arch-vile health siphoning. Optional; single-player sprites only.",[](const Settings &d){return d.accelerated!=0;}),
                check("eye_injuries","Eye Injuries",&s.eyeInjuries,
                    "15% per damaging eye-hit discharge: Cacodemons and Pain Elementals fire blindly until eating a corpse. Spider Mastermind rages for ten seconds, deals double player damage and takes at least double damage; existing larger bonuses do not stack. Requires Enhanced Monster Weak Points. Single-player sprites only.",
                    [](const Settings &d){return d.accelerated&&d.enhancedWeakPoints;}),
                check("monster_bleeding","Monster Bleeding",&s.monsterBleeding,
                    "Player bullets and pellets: 30% bleeding chance on Imp, Hell Knight, Mancubus and Pinky; enabled criticals guarantee bleeding. Repeated discharges raise severity: 2/3/4 HP per second for 6/8/10 seconds. Walking remains 20% slower.",[](const Settings &d){return d.accelerated!=0;}),
                check("monster_feeding","Monster Feeding",&s.monsterFeeding,
                    "Cacodemons/Pinkies below 30% health search faster and retreat to eat; after 10 seconds without recovery they return aggressively to combat. Arch-viles below 50% hunt living biological prey to steal health. Stops at 60%. Single-player sprites only.",[](const Settings &d){return d.accelerated!=0;}),
                check("headshot_sound","Headshot Sound",&s.headshotSound,
                    "Short confirmation on a damaging anatomical headshot, once per discharge. Headshot Feedback Off also silences it.",[](const Settings &d){return d.accelerated!=0;}),
                check("headshot_frenzy","Headshot Frenzy",&s.headshotFrenzy,
                    "Close super-shotgun head hits: 40% chance for surviving Mancubus, Hell Knight and Baron to attack nearby monsters for ten seconds. Requires Enhanced Monster Weak Points.",[](const Settings &d){return d.accelerated!=0;}),
                check("monster_metal_sparks","Monster Metal Sparks",&s.monsterMetalSparks,
                    "Orange door-track sparks on confirmed Arachnotron and Spider Mastermind metal-leg impacts. Cosmetic only.",[](const Settings &d){return d.accelerated!=0;}),
                choice("headshot_feedback","Headshot Feedback",&s.headshotFeedback,{"Off","HS Indicator","Impact Effect","Both"},{0,1,2,3},
                    "Blood-colored impact and/or amber HS text for confirmed damaging headshots. HS text works with the crosshair off. Off also suppresses headshot sound.",[](const Settings &d){return d.accelerated!=0;}),
                check("material_rubble","Match rubble to surface",&s.materialRubble,"Crater interiors, chips and dust inherit the nearby wall or floor color. Off restores grey stone."),
                check("gun_impacts","Gun impacts",&s.gunImpacts,"Uzi animated sparks, voxel dust and impact puff from Stewie-Brian Arsenal. Disable to restore original bullet puffs. Blood and shootable lights have separate options."),
                check("blood","Blood splats and pools",&s.blood,
                    "Hitscan hits leave splats on floors and nearby walls; corpses grow a pool. Colors come from the WAD's blood sprite."),
                check("blood_spray","Blood spray and trails",&s.bloodSpray,"Flying blood droplets, wound drips and smears. Cosmetic only."),
                check("blood_shine","Wet blood shine",&s.bloodShine,"Fresh blood on the floor glints in a brighter palette red under dynamic lights, then dries matte.",
                    [](const Settings &d){return d.blood!=0;}),
                check("monster_skin_damage","Monster skin damage",&s.monsterSkinDamage,
                    "Purely cosmetic health-based scratches and blood on the body of stock Imps, Barons, Pinkies and Spectres. "
                    "No impact tracking, no gameplay changes. Keeps heads untouched, excludes replacement sprites and voxel models. "
                    "Independent of Wound holes and Monster Bleeding; Off by default.",
                    [](const Settings &d){return d.accelerated!=0;}),
                check("wounds","Wound holes",&s.wounds,
                    "Bullets and pellets leave holes with ragged bloody edges, dark torn rims and short blood smears. "
                    "The marks follow the monster's animation. Close hits widen a hole; seen from the side it closes. "
                    "Drawing only: the monster is hit as always."),
                check("varied_deaths","Varied deaths",&s.deathDirector,
                    "No two monsters die alike: the death frames keep their order but take new timing each kill (a snap, a "
                    "hesitation then a drop, a slump), monsters dropped by one blast differ on purpose, and the body settles "
                    "as it lands. Now and then a hit off the middle turns it, or a hard push steps it back. Drawing only."),
                choice("death_falls","Procedural falls",&s.deathFalls,{"Off","By the hit","Legs swept","Chest blast","Head shot"},{0,1,2,3,4},
                    "Monsters standing on two legs fall a way their drawings never show, on joints placed by hand or found in their "
                    "own art. A weak or low hit buckles the knee on its side and the body goes over, toward its weapon or away; a "
                    "hard hit snaps the shoulders back and knocks the body down, sooner and farther the harder the hit; a killing "
                    "shot through the head cuts the strings and the body crumples. Drawing only: damage is the game's own. The "
                    "fixed choices are for judging each one.",
                    [](const Settings &d){return d.deathDirector!=0;}),
                check("death_face_viewer","Dying turn to face you",&s.deathFaceViewer,
                    "A monster killed facing away turns toward you before its death frames, which are drawn facing the viewer, "
                    "instead of snapping round.",
                    [](const Settings &d){return d.deathDirector!=0;}),
                check("death_motion","Death reactions",&s.deathMotion,
                    "Dying monsters jolt with the killing blow: thrown back by high hits, legs knocked out by low ones, crumpling "
                    "under the chainsaw, hopping when gibbed, swaying as fliers fall. Drawn in whole-pixel rows over the game's own "
                    "death frames, and gone by the corpse frame."),
                check("hit_flinch","Hit flinches",&s.hitFlinch,"Monsters lean with the push of every hit and spring back. Drawing only: the game sees no difference."),
                slider("body_motion_strength","Reaction strength",&s.bodyMotionStrength,0.5,3,0.25,
                    "How far death reactions and hit flinches throw, bend and buckle the body.",
                    [](const Settings &d){return d.deathMotion!=0||d.hitFlinch!=0;}),
                check("soft_effects","Soft effect sprites",&s.softSprites,"Explosions, puffs and teleport fog stop slicing into walls and dissolve on a dither at floors."),
                check("shootable_lights","Shootable lights",&s.shootableLights,
                    "Computer panels and tech walls throw blue sparks when shot. A shot or fireball into the glowing part "
                    "of a lamp or screen breaks its whole fixture, and rockets and barrels break those around them: they sputter out "
                    "and go dark, and the room darkens with them a moment later. Render-only: monsters see as before, and the lights "
                    "are back after a reload.",
                    [](const Settings &d){return d.emissive!=0;}),
                check("kill_shots_carry","Kill shots carry on",&s.killShotsCarry,
                    "A bullet or pellet that kills what it hits carries on past the body, so a fight can shoot out the lamps and "
                    "screens behind the monsters. Only for sparks and lights, never damage: the game sees the shot stop as always.",
                    [](const Settings &d){return d.emissive!=0&&d.shootableLights!=0;})}}}},
        {"Textures",{
            {"Filtering",{
                choice("filter","Walls and floors",&s.filter,{"Crisp pixels","Smooth (bilinear)","Sharp bilinear"},{0,1,2},
                    "Sharp bilinear keeps every texel flat and crisp and only softens the step between texels, "
                    "by about a screen pixel plus the edge softness below."),
                slider("sharp_softness","Edge softness",&s.sharpSoftness,0,1,0.05,
                    "With sharp bilinear walls and floors: how far texel edges blend, in texels. Left keeps only a one-pixel edge; right approaches bilinear.",
                    [](const Settings &d){return d.filter==2;}),
                check("palette_mipmaps","Palette mipmaps",&s.paletteMips,
                    "With smooth or sharp walls and floors: far and grazing surfaces read smaller versions snapped to the WAD palette, "
                    "as software Quake did, instead of shimmering. Crisp pixels stay vanilla.",
                    [](const Settings &d){return d.filter!=0;}),
                choice("sprite_filter","Sprites",&s.spriteFilter,{"Crisp pixels","Soft (bilinear)"},{0,1},
                    "Crisp keeps the artwork's pixels square; soft blends them smoothly.")}},
            {"Surface detail",{
                check("surface_detail","Bump detail and metal gloss",&s.detail,
                    "Bump detail and metal gloss derived from the WAD artwork; affects dynamic lights, and static ones with bake-only lights."),
                check("varied_highlights","Varied highlights",&s.variedHighlights,
                    "Gloss highlights are tight and bright where the artwork runs smooth and broad and dim where it is busy, "
                    "with slime and blue panels sharper than worn grey metal.",
                    [](const Settings &d){return d.detail!=0;}),
                check("uneven_tiles","Uneven floor tiles",&s.unevenTiles,
                    "Tiles, plates and stones painted into floors each lie a little crooked, as if laid by hand: "
                    "lights catch some and miss others, and a thin lip lights up or falls into shadow where neighbors meet. Found from the artwork's seams at load.",
                    [](const Settings &d){return d.detail!=0;}),
                check("uneven_walls","Uneven wall and ceiling tiles",&s.unevenWalls,
                    "The same for the plates, panels and bricks painted into walls and ceilings; walls split only into straight rows and joints.",
                    [](const Settings &d){return d.detail!=0;}),
                check("weapon_lighting","Relit weapon",&s.weaponLighting,
                    "The sheen and light painted into the weapon artwork are taken out, and the lights around you relight it: "
                    "their color, darker rounded edges, glowing rims against lights behind it, and highlights that slide over "
                    "the metal as you look around and the weapon bobs."),
                check("window_glass","Window glass",&s.windowGlass,
                    "Windows found in the map's geometry get a pane of glass: grimy at the edges, streaked with highlights that "
                    "slide as you move, reflecting the sky from outside. The first shot, projectile or blast through it shatters "
                    "it into large, reflective shards that tumble and bounce; shots and sight pass exactly as in vanilla.",hasGlass),
                check("progressive_glass","Progressive glass damage",&s.progressiveGlass,"Bullets chip and crack glass before it fractures. Off breaks it immediately."),
                check("glass_reflections","Glass reflections",&s.glassReflections,
                    "Experimental: the window filling most of the view mirrors the room in front of it, monsters and shots "
                    "included, drawn a second time like the liquid reflections and at their resolution. Costs about as much as they do.",
                    [](const Settings &d){return d.windowGlass!=0;})}},
            {"Selected Texture Immersion",{
                check("living_surfaces","Selected Texture Immersion",&s.livingSurfaces,
                    "Animates selected organic walls, teleports, fire, switches, and Doom II skies. Default off."),
                choice("living_intensity","Intensity",&s.livingIntensity,{"Low","Medium","High"},{1,2,3},
                    "Adjusts the strength of all enabled immersion effects.",
                    [](const Settings &d){return d.livingSurfaces!=0;}),
                check("living_morph","Morph",&s.livingMorph,
                    "Moves selected artwork and sky layers with the chosen texture filter.",
                    [](const Settings &d){return d.livingSurfaces!=0;}),
                check("living_pulse","Organic pulse",&s.livingPulse,
                    "Adds breathing, blinking, surging energy, and flickering light.",
                    [](const Settings &d){return d.livingSurfaces!=0;}),
                check("living_wetness","Wetness",&s.livingWetness,
                    "Adds surface blood runoff and wet highlights to selected flesh and torn skin.",
                    [](const Settings &d){return d.livingSurfaces!=0;})}},
            {"Glow",{
                check("emissive","Emissive textures",&s.emissive,
                    "Lamp strips, computer displays, lava and nukage glow with soft bloom and colored light on nearby surfaces."),
                check("glossy_screens","Glossy monitor screens",&s.glossyScreens,
                    "Screens found in computer textures get curved CRT glass: the picture bends and shifts behind it as you move, "
                    "and it catches sharp highlights from lamps, shots and the flashlight."),
                check("live_readouts","Live readouts",&s.liveReadouts,
                    "The blocks of glowing cells painted into computer panels (the green grids beside DOOM's consoles) "
                    "change now and then, cell by cell, each taking the light of another in its row."),
                check("glowing_eyes","Glowing eyes",&s.glowingEyes,
                    "The eyes painted into monster artwork glow in the dark: the zombies' and imp's red, the demon's yellow, "
                    "the cacodemon's and mancubus's green, the arachnotron's blue. Picked by hand for stock monsters, searched for in other artwork; bloom needs emissive textures.")}}}},
        {"Monster AI",{
            {"Navigation",{
                check("advanced_tactics","Advanced tactics",&s.advancedTactics,"WARNING: Can make the game hard, and very hard with pistol starts. Adds monster-specific movement, cover, suppression and predictive attacks. Single-player only."),
                check("aggressive_ai","Aggressive AI",&s.aggressiveAI,"WARNING: Can make the game hard, and very hard with pistol starts. Adds sustained attacks, faster gunfire, repositioning and barrages. Single-player only."),
                check("persistent_pursuit","Persistent pursuit",&s.persistentPursuit,"Alerted monsters search around walls and retry blocked routes. Off keeps original AI. Single-player only; does not wake unaware monsters.")}}}},
        {"Bundled mods",{
            {voxelDetected()?"VoxelDoom detected":"VoxelDoom not loaded",{
                check("voxel_models","Voxel models",&s.voxelModels,"Load VoxelDoom through the launcher to use Cheello's voxel models. Missing frames use sprites. Uses native animation and lighting; pack scripts are not executed.",[](const Settings &d){return d.accelerated&&voxelDetected();}),
                check("voxel_eyes","Voxel glowing eyes",&s.voxelEyes,"Living voxel monsters use detected eye surfaces. Requires Glowing monster eyes and Voxel models.",[](const Settings &d){return d.accelerated&&voxelDetected()&&d.voxelModels&&d.glowingEyes;})}},
            {"Enable / disable",{
                check("mod_fluid_weapons","FluidWeapons",&s.fluidWeapons,"Toggle weapon animations and Fluid firing behavior. Applies immediately."),
                check("mod_fluid_sounds","FluidWeapons SFX add-on",&s.fluidSounds,"Toggle the enhanced weapon sounds independently. Applies immediately."),
                check("mod_fluid_monsters","FluidMonsters",&s.fluidMonsters,"Toggle smooth monster walking. Applies immediately; demos keep original movement.")}}}},
        {"Fluid Weapons",{
            {"Smoke and details",{
                check("fluid_firing_smoke","Muzzle smoke",&s.fluidFiringSmoke,"Original smoke artwork after shots."),
                check("fluid_reload_smoke","Reload smoke",&s.fluidReloadSmoke,"Smoke from shotgun chambers and open super-shotgun barrels."),
                check("fluid_hot_smoke","Hot barrel smoke",&s.fluidHotSmoke,"Lingering wisps after sustained fire."),
                slider("fluid_smoke_duration","Barrel fume duration",&s.fluidSmokeDuration,1,5,.5,
                    "Scales heat gained per shot. Default 2x; shotgun heat is capped to prevent prolonged fumes after repeated shots. Does not enlarge particles.",
                    [](const Settings &d){return d.fluidWeapons&&d.fluidHotSmoke;}),
                check("fluid_smoke_bio","Original Bio smoke texture",&s.fluidSmokeBio,"Fluid Weapons optional flowing smoke texture."),
                check("fluid_indoor_smoke","Indoor smoke buildup",&s.fluidIndoorSmoke,"Lingering indoor smoke. Repeated rocket explosions build a haze that clears over time."),
                check("fluid_saw_smoke","Chainsaw exhaust",&s.fluidSawSmoke,"Exhaust smoke while the chainsaw runs."),
                check("fluid_saw_blood","Chainsaw blade blood",&s.fluidSawBlood,"Blood on the blade after cutting a bleeding enemy. Fades over time."),
                check("fluid_idle_gestures","Idle gestures",&s.fluidIdleGestures,"Original weapon idle gestures.")}},
            {"Rocket launcher",{
                choice("fluid_rocket_fire","Rocket fire effect",&s.fluidRocketFire,
                    {"Soft glow","Grainy fire"},{0,1},
                    "Choose the earlier soft glow or the grainy Fluid-style fire. Uses the selected fire style; original impact smoke is preserved. Rocket height, trajectory and weapon position stay the same.")}},
            {"Gloves",{
                check("fluid_bare_hands","Bare fists",&s.fluidBareHands,"Show bare hands with fists; disable to use your glove color."),
                check("fluid_fast_pistol","Fast pistol",&s.fluidFastPistol,"Fluid Weapons fast pistol: 3.5 shots/sec; off restores 2.5 shots/sec."),
                choice("fluid_gloves","Glove color",&s.fluidGloves,
                    {"Classic tan","Black","Brown","Green","Red","Grey","Blue","Custom"},{0,1,2,3,4,5,6,7},
                    "Recolors Fluid Weapons gloves. Classic tan is the mod's default."),
                slider("fluid_glove_red","Highlight red",&s.fluidGloveRed,0,255,1,"Custom glove highlight color.",[](const Settings&d){return d.fluidGloves==7;}),
                slider("fluid_glove_green","Highlight green",&s.fluidGloveGreen,0,255,1,"Custom glove highlight color.",[](const Settings&d){return d.fluidGloves==7;}),
                slider("fluid_glove_blue","Highlight blue",&s.fluidGloveBlue,0,255,1,"Custom glove highlight color.",[](const Settings&d){return d.fluidGloves==7;}),
                slider("fluid_shade_red","Shadow red",&s.fluidShadeRed,0,255,1,"Custom glove shadow color.",[](const Settings&d){return d.fluidGloves==7;}),
                slider("fluid_shade_green","Shadow green",&s.fluidShadeGreen,0,255,1,"Custom glove shadow color.",[](const Settings&d){return d.fluidGloves==7;}),
                slider("fluid_shade_blue","Shadow blue",&s.fluidShadeBlue,0,255,1,"Custom glove shadow color.",[](const Settings&d){return d.fluidGloves==7;})}}}},
        {"Lighting",{
            {"Baked at level load",{
                check("map_light_cache","Cache map lighting",&s.mapLightCache,"Reuse baked lighting from disk when map, resources, settings and static world state match. Live effects remain dynamic. Cache is limited to 1 GiB."),
                  check("baked_lights","Baked static lights",&s.bakedLights,
                    "Torches, lamps and glowing textures light the whole level, baked at level load; nearby ones still flicker."),
                check("window_light","Window light",&s.windowLight,
                    "Windows, open doorways and holes in the ceiling let the outdoor light in: they light the rooms beside them "
                    "like big soft lamps in the sky's color, fading deeper into the room. Baked at level load.",
                    [](const Settings &d){return d.bakedLights!=0;}),
                slider("window_strength","Window light strength",&s.windowStrength,0.5,2,0.25,
                    "How brightly the outdoors shines in through windows and openings. Changing it re-bakes the lights.",
                    [](const Settings &d){return d.bakedLights!=0&&d.windowLight!=0;}),
                check("dim_lit_rooms","Darker rooms around lamps and windows",&s.dimLitRooms,
                    "Rooms that lamps, screens and windows light well lose part of their even light level, so the light "
                    "visibly comes from them and the far corners fall darker. Baked at level load.",
                    [](const Settings &d){return d.bakedLights!=0;}),
                slider("unlit_darkening","Darker rooms without lamps",&s.unlitDarkening,0,0.85,0.05,
                    "Remove residual even lighting from enclosed interiors. Above 50%, extra darkness applies only "
                    "where very little steady static light reaches the room; actual lamps, windows and shots still illuminate it. "
                    "Changing this setting re-bakes lighting.",
                    [](const Settings &d){return d.bakedLights!=0&&d.dimLitRooms!=0;},
                    {"Off","5%","10%","15%","20%","25%","30%","35%","40%","45%","50%",
                     "55%","60%","65%","70%","75%","80%","85%"}),
                check("sun_shadows","Sun shadows",&s.sun,"Shadows and sunlit patches baked at level load from the sky texture and map geometry."),
                check("bounce_light","Bounce light",&s.bounce,
                    "Sunlit and lamp-lit surfaces tint and lift what faces them, including ceilings; baked at level load."),
                slider("bounce_tint","Room color from bounce",&s.bounceTint,0,4,0.5,
                    "Light from colored lamps, bounced around the room, tints its even light level too, so corners "
                    "the lamps never reach directly still take their color.",
                    [](const Settings &d){return d.bakedLights!=0&&d.bounce!=0;}),
                check("sky_light","Sky light",&s.skyLight,
                    "A cool fill from the visible sky, tinted by the sky texture: windows glow faintly, overhangs and alcoves outdoors stay darker."),
                check("baked_occlusion","Ambient occlusion",&s.bakedAO,"Corners, ledges, stair risers and alcoves darken in light-level steps."),
                check("decoration_shadows","Decoration shadows",&s.thingShadows,
                    "Columns, trees and hanging bodies cast baked sun and lamp shadows shaped like their sprites. Changing it re-bakes the level."),
                check("texel_lighting","Texel-aligned baked light",&s.texelLight,
                    "Baked lights, sun patches and bounce blend smoothly but step per texture pixel, instead of in hard 4-5 unit map cells.")}},
            {"Live light",{
                check("green_impact_fire","Infernal green fire impacts",&s.greenImpactFire,
                    "Hell Knight and Baron fireballs leave temporary green flames on walls and floors. Active only on maps with fewer than 200 monsters."),
                check("soft_light","Light seams and contact shading",&s.softLight,"Blend sector light across openings and darken floors along walls."),
                check("light_flow","Sector light flow",&s.lightFlow,
                    "Brighter rooms light the floors and walls beyond their openings; flickering rooms flicker faintly next door."),
                check("door_light","Door light spill",&s.doorLight,"Light from a brighter room spills through a door as it opens."),
                check("moving_relight","Moving doors re-light",&s.movingRelight,
                    "Baked lamp light and sunlight follow doors, lifts and lowering walls: as a door rises, the light beyond it "
                    "steps in under the edge, re-baked around it in the background.",
                    [](const Settings &d){return d.bakedLights!=0||d.sun!=0;}),
                check("eye_adaptation","Eye adaptation",&s.eyeAdaptation,"Brief stepped over- or underexposure when you move between bright and dark areas.")}},
            {"Flashlight and shadows",{
                slider("flashlight_tint","Flashlight tint",&s.flashlightTintGain,0,2,0.5,"Subtle target-color tint: left is off, middle is the default, right is stronger.",hasFlashlight),
                check("classic_flashlight","Classic flashlight",&s.classicFlashlight,
                    "A shorter, warmer beam that works like Doom's own light levels: it brightens what it hits in "
                    "hard steps to a little past full brightness and no further, so monsters keep their palette colors.",hasFlashlight),
                check("stippled_shadows","Stippled shadow edges",&s.stippledShadows,
                    "The shadows walls, ledges and monsters throw in the flashlight stay hard but break into a coarse dither at the edge: "
                    "a pixel where the wall meets its shadow, wider the farther the shadow falls behind it.",hasFlashlight),
                check("flashlight_shadows","Flashlight silhouettes",&s.flashlightShadows,"Things caught in the flashlight throw their sprite outline onto the wall behind them.",hasFlashlight),
                check("player_shadow","Player shadow",&s.playerShadow,"Your own sprite's shadow from the sun or the strongest light above you.")}}}},
        {"Atmosphere",{
            {"Air",{
                check("fog","Atmospheric fog",&s.fog,"Height fog and soft cloud layers derived from pits, liquids and damaging floors."),
                check("sun_shafts","Sunbeams",&s.sunShafts,
                    "Soft shafts of sunlight slant down through ceiling holes and windows, where the dust motes glint. Needs sun shadows.",
                    [](const Settings &d){return d.sun!=0;}),
                check("sun_scatter","Sunbeam scattering",&s.sunScatter,
                    "Sunbeams glow brighter when you look toward the sun through them and fainter from behind, as dust scatters light onward.",
                    [](const Settings &d){return d.sun!=0&&d.sunShafts!=0;}),
                choice("immersive_sky","Immersive sky environment",&s.immersiveSky,
                    {"Off (original sky)","Auto (DOOM episodes)","Rocky mountains","Ice mountains","Inferno","Burning city"},
                    {0,1,2,3,4,5},
                    "Cosmetic distant scenery and depth-separated clouds. Off preserves the original sky. Auto uses classic DOOM episode/map conventions; for replacement skies choose a profile or Off."),
                check("sun_disc","Sun in the sky",&s.sunDisc,"A faint sun disc and soft glow where the baked sunlight comes from, over the sky texture's brightest part."),
                check("dust_motes","Dust motes",&s.dust,"Specks that glint in indoor sunbeams and the flashlight beam."),
                check("embers","Embers",&s.embers,"A few sparks rise from torches and fires and cool as they climb; where the game's wind blows, it carries them off."),
                check("heat_haze","Heat haze",&s.heatHaze,"Rows of pixels shimmer above lava, hot damaging floors and flames.")}},
            {"Decoration Immersion",{
                check("decoration_ambience","Decoration immersion",&s.decorationAmbience,
                    "Purely cosmetic flies and worms around corpses and organic decorations, after a delay. "
                    "Automatically disabled on maps with 200 or more monsters."),
                choice("decoration_insects","Corpse insects",&s.decorationInsects,
                    {"Off","Flies","Flies and worms"},{0,1,2},
                    "Flies may circle corpses or hanging and impaled remains. Worms crawl only over settled ground-level remains.",
                    [](const Settings &d){return d.decorationAmbience!=0;}),
                choice("decoration_delay","Decay delay",&s.decorationDelay,
                    {"Short","Normal","Long"},{0,1,2},
                    "Random delay after monster death (or from level start for placed decorations): "
                    "Normal is 15-30 seconds for flies and 30-60 for worms.",
                    [](const Settings &d){return d.decorationAmbience!=0&&d.decorationInsects!=0;}),
                check("corpse_decomposition","Corpse decomposition",&s.corpseDecomposition,
                    "Dried and discolored death artwork for selected Zombieman, Shotgun Guy, Imp, Pinky, Mancubus, "
                    "Cacodemon, Hell Knight and Baron corpses. Spectre and Chaingunner are excluded. "
                    "At Stage 2, selected corpses lose scavengers and associated blood pools. "
                    "Independent of insects, disabled at 200 or more monsters, cosmetic only.",
                    [](const Settings &d){return d.accelerated!=0;}),
                choice("corpse_decay_delay","Decomposition begins after",&s.corpseDecayDelay,
                    {"2 minutes","5 minutes","8 minutes","10 minutes"},{2,5,8,10},
                    "Gameplay minutes after a corpse is detected in the map-wide scan. "
                    "Stages 2/3 follow after another 45/120 seconds. Age resets after loading a save or changing maps.",
                    [](const Settings &d){return d.accelerated!=0&&d.corpseDecomposition!=0;})}},
            {"Authored episodes",{
                check("authored_episodes","Authored episodes",&s.authored,
                    "Looks picked by hand for the game's own episodes, beyond what DERP derives from the WAD: Knee-Deep in the Dead "
                    "lies at night in a downpour, the sun hidden and the lit rooms shining out through their windows into the dark. "
                    "Off: every map is lit as the WAD gives it, sun and all. Maps from custom WADs never get these looks.",hasAuthored)}},
            {"Weather and wear",{
                choice("episode_atmosphere","Episode atmosphere",&s.episodeAtmosphere,
                    {"Off","Authored episodes","Inferno (manual)","Deimos (manual)",
                     "Follow active SKY1/2/3","Starport duststorm (manual)",
                     "Burning City (manual)","Blood Hell (manual)"},{0,1,2,3,4,5,6,7},
                    "Classic Deimos/Inferno and DOOM II SKY1 Starport duststorms, SKY2 Burning City smoke, "
                    "and SKY3 Blood Hell rain and fog. Follow active sky explicitly opts custom WADs in; "
                    "Off disables themed particles/haze. Themed dust and blood drips do not create water wetness.",
                    [](const Settings &d){return d.accelerated!=0;}),
                choice("episode_particles","Episode atmosphere particles",&s.episodeParticles,
                    {"Low","Normal","High"},{0,1,2},
                    "Inferno ash/embers: 48/12, 96/24, 128/32; Deimos ice/dust: 180/80, 320/144, 480/220; "
                    "Starport fine sand/heavy grit: 80/16, 128/24, 160/32; "
                    "Burning City smoke/ash: 16/24, 28/40, 40/56; "
                    "Blood Hell droplets/embers: 64/12, 96/24, 120/32. "
                    "Only open-sky sectors and swept sky-exposed paths are eligible.",
                    [](const Settings &d){return d.accelerated!=0&&d.episodeAtmosphere!=0;}),
                choice("deimos_fog","Deimos blizzard visibility",&s.deimosFog,
                    {"Off","Light","Heavy","Whiteout"},{0,1,2,3},
                    "Deimos-only outdoor distance fog. Light preserves distant detail; Heavy brings fog into the ice field; "
                    "Whiteout sharply reduces outdoor sight lines. Does not change gameplay detection or enclosed-room fog. "
                    "Requires Fog enabled. Independent of ice-particle density.",
                    [](const Settings &d){return d.accelerated!=0&&d.episodeAtmosphere!=0&&d.fog!=0;}),
                check("episode_lightning","Themed episode lightning",&s.episodeLightning,
                    "Crimson lightning in Inferno and Blood Hell, pale electric blue-white in Deimos. "
                    "Allows thunder on dry episode maps when Thunderstorms is enabled; Rain=Never stays dry.",
                    [](const Settings &d){return d.accelerated!=0&&d.episodeAtmosphere!=0;}),
                check("inferno_tree_strikes","Hell tree lightning strikes",&s.infernoTreeStrikes,
                    "Cosmetic crimson branches may hit outdoor original small (TRE1) or large (TRE2) dead trees. "
                    "Each strike replaces ordinary thunder with the close recording, and has a 20% chance "
                    "to ignite a 24-second cosmetic fire with drifting embers. Burned-out trees remain charcoal-dark "
                    "until the level resets (up to 64 tracked). No actor damage or gameplay changes. "
                    "Available in Inferno or Blood Hell with themed lightning, thunderstorms and visible bolts. Other maps are unaffected.",
                    [](const Settings &d){return d.accelerated!=0&&d.episodeAtmosphere!=0&&
                        d.episodeLightning!=0&&d.stormMode!=0&&d.stormBolts!=0;}),
                choice("weather","Rain",&s.weather,{"Never","Authored episodes","Always"},{0,1,2},
                    "Rain falls under the sky in streaks of whole pixels and splashes where it lands; drops gather and run down "
                    "the outside of window glass, the ground darkens as it soaks, and you hear it. Authored episodes: it rains where "
                    "an authored episode's look has rain (DOOM's first episode). Always: on every map. Through a broken window the wind "
                    "blows the rain in, and the floor and wall beneath it get wet."),
                choice("storm_mode","Thunderstorms",&s.stormMode,{"Off","Subtle","Normal","Dramatic"},{0,1,2,3},
                    "Optional lightning and delayed close, medium or distant thunder with active rain, "
                    "or in an enabled dry Inferno, Deimos or Blood Hell profile with themed lightning. Off disables thunder.",
                    [](const Settings &d){return d.accelerated!=0&&(d.weather!=0||
                        (d.episodeAtmosphere!=0&&d.episodeLightning!=0));}),
                check("storm_strong_flashes","Allow strong lightning flashes",&s.stormStrongFlashes,
                    "Off limits bright pulses. Sky-only flashes never rebuild lighting or brighten indoor geometry.",
                    [](const Settings &d){return d.accelerated!=0&&d.stormMode!=0&&(d.weather!=0||
                        (d.episodeAtmosphere!=0&&d.episodeLightning!=0));}),
                check("storm_bolts","Visible distant lightning bolts",&s.stormBolts,
                    "Procedural branching lightning behind the outdoor sky and immersive clouds. "
                    "Disable to retain diffuse flashes and thunder without visible forks.",
                    [](const Settings &d){return d.accelerated!=0&&d.stormMode!=0&&(d.weather!=0||
                        (d.episodeAtmosphere!=0&&d.episodeLightning!=0));}),
                check("storm_atmosphere","Advanced storm atmosphere",&s.stormAtmosphere,
                    "When a rainy thunderstorm is active, gradually builds and fades rain, adds wind gusts, "
                    "bounded distant rain curtains and light outdoor haze. Dry episodes remain dry. "
                    "No lighting rebake, new actors or gameplay changes.",
                    [](const Settings &d){return d.accelerated!=0&&d.weather!=0&&d.stormMode!=0;}),
                slider("storm_volume","Thunder volume",&s.stormVolume,0,100,5,
                    "Thunder level, in percent. The standard sound-effects volume remains authoritative.",
                    [](const Settings &d){return d.accelerated!=0&&d.stormMode!=0&&(d.weather!=0||
                        (d.episodeAtmosphere!=0&&d.episodeLightning!=0));}),
                check("worn_plates","Worn plates",&s.wornPlates,
                    "Plain metal plates show years of use: grime packed into their seams, dull blotches, boot scuffs low down, "
                    "and a duller sheen. Only on plates that are otherwise blank, such as the fronts of DOOM's computer desks."),
                check("window_streaks","Window streaks",&s.windowStreaks,
                    "Rain has run down the outside walls below windows for years: dark ragged streaks from the sill, rusty on "
                    "metal. Inside they start once rain blows in through a broken pane. Needs window glass.",hasGlass)}},

            {"Liquids",{
                check("reflections","Reflections",&s.reflections,
                    "Mirror the scene in nearby water; renders the view a second time at half resolution."),
                check("retro_reflections","Retro reflections (PS1 style)",&s.retroReflections,
                    "Eighth resolution, hard pixels, Doom palette colors and a stepped wobble.",
                    [](const Settings &d){return d.reflections!=0;}),
                check("caustics","Caustics",&s.caustics,
                    "Rippling light from water, nukage, slime, blood and lava on the walls around them; pattern taken from the level's liquid flat."),
                check("ceiling_caustics","Caustics on ceilings",&s.ceilingCaustics,"The ripples also play on the ceiling above water, nukage, slime and lava.",
                    [](const Settings &d){return d.caustics!=0;}),
                check("caustics_computed","Computed caustics",&s.causticsComputed,
                    "The light the liquid's waves actually gather, worked out from its flat at level load: soft near the water, "
                    "sharp lines about 40 units up, broader above. Off shows the flat's own bright streaks.",
                    [](const Settings &d){return d.caustics!=0;}),
                check("caustics_grow","Caustics grow with distance",&s.causticsGrow,
                    "The shapes widen from 2 to 4 units per pixel the farther the light travels from the water; off keeps one unit per pixel like the floor.",
                    [](const Settings &d){return d.caustics!=0;}),
                check("caustics_angle","Flashlight reflection angle",&s.causticsAngle,
                    "Water reflects more of the beam the flatter it strikes: aiming across a pool lights the walls, aiming straight down barely does.",
                    [](const Settings &d){return d.caustics!=0;}),
                check("caustics_sway","Caustic sway",&s.causticsSway,
                    "The pattern sways with the wave slope, more the higher up the wall, like light dancing under a bridge.",
                    [](const Settings &d){return d.caustics!=0;}),
                check("caustics_sprites","Caustics on monsters and things",&s.causticsSprites,
                    "Monsters, items and decorations near water, nukage, slime and lava catch the ripples too, in their own colors.",
                    [](const Settings &d){return d.caustics!=0;}),
                check("caustics_shots","Caustics from shots",&s.causticsShots,
                    "Muzzle flashes and projectiles over liquids throw brief ripples on the walls around them, like the flashlight. Subtle next to their direct light.",
                    [](const Settings &d){return d.caustics!=0;}),
                check("damp_shores","Damp shores",&s.dampShores,
                    "Walls and banks just above water, nukage, slime and blood turn darker and damp up to a ragged line, "
                    "a little stained by the liquid. Lava stays dry."),
                check("glow_edges","Glowing liquid edges",&s.glowEdges,
                    "Walls running straight into nukage, slime or lava take the glowing strip the game paints on its own edge textures, "
                    "like the NUKE24 steps, instead of only the walls a mapper gave that texture."),
                check("shore_ripples","Shore ripples",&s.shoreRipples,
                    "Liquids ripple where they meet walls, like the splash rings running along the shore: quick thin lines on water, "
                    "slow broad ones on nukage and slime, slow glowing ones on lava, a still dark line on blood."),
                check("liquid_projectile_impacts","Liquid projectile bursts",&s.liquidProjectileImpacts,"Projectiles hitting liquid throw colored spray and wall drips instead of a fireball. Rockets make large plumes; other projectiles make smaller splashes. Requires Splashes.",[](const Settings &d){return d.splashes!=0;}),
                check("splashes","Splashes",&s.splashes,"Ripple rings where things land, wade or are shot in liquids, and spouts where shots and projectiles hit them. "
                    "Things standing in liquids sink in; nukage, slime, blood and lava splash thick and bubble.")}}}},
        {"Sound",{
            {"Music",{
                choice("music_device","Music",&s.musicDevice,
#ifdef __APPLE__
                    {"SoundFont","OPL (FM)","Mac synthesizer"},{0,1,2},
#else
                    {"SoundFont","OPL (FM)"},{0,1},
#endif
                    "SoundFont plays sampled General MIDI instruments, close to the Roland Sound Canvas the music was written on. "
                    "OPL is the FM synthesis of DOS sound cards, with DOOM's own instrument set. "
                    "A .sf2 bank put in the soundfonts folder beside doom.cfg replaces the bundled one.")}},
            {"Effects",{
                choice("sfx_resampling","Sample playback",&s.sfxResampling,{"Original","Smooth","Clean"},{0,1,2},
                    "DOOM's effects are 11 kHz samples. Original holds each sample, raw and gritty, with a bright sizzle on top. "
                    "Smooth blends between samples and keeps some edge. Clean filters the sizzle out entirely."),
                slider("sfx_treble","Made-up treble",&s.sfxTreble,0,2,0.25,
                    "With Smooth or Clean, treble the samples never had is made up from their own top end as they play: "
                    "overtones for tones, finer hiss for shots and blasts, without the metallic ring of held samples. "
                    "At 100% about as bright as Original.",
                    [](const Settings &d){return d.sfxResampling!=0;},
                    {"Off","25%","50%","75%","100%","125%","150%","175%","200%"})}},
            {"Made-up sounds",{
                check("made_impacts","Sparks, dying lights and glass",&s.madeImpacts,
                    "Sounds DOOM never had, made at startup in its own 8-bit style: machinery crackles where it is shot, "
                    "a lamp sputters out or a screen dies in static, and windows break."),
                choice("glass_sound","Breaking glass",&s.glassSound,{"Shatter","Fracture"},{0,1},
                    "Shatter: a hard crack, pieces rattling as the pane comes apart, a rush of splinters and the shards falling. "
                    "Fracture: a crack front branching through the pane, hundreds of tiny cracks ringing the pieces they leave.",
                    [](const Settings &d){return d.madeImpacts!=0;}),
                check("footsteps","Footsteps and splashes",&s.footsteps,
                    "Your steps sound by what the floor is: stone, metal, wood, grass and carpet. In water you splash, "
                    "in nukage, slime, blood and lava it gloops, and monsters wading through liquids splash too. "
                    "Monsters on dry floors stay as silent as ever."),
                check("ambience","Ambience",&s.ambience,
                    "Lamps, door lights and computers hum, nukage and blood bubble, lava rumbles, fire crackles, water drips and wind blows "
                    "under the sky, heard dull through window glass until it is shot, where the map shows them. Only near ones are heard; "
                    "steady lamps barely, flickering ones buzz with their light, and shot-out fixtures go quiet. "
                    "Loud effects push it down for a moment."),
                check("hidden_machinery","Machinery behind walls",&s.hiddenMachinery,"Occasional muffled pipes, vents and machinery during quiet moments."),
                check("adjacent_reverb","Echoes from adjoining halls",&s.adjacentReverb,"Hear larger halls through nearby openings. Requires room acoustics."),
                check("alternative_computer_sounds","Alternative computer sounds",&s.alternativeComputerSounds,
                    "Computers use a narrow machine hum, hard-drive seeks and relay chatter as an alternative to their original bleeps.",
                    [](const Settings &d){return d.ambience!=0;}),
                slider("ambience_amount","Ambience volume",&s.ambienceAmount,0.25,2,0.25,
                    "How loud the ambience is next to the effects.",
                    [](const Settings &d){return d.ambience!=0;},
                    {"25%","50%","75%","100%","125%","150%","175%","200%"})}},
            {"Rooms",{
                check("near_miss_sounds","Near-miss bullet sounds",&s.nearMissSounds,"Distant enemy bullets whizz when they pass close without hitting you."),
                check("room_acoustics","Room acoustics",&s.roomAcoustics,
                    "Sounds echo off the walls around you and ring on in a tail measured from the room: corridors slap back, "
                    "big metal halls boom, wooden rooms stay dull and the outdoors almost dry."),
                slider("reverb_amount","Echo and reverb",&s.reverbAmount,0.25,2,0.25,
                    "How loud the echoes and the tail are next to the sound itself.",
                    [](const Settings &d){return d.roomAcoustics!=0;},
                    {"25%","50%","75%","100%","125%","150%","175%","200%"})}},
            {"Hearing",{
                check("muffle_walls","Muffled behind walls",&s.muffleWalls,
                    "Sounds you can't see come through muffled, the more the longer their way round through the map's openings, "
                    "more through closed doors and window glass, most through solid walls. Walls take their treble, not their loudness."),
                check("muffle_distance","Duller with distance",&s.muffleDistance,
                    "Far sounds lose their treble, the way they do through air."),
                check("natural_falloff","Natural distance falloff",&s.naturalFalloff,
                    "Far sounds fade the way they do in air, losing half their strength each time the distance doubles, "
                    "so a monster across the room sounds across the room. Gunfire and blasts carry farther, faint and dull. "
                    "Off: DOOM's straight fade, still loud at mid range.",hasFalloff)}}}},
        {"Performance",{
            {"Monster visibility",{
                check("ambush_sight_reuse","Ambush sight reuse",&s.ambushSightReuse,
                    "Reduce redundant monster visibility checks without changing hearing, activation or reactions. Works with destructible maps. Disabled during demos and multiplayer.")}},
            {"Sprite preparation",{
                check("sprite_frustum_filter","Conservative sprite filtering",&s.spriteFiltering,
                    "Skips off-camera sprite work only when artwork and shadow bounds are safely outside view. Mirrors, reflections, flashlight silhouettes and uncertain effects keep normal rendering. Monster simulation is unchanged.",[](const Settings &d){return d.accelerated!=0;})}},
            {"Cheaper light work",{
                check("bake_only_lights","Bake-only static lights",&s.bakeOnlyLights,
                    "Torches, lamps and glowing textures stop costing per-frame light work: their flicker, bump detail and gloss come from the bake, "
                    "and lava and nukage pools glow as one light each. Shots, explosions and the flashlight stay dynamic. Changing it re-bakes the level.",
                    [](const Settings &d){return d.bakedLights!=0;}),
                check("grid_sprite_light","Grid lighting for monsters",&s.gridSpriteLight,
                    "Monsters and items take static light, and the light their shadow falls from, from a grid baked at level load "
                    "instead of tracing rays every frame.",
                    [](const Settings &d){return d.bakedLights!=0;}),
                check("unoccluded_surface_lights","Unblocked glowing-texture light",&s.unoccludedSurfaceLights,
                    "Light from lamp strips and glowing flats skips the wall tests per pixel. Cheaper near light panels, "
                    "but their highlights can show on shiny surfaces through walls.",
                    [](const Settings &d){return d.emissive!=0&&d.bakeOnlyLights==0;}),
                check("fewer_surface_lights","Fewer glowing-texture lights",&s.fewerSurfaceLights,
                    "Only the 8 nearest lamp strips and glowing flats light their surroundings per pixel, not 24. Cheaper in rooms "
                    "full of light panels; farther ones keep their baked glow but lose their moving highlights.",
                    [](const Settings &d){return d.emissive!=0&&d.bakeOnlyLights==0;})}}}}};
    // Independent pages use the existing tab navigation, persistence and draft.
    auto allowed=+[](const Settings&)->bool{return !netgame&&!demoplayback&&!demorecording&&!arcade_active;};
    auto& e=s.encounter;
    tabs.push_back({"ENCOUNTER DIRECTOR",{{"Single-player encounter controls",{
        check("encounter_director_enabled","Enable Encounter Director",&e.enabled,"Master control for all four modules. Default off. Active streaming drains pending originals at its bounded tic budget when disabled; live originals are never deleted. Network games/demos bypass.",allowed),
        check("encounter_randomize_monsters","Initial Monster Randomization",&e.monsters,"One-for-one original monsters at map start. Enable before starting/restarting a map. Population and protected encounters are preserved.",allowed),
        check("encounter_reinforcements_enabled","Dynamic Reinforcements",&e.reinforcements,"Adds bounded, out-of-sight monsters at original map spawn locations. May be enabled during play. This adds monsters; it does not stream or reduce the original population.",allowed),
        check("encounter_randomize_items","Item Randomization",&e.items,"One-for-one original pickups at map start, using independent category pools. Keys and unknown progression items remain intact.",allowed),
        check("","Original Monster Streaming",&e.stream,"Fourth independent module. Configure on ED Population. Apply before restarting the map; remote dormant monsters cannot wake until activated.",allowed)
    }}}});
    tabs.push_back({"ED Monsters",{{"MONSTER RANDOMIZATION - next map",{
        check("","Enable Monster Randomization",&e.monsters,"Also requires the master Encounter Director switch. Applies once before the first simulation tic of a new map.",allowed),
        slider("encounter_randomization_strength","Randomization Strength (%)",&e.strength,0,100,1,"Probability and threat breadth of eligible substitutions; never changes original monster count.",allowed),
        choice("encounter_preserve_bosses","Preserve Bosses",&e.bossProtection,{"Off","Bosses","Strict"},{0,1,2},"Strict also preserves minibosses. Progression/boss-death actors, unique and unrecognized scripted actors are always protected.",allowed),
        check("encounter_threat_weighted","Threat-Weighted Selection",&e.weighted,"Prefer similar threat costs. Flying/ground and kill-count compatibility restrictions always apply.",allowed)
    }} ,{"Original population and progression are always protected",{}}}});
    tabs.push_back({"ED Reinforcements",{{"DYNAMIC REINFORCEMENTS",{
        check("","Enable Reinforcements",&e.reinforcements,"Requires Encounter Director. Limits apply to additional monsters, not original map monsters.",allowed),
        slider("encounter_min_spawn_distance","Minimum Spawn Distance",&e.minDistance,64,8191,1,"World units from every living player. Applied maximum is always greater than minimum.",allowed),
        slider("encounter_max_spawn_distance","Maximum Spawn Distance",&e.maxDistance,65,8192,1,"World units from every living player. Cheap distance rejection precedes line-of-sight work.",allowed),
        slider("encounter_min_group_size","Minimum Group Size",&e.minGroup,1,16,1,"Desired group minimum; blocked locations, adaptive pacing and hard budgets may produce a smaller group.",allowed),
        slider("encounter_max_group_size","Maximum Group Size",&e.maxGroup,1,16,1,"Hard upper bound per reinforcement event.",allowed),
        slider("encounter_min_spawn_interval","Minimum Interval (seconds)",&e.minInterval,5,600,1,"Simulation time between events; pauses and menus do not advance it.",allowed),
        slider("encounter_max_spawn_interval","Maximum Interval (seconds)",&e.maxInterval,5,600,1,"Next interval is independently selected with the director random stream.",allowed),
        slider("encounter_max_active_additional","Maximum Active Additional",&e.maxActive,1,64,1,"Living director monsters allowed before more may spawn. Original monsters are never removed.",allowed),
        slider("encounter_max_total_additional","Maximum Additional per Map",&e.maxTotal,1,256,1,"Lifetime number issued this map. Killing monsters or toggling the director never resets this budget.",allowed),
        slider("encounter_max_threat","Maximum Additional Threat",&e.maxThreat,1,256,1,"Threat budget for living additional monsters; heavy monsters cost more than Imps.",allowed),
        check("encounter_no_spawns_behind","No Spawns Behind Player",&e.noBehind,"Uses movement direction above one unit/tic, otherwise facing. A candidate must be in the forward half-plane.",allowed),
        check("encounter_adaptive_pacing","Adaptive Reinforcement Pacing",&e.adaptive,"Low health/armor increases delays and reduces group/threat budgets; severe distress pauses reinforcements.",allowed),
        check("encounter_pause_for_boss","Pause During Boss Encounters",&e.pauseBoss,"Suspend additions around a living boss with an active target.",allowed)
    }},{"Never spawn in player LOS - mandatory",{}}}});
    tabs.push_back({"ED Items",{{"ITEM RANDOMIZER - next map",{
        check("","Enable Item Randomization",&e.items,"Requires Encounter Director and independently selected categories. Original item count is preserved.",allowed),
        check("encounter_randomize_ammo","Randomize Ammo",&e.ammo,"Separate ammo-family pools preserve weapon/ammo compatibility.",allowed),
        check("encounter_randomize_armor","Randomize Armor",&e.armor,"Replace eligible armor only with available armor classes.",allowed),
        check("encounter_randomize_health","Randomize Health",&e.health,"Replace eligible health pickups only within the health pool.",allowed),
        check("encounter_randomize_powerups","Randomize Powerups",&e.powerups,"Compatible powerups only. Suits, invulnerability, berserk and unknown objective effects remain protected.",allowed),
        check("encounter_randomize_weapons","Randomize Weapons",&e.weapons,"Conservative ammo/progression tiers; retain at least one of each original weapon class.",allowed),
        check("encounter_randomize_custom_inventory","Randomize Custom Inventory",&e.customInventory,"Custom inventory randomization may produce unpredictable results with gameplay mods. Only compatible and explicitly safe inventory classes will be randomized. Unknown classes stay unchanged.",allowed),
        check("encounter_exclude_counted_items","Exclude Counted Items",&e.excludeCounted,"Protect MF_COUNTITEM pickups from substitutions by default.",allowed),
        slider("encounter_item_coverage","Item Randomization Coverage (%)",&e.coverage,0,100,1,"Percentage of eligible original placements considered. No items are added or deleted.",allowed)
    }},{"Keys, quest items and progression are always protected",{}}}});
    auto packs=+[](const Settings& d)->bool{return !netgame&&!demoplayback&&!demorecording&&!arcade_active&&d.encounter.streamPolicy!=0;};
    auto unavailable=+[](const Settings&)->bool{return false;};
    auto initialCount=+[](const Settings& d)->bool{return !netgame&&!demoplayback&&!demorecording&&!arcade_active&&d.encounter.initialBudgetMode==0;};
    auto initialPercent=+[](const Settings& d)->bool{return !netgame&&!demoplayback&&!demorecording&&!arcade_active&&d.encounter.initialBudgetMode==1;};
    tabs.push_back({"ED Population",{{"ORIGINAL MONSTERS - next map/restart",{
        check("encounter_stream_original_monsters","Original Monster Streaming",&e.stream,"Requires Director. Default off. Performance changes encounters: dormant originals cannot hear shots or fight. Apply budget and safety mode before map restart. Master off uses a bounded drain.",allowed),
        choice("encounter_stream_initial_budget_mode","Initial Budget Type",&e.initialBudgetMode,{"Count","Percent"},{0,1},"Next map/restart. Count filtered original monsters, not THINGS, replacements or reinforcements.",allowed),
        slider("encounter_stream_initial_count","Initial Monster Count",&e.initialMonsterCount,0,1000000,100,"Desired initial originals. Clamp to filtered map population. Mandatory originals may exceed this target; nearest authored locations are selected first.",initialCount),
        slider("encounter_stream_initial_percent","Initial Monster Percentage",&e.initialMonsterPercent,1,100,.5,"Next map: percentage of filtered original monsters, rounded up. Half-percent precision; 12.5% of 4317 is 540, not 500.",initialPercent),
        choice("encounter_stream_population_policy","Population Safety Mode",&e.populationPolicy,{"Compatibility","Performance"},{0,1},"Next map. Compatibility exceeds the target without an occlusion proof. Performance stages ordinary visible monsters too: temporary absence/pop-in is possible. Never freezes gameplay.",allowed),
        choice("encounter_stream_policy","Activation Policy",&e.streamPolicy,{"Proximity / Progress","Timed Packs","Hybrid"},{0,1,2},"Next map. Proximity responds to progress/deaths; Timed responds to its clock; Hybrid uses both. Performance never expands the budget just because a monster might be visible.",allowed),
        slider("encounter_stream_activate_distance","Activation Distance",&e.activateDistance,128,8190,1,"Priority range for ordinary candidates; Performance still obeys packs. Compatibility loads uncertain visible regions eagerly at map start.",allowed),
        slider("encounter_stream_prefetch_distance","Prefetch Distance",&e.prefetchDistance,129,8191,1,"Query nearby spatial bins and queue authored locations before approaching doors/corners.",allowed),
        slider("encounter_stream_max_spawns_per_tic","Normal Spawn Budget",&e.spawnBudget,1,64,1,"Default 8 attempts per tic, including master-off drain. Initial map construction is separate; Performance never adds render-driven spawns.",allowed),
        slider("encounter_stream_pack_size","Spawn Pack Size",&e.packSize,1,64,1,"Originals per pack; also bounded by attempts/tic. Proximity uses progress/deaths; Timed uses the interval; Hybrid uses both.",allowed),
        slider("encounter_stream_pack_interval","Pack Interval (seconds)",&e.packInterval,.5,60,.5,"Only nearby/visited/linked encounter regions enter timed packs. Credits do not accumulate into unlimited catch-up waves.",packs),
        slider("encounter_stream_soft_active_limit","Soft Active Target",&e.softLimit,1,8192,1,"Post-start routine packs pause at max(this value, actual initial live originals). Deaths free slots; progress can override. Never removes a living original.",allowed),
        check("encounter_stream_safe_stasis","Safe Distant Stasis (unavailable)",&e.safeStasis,"Not implemented: no live monster AI, physics or combat is frozen.",unavailable),
        slider("encounter_stream_stasis_distance","Stasis Distance (reserved)",&e.stasisDistance,130,8192,1,"Reserved for a separately validated safe stasis implementation.",unavailable)
    }}}});
    std::vector<Option> advanced={
        slider("encounter_random_seed","Custom Random Seed",&e.seed,0,16777215,1,"0 uses an automatic session/map seed; positive integers reproduce selection on the same map/settings. Applies next map.",allowed),
        slider("encounter_max_local_threat","Maximum Local Encounter Threat",&e.maxLocalThreat,1,512,1,"Nearby existing plus additional monster pressure blocks new groups when already crowded.",allowed),
        slider("encounter_max_spawn_attempts","Maximum Attempts per Event",&e.maxAttempts,1,64,1,"Bounded candidate attempts. Failure skips the event until the next normal interval.",allowed)
    };
    advanced.push_back(check("encounter_debug","Display Director Debug Information",&e.debug,"Aggregate population/pack statistics in the game log once per second; no per-actor logging.",allowed));
    static const char* threatKeys[]={"encounter_threat_zombieman","encounter_threat_shotgun_guy","encounter_threat_chaingunner","encounter_threat_imp","encounter_threat_pinky","encounter_threat_spectre","encounter_threat_lost_soul","encounter_threat_cacodemon","encounter_threat_hell_knight","encounter_threat_baron","encounter_threat_mancubus","encounter_threat_arachnotron","encounter_threat_revenant","encounter_threat_archvile","encounter_threat_pain_elemental","encounter_threat_cyberdemon","encounter_threat_spider"};
    static const char* threatNames[]={"Zombieman threat","Shotgun guy threat","Chaingunner threat","Imp threat","Pinky threat","Spectre threat","Lost Soul threat","Cacodemon threat","Hell Knight threat","Baron threat","Mancubus threat","Arachnotron threat","Revenant threat","Arch-vile threat","Pain Elemental threat","Cyberdemon threat","Spider Mastermind threat"};
    std::vector<Option> threats;
    for(int i=0;i<17;++i)threats.push_back(slider(threatKeys[i],threatNames[i],&e.threatCosts[i],1,128,1,"Director classification/budget cost only. Does not change the monster's damage, speed, health or behavior.",allowed));
    tabs.push_back({"ED Advanced",{{"ADVANCED",std::move(advanced)},{"Threat tuning",std::move(threats)}}});
    for(auto &tab:tabs)for(auto &section:tab.sections)for(auto &option:section.options) {
        std::string key=option.key;
        if(key=="fluid_firing_smoke"||key=="fluid_reload_smoke"||key=="fluid_hot_smoke"||key=="fluid_indoor_smoke"||key=="fluid_saw_smoke"||key=="fluid_saw_blood"||key=="fluid_idle_gestures"||key=="fluid_rocket_fire"||key=="fluid_bare_hands"||key=="fluid_fast_pistol"||key=="fluid_gloves")
            option.enabled=[](const Settings &d){return d.fluidWeapons!=0;};
        if(key.find("fluid_glove_")==0||key.find("fluid_shade_")==0)
            option.enabled=[](const Settings &d){return d.fluidWeapons&&d.fluidGloves==7;};
        if(key=="through_walls"||key=="breach_doors")
            option.enabled=[](const Settings &d){return d.breakableWalls!=0;};
        if(key=="progressive_glass"||key=="glass_reflections")
            option.enabled=[](const Settings &d){return d.windowGlass&&hasGlass(d);};
    }
    // Keep combat controls separate from the large destruction section.
    OptionTab combat{"Combat",{}};
    for(auto &tab:tabs)if(std::string(tab.title)=="Effects") {
        tab.title="Destruction";
        for(auto it=tab.sections.begin();it!=tab.sections.end();) {
            if(std::string(it->title)=="Combat") {combat.sections.push_back(std::move(*it));it=tab.sections.erase(it);}
            else ++it;
        }
    }
    if(!combat.sections.empty())tabs.insert(tabs.begin()+3,std::move(combat));
    auto key=[&](const char *name,const char *title,int *value){
        Option option{};option.kind=OptionKind::Key;option.key=name;option.title=title;option.flag=value;
        option.tip="Press Enter, then a key. Escape cancels; Delete clears. Choose an unused key. Q, F and function keys are reserved. Hold to peek; release to return to center.";
        option.enabled=[](const Settings &d){return d.accelerated&&d.cornerPeek;};return option;
    };
    tabs.push_back({"Controls",{{"Corner peeking",{
        check("corner_peek","Corner peeking",&s.cornerPeek,"Hold the left/right peek keys to lean around a corner with a gentle 12-degree view tilt; release to return. Walls stop the camera. Shots originate from the collision-checked leaning view, keeping the crosshair centered. Disabled in demos and multiplayer.",[](const Settings &d){return d.accelerated!=0;}),
        key("peek_left","Peek left",&s.peekLeft),key("peek_right","Peek right",&s.peekRight)}}}});
    tabs.push_back({"SPEEDRUN",{{"Session overlay",{
        check("speedrun_hud","Enable Speedrun HUD",&s.speedrunHUD,"Optional session statistics. Counters continue while hidden; saves do not roll back session history.",[](const Settings &d){return d.accelerated!=0;}),
        check("speedrun_only","Speedrun HUD only",&s.speedrunOnly,"Hide the standard HUD, crosshair and gameplay notices. Menus, the automap, and the weapon wheel remain available.",[](const Settings &d){return d.accelerated&&d.speedrunHUD;}),
        slider("speedrun_background_opacity","Background opacity",&s.speedrunBackgroundOpacity,0,100,5,
            "Speedrun panel background only. 0% shows the text without a dark box; 100% is fully opaque.",[](const Settings &d){return d.accelerated&&d.speedrunHUD;}),
        choice("speedrun_layout","Info layout",&s.speedrunLayout,{"Column","Row","Split Columns"},{0,1,2},"Row wraps when needed. Split Columns puts map and run stats on the left and player stats on the right.",[](const Settings &d){return d.accelerated&&d.speedrunHUD;}),
        slider("speedrun_x","X adjustment",&s.speedrunX,-100,100,1,"Moves the panel horizontally. In Split Columns this moves the map and run column.",[](const Settings &d){return d.accelerated&&d.speedrunHUD;}),
        slider("speedrun_y","Y adjustment",&s.speedrunY,-100,100,1,"Moves the panel vertically. In Split Columns this moves the map and run column.",[](const Settings &d){return d.accelerated&&d.speedrunHUD;}),
        choice("speedrun_anchor","Overlay position",&s.speedrunAnchor,{"Upper left","Upper right","Lower left","Lower right"},{0,1,2,3},"Sets the panel's anchor. In Split Columns, the map column uses only the upper or lower position on the left.",[](const Settings &d){return d.accelerated&&d.speedrunHUD;}),
        choice("speedrun_player_anchor","Player column position",&s.speedrunPlayerAnchor,{"Upper right","Lower right"},{1,3},"Sets the independent anchor for the right column.",[](const Settings &d){return d.accelerated&&d.speedrunHUD&&d.speedrunLayout==2;}),
        slider("speedrun_player_x","Player X adjustment",&s.speedrunPlayerX,-100,100,1,"Moves the player column horizontally on the right half of the screen.",[](const Settings &d){return d.accelerated&&d.speedrunHUD&&d.speedrunLayout==2;}),
        slider("speedrun_player_y","Player Y adjustment",&s.speedrunPlayerY,-100,100,1,"Moves the player column vertically without moving the map column.",[](const Settings &d){return d.accelerated&&d.speedrunHUD&&d.speedrunLayout==2;}),
        check("speedrun_map_name","Map name",&s.speedrunMapName,"Shows the current map number.",[](const Settings &d){return d.speedrunHUD!=0;}),
        check("speedrun_attempts","Map Attempts",&s.speedrunAttempts,"Each playable map load/restart/checkpoint begins one attempt.",[](const Settings &d){return d.speedrunHUD!=0;}),
        check("speedrun_deaths","Map Deaths",&s.speedrunDeaths,"Actual player deaths this map visit.",[](const Settings &d){return d.speedrunHUD!=0;}),
        check("speedrun_killstreak","Killstreak",&s.speedrunStreak,"Player-credited monster kills this attempt, including owned damage-over-time kills.",[](const Settings &d){return d.speedrunHUD!=0;}),
        check("speedrun_attempt_time","Current Attempt Time",&s.speedrunAttemptTime,"Simulation gameplay time; freezes at player death until a new attempt.",[](const Settings &d){return d.speedrunHUD!=0;}),
        check("speedrun_runtime","Gameplay Runtime",&s.speedrunRuntime,"Session simulation time, including active world ticks while dead; excludes pause, stopped menus, loading and intermission.",[](const Settings &d){return d.speedrunHUD!=0;}),
        check("speedrun_damage_dealt","Total Damage Dealt",&s.speedrunDealt,"Session monster HP actually removed by player-owned attacks. Excludes overkill and duplicate notifications.",[](const Settings &d){return d.speedrunHUD!=0;}),
        check("speedrun_damage_taken","Total Damage Taken",&s.speedrunTaken,"Session player HP lost after armor and protection.",[](const Settings &d){return d.speedrunHUD!=0;}),
        check("speedrun_secrets","Secrets",&s.speedrunSecrets,"Secrets found out of the map total.",[](const Settings &d){return d.speedrunHUD!=0;}),
        check("speedrun_items","Items",&s.speedrunItems,"Items collected out of the map total.",[](const Settings &d){return d.speedrunHUD!=0;}),
        check("speedrun_health","Health",&s.speedrunHealth,"Current player health.",[](const Settings &d){return d.speedrunHUD!=0;}),
        check("speedrun_armor","Armor",&s.speedrunArmor,"Current player armor.",[](const Settings &d){return d.speedrunHUD!=0;}),
        check("speedrun_ammo","Ammo",&s.speedrunAmmo,"Current and maximum ammo for the equipped weapon.",[](const Settings &d){return d.speedrunHUD!=0;})
    }}}});
    return tabs;
}

namespace {
template<class Visit> void eachOption(Settings &s,Visit visit) {
    for(OptionTab &tab:optionTabs(s))for(OptionSection &section:tab.sections)for(Option &option:section.options)if(option.key&&*option.key&&option.kind!=OptionKind::Action)visit(option);
}
}
// graphics.cfg from before the combined Combat choices: the old switches' values, until finishReading
// turns them into the choices a file without those keys still lacks.
static double oldDeath[4]={1,1,0,0},oldHit[2]={1,1},oldBlood[3]={1,1,1};   // varied, falls, ragdoll, blast; motion, flinch; blood, spray, shine
static bool readOld[3],readNew[3];
void readOption(Settings &s,const char *key,double value) {
    if(P_WeakpointReadBalance(key,value))return;
    static const char *const olds[][4]={{"varied_deaths","death_falls","death_ragdoll","death_blast"},{"death_motion","hit_flinch"},{"blood","blood_spray","blood_shine"}};
    double *into[]={oldDeath,oldHit,oldBlood};
    static const char *const news[]={"deaths","hit_reactions","blood_level"};
    for(int g=0;g<3;++g) {
        for(int k=0;k<4&&olds[g][k];++k)if(!strcmp(key,olds[g][k])) {into[g][k]=value;readOld[g]=true;}
        if(!strcmp(key,news[g]))readNew[g]=true;
    }
    eachOption(s,[&](Option &option) {
        if(strcmp(option.key,key))return;
        if(option.kind==OptionKind::Check)*option.flag=value!=0;
        else if(option.kind==OptionKind::Key){if(value==0||(value==std::floor(value)&&value>0&&value<SDL_SCANCODE_COUNT&&validPeekBinding((int)value)))*option.flag=(int)value;}
        else if(option.kind==OptionKind::Choice) {for(int v:option.values)if(v==value)*option.flag=v;}
        else if(!strcmp(option.key,"encounter_stream_initial_count"))*option.number=(float)std::clamp(std::round(value),1.,1000000.);
        else *option.number=option.snap(value);
    });
}
void finishReading(Settings &s) {
    ED_ValidateConfig(&s.encounter);
    if(readOld[0]&&!readNew[0])s.deaths=!oldDeath[0]?0:oldDeath[2]?(oldDeath[3]?4:3):oldDeath[1]?2:1;
    if(readOld[1]&&!readNew[1])s.hitReactions=oldHit[0]||oldHit[1];
    if(readOld[2]&&!readNew[2])s.bloodLevel=!oldBlood[0]?0:oldBlood[1]||oldBlood[2]?2:1;
    for(int g=0;g<3;++g)readOld[g]=readNew[g]=false;
}
void writeOptions(FILE *file,Settings &s) {
    P_WeakpointWriteBalance(file);
    eachOption(s,[&](Option &option) {
        if(option.kind==OptionKind::Slider)fprintf(file,!strcmp(option.key,"encounter_random_seed")?"%s %.9g\n":"%s %g\n",option.key,*option.number);
        else fprintf(file,"%s %d\n",option.key,*option.flag);
    });
}
}


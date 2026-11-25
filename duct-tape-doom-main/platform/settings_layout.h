#ifndef DOOM_SETTINGS_LAYOUT_H
#define DOOM_SETTINGS_LAYOUT_H
#include "settings_panel.h"
#include <initializer_list>
#include <utility>
namespace doom3d {
inline bool settingIs(const char *key,std::initializer_list<const char*> keys) {
    for(const char *candidate:keys)if(!strcmp(key,candidate))return true;
    return false;
}
// Only placement changes here. The original Option keeps its Settings pointer,
// dependency predicate, valid values, help and persistence key.
inline std::pair<const char*,const char*> settingLocation(const Option &o,const char *tab,const char *section) {
    const char *k=o.key?o.key:"";
    if(!strcmp(tab,"Game"))return {"Game","Maps"};
    if(!strcmp(tab,"Multiplayer"))return {"Multiplayer","LAN"};
    if(settingIs(k,{"quick_confirm","death_load_last_save"}))return {"Game","Saving"};
    if(settingIs(k,{"look","invert_look","classic_aim","crosshair"}))return {"Controls","Aiming"};
    if(!strcmp(tab,"Controls"))return {"Controls","Peeking"};
    if(settingIs(k,{"hud_face","helmet_face"}))return {"HUD","Portrait"};
    if(settingIs(k,{"combat_notices","combat_event_messages","key_pickup_hands"}))return {"HUD","Feedback"};
    if(!strncmp(k,"hud_",4))return {"HUD","Layout"};
    if(!strcmp(tab,"SPEEDRUN")) {
        if(settingIs(k,{"speedrun_hud","speedrun_only","speedrun_background_opacity","speedrun_layout","speedrun_x","speedrun_y","speedrun_anchor","speedrun_player_anchor","speedrun_player_x","speedrun_player_y"}))return {"HUD","Speedrun"};
        return {"HUD","Run stats"};
    }
    if(settingIs(k,{"fps","swarm_debug"}))return {"Performance","Diagnostics"};
    if(settingIs(k,{"monster_hibernation","ambush_sight_reuse"}))return {"Performance","Monsters"};
    if(!strcmp(k,"sprite_frustum_filter"))return {"Performance","Visibility"};
    if(!strcmp(tab,"Performance"))return {"Performance","Lights"};
    if(!strcmp(tab,"Display"))return {"Display",!strcmp(section,"Retro look")?"Filters":settingIs(k,{"fov","widescreen"})?"View":"Video"};
    if(settingIs(k,{"monster_recovery","flashlight_monster_awareness"})||!strcmp(tab,"Monster AI"))return {"Monsters","AI"};
    if(settingIs(k,{"monster_feeding","headshot_frenzy"}))return {"Monsters","Survival"};
    if(!strcmp(k,"glowing_eyes"))return {"Monsters","Appearance"};
    if(!strcmp(tab,"Gameplay"))return {"Gameplay",!strcmp(section,"Balance")?"Balance":settingIs(k,{"key_arena","arena_radius"})?"Arenas":"Weapons"};
    if(!strcmp(tab,"ENCOUNTER DIRECTOR"))return {"ENCOUNTER DIRECTOR","Overview"};
    if(!strncmp(tab,"ED ",3))return {"ENCOUNTER DIRECTOR",tab+3};
    if(settingIs(k,{"outdoor_wind","outdoor_wind_density"}))return {"Atmosphere","Particles"};
    if(!strcmp(tab,"Environment"))return {"Destruction",!strcmp(section,"Blasts and rubble")?"Debris":"Effects"};
    if(!strcmp(tab,"Effects")||!strcmp(tab,"Destruction")||!strcmp(tab,"Combat")) {
        if(!strcmp(section,"Destruction and doors"))return {"Destruction",settingIs(k,{"door_blowout","key_door_shield","switch_door_shield","breach_doors","door_sparks"})?"Doors":"Surfaces"};
        if(!strcmp(section,"Collapse gameplay"))return {"Destruction","Collapses"};
        if(!strcmp(k,"material_rubble"))return {"Destruction","Debris"};
        if(settingIs(k,{"corrosive_barrels","acid_splatters","acid_monster_coating","incendiary_barrels"}))return {"Combat","Barrels"};
        if(settingIs(k,{"critical_parts","enhanced_weak_points","monster_weapon_vulnerabilities","kill_shots_carry"}))return {"Combat","Hits"};
        if(settingIs(k,{"localized_injury_effects","elemental_damage_over_time","advanced_monster_reactions","eye_injuries","visible_eye_injuries","monster_bleeding","hit_flinch"}))return {"Combat","Injuries"};
        if(settingIs(k,{"gore_level","gore_heads","gore_fragments","gore_wall_blood","gore_body_damage"}))return {"Combat","Gore"};
        if(settingIs(k,{"blood","blood_spray","blood_shine","severe_critical_bloodspill","monster_skin_damage","wounds"}))return {"Combat","Blood"};
        if(settingIs(k,{"varied_deaths","death_falls","death_face_viewer","death_motion","body_motion_strength","soft_effects"}))return {"Combat","Deaths"};
        if(!strcmp(k,"shootable_lights"))return {"Destruction","Lights"};
        return {"Combat","Feedback"};
    }
    if(!strcmp(tab,"Bundled mods"))return {"Mods",settingIs(k,{"voxel_models","voxel_eyes"})?"Voxels":"Enable"};
    if(!strcmp(tab,"Fluid Weapons"))return {"Mods",settingIs(k,{"fluid_bare_hands","fluid_fast_pistol"})||!strcmp(section,"Rocket launcher")?"Weapons":!strcmp(section,"Gloves")?"Gloves":"Smoke"};
    if(!strcmp(k,"weapon_lighting"))return {"Lighting","Flashlight"};
    if(!strcmp(k,"emissive"))return {"Lighting","Live"};
    if(!strcmp(tab,"Textures"))return {"World detail",!strcmp(section,"Filtering")?"Filtering":!strcmp(section,"Selected Texture Immersion")?"Living":!strcmp(section,"Glow")?"Screens":settingIs(k,{"window_glass","progressive_glass","glass_reflections"})?"Glass":"Surfaces"};
    if(!strcmp(tab,"Lighting")) {
        if(settingIs(k,{"window_light","window_strength"}))return {"Lighting","Windows"};
        if(settingIs(k,{"sun_shadows","sky_light","bounce_light","bounce_tint","baked_occlusion"}))return {"Lighting","Sun / sky"};
        return {"Lighting",!strcmp(section,"Flashlight and shadows")?"Flashlight":!strcmp(section,"Live light")?"Live":"Baked"};
    }
    if(!strcmp(tab,"Atmosphere")) {
        if(!strcmp(section,"Liquids"))return {"World detail",!strncmp(k,"caustics_",9)||!strcmp(k,"ceiling_caustics")?"Caustics":"Liquids"};
        if(!strcmp(k,"storm_volume"))return {"Audio","World"};
        if(settingIs(k,{"fog","deimos_fog"}))return {"Atmosphere","Fog"};
        if(settingIs(k,{"dust_motes","embers","heat_haze","episode_particles"}))return {"Atmosphere","Particles"};
        if(settingIs(k,{"authored_episodes","episode_atmosphere","episode_lightning","inferno_tree_strikes"}))return {"Atmosphere","Episodes"};
        if(!strcmp(section,"Decoration Immersion"))return {"Atmosphere","Decorations"};
        if(settingIs(k,{"worn_plates","window_streaks"}))return {"World detail","Surfaces"};
        return {"Atmosphere",!strcmp(section,"Air")?"Sky":"Weather"};
    }
    if(!strcmp(tab,"Sound"))return {"Audio",!strcmp(section,"Music")||!strcmp(section,"Effects")?"Output":!strcmp(section,"Made-up sounds")?"World":!strcmp(k,"near_miss_sounds")?"Weapons":"Rooms"};
    return {tab,section}; // Future options remain reachable without a key whitelist.
}
inline std::vector<OptionTab> settingsLayout(std::vector<OptionTab> source) {
    std::vector<OptionTab> result={
        {"Game",{},{{"Maps",{}},{"Saving",{}}}},
        {"Multiplayer",{},{{"LAN",{}}}},
        {"Display",{},{{"Video",{}},{"View",{}},{"Filters",{}}}},
        {"Controls",{},{{"Aiming",{}},{"Peeking",{}}}},
        {"HUD",{},{{"Layout",{}},{"Portrait",{}},{"Feedback",{}},{"Speedrun",{}},{"Run stats",{}}}},
        {"Gameplay",{},{{"Weapons",{}},{"Balance",{}},{"Arenas",{}}}},
        {"Combat",{},{{"Hits",{}},{"Injuries",{}},{"Blood",{}},{"Gore",{}},{"Deaths",{}},{"Feedback",{}},{"Barrels",{}}}},
        {"Monsters",{},{{"AI",{}},{"Survival",{}},{"Appearance",{}}}},
        {"ENCOUNTER DIRECTOR",{},{{"Overview",{}},{"Monsters",{}},{"Reinforcements",{}},{"Items",{}},{"Population",{}},{"Advanced",{}}}},
        {"Destruction",{},{{"Surfaces",{}},{"Doors",{}},{"Collapses",{}},{"Debris",{}},{"Lights",{}},{"Effects",{}}}},
        {"World detail",{},{{"Filtering",{}},{"Surfaces",{}},{"Glass",{}},{"Living",{}},{"Screens",{}},{"Liquids",{}},{"Caustics",{}}}},
        {"Lighting",{},{{"Baked",{}},{"Windows",{}},{"Sun / sky",{}},{"Live",{}},{"Flashlight",{}}}},
        {"Atmosphere",{},{{"Sky",{}},{"Fog",{}},{"Weather",{}},{"Particles",{}},{"Episodes",{}},{"Decorations",{}}}},
        {"Audio",{},{{"Output",{}},{"Weapons",{}},{"World",{}},{"Rooms",{}}}},
        {"Mods",{},{{"Enable",{}},{"Voxels",{}},{"Smoke",{}},{"Weapons",{}},{"Gloves",{}}}},
        {"Performance",{},{{"Lights",{}},{"Monsters",{}},{"Visibility",{}},{"Diagnostics",{}}}}
    };
    for(auto &tab:source)for(auto &section:tab.sections)for(auto &option:section.options) {
        auto location=settingLocation(option,tab.title,section.title);
        auto category=std::find_if(result.begin(),result.end(),[&](const OptionTab &t){return !strcmp(t.title,location.first);});
        if(category==result.end()){result.push_back({location.first,{},{}});category=result.end()-1;}
        auto &pages=category->pages;
        auto page=std::find_if(pages.begin(),pages.end(),[&](const OptionTab &t){return !strcmp(t.title,location.second);});
        if(page==pages.end()){pages.push_back({location.second,{}});page=pages.end()-1;}
        const char *heading=!strcmp(section.title,"Combat")?page->title:section.title;
        if(page->sections.empty()||strcmp(page->sections.back().title,heading))page->sections.push_back({heading,{}});
        page->sections.back().options.push_back(std::move(option));
    }
    for(auto &tab:result)tab.pages.erase(std::remove_if(tab.pages.begin(),tab.pages.end(),[](const OptionTab &p){return p.sections.empty();}),tab.pages.end());
    result.erase(std::remove_if(result.begin(),result.end(),[](const OptionTab &t){return t.pages.empty();}),result.end());
    return result;
}
}
#endif

/* Weather: rain under the sky (settings.weather). Whether a map rains is
   authored, not derived: the episode's mood says so (levelMood, Game_Mood),
   or the player asks for rain everywhere. It pours from the start to the
   end of the map, driven slant by a wind of its own, apart from the sun:
   the way the episode's mood says (rainAngle), or the level picks it.
   Drawn as streaks of whole Doom pixels stepping at the tic rate, like the
   debris, falling from the sky ceiling and splashing where they land; on
   liquids they ring. Through a broken window the wind blows it in, and the
   floor and wall under the window soak (wear.cpp draws them wet). The ground
   under the sky lies wet in steps baked once (bakeWetness), the same all
   map long. The glass draws its drops itself (glass.frag), the ground its
   wetness (wear.glsl), the ambience its sound. Render-only, with its own
   random numbers. */
#include "internal.h"
#include "storm_audio.h"
#include "../storm_atmosphere.h"
#include "../episode_atmosphere.h"
#include "../lightning_interactions.h"
extern "C" {
#include "../audio/sound_options.h"
}

namespace doom3d {
// E2.2: soft Fluid smoke masks are appended to the existing textured-effect
// batches after buildImpacts() has cleared them. No weapon particles,
// gameplay RNG, or new image keys are involved. Implemented in
// fluid_effects.inc; only called for SKY2 / Burning City.
void fluidWeatherSmoke(const Uniforms &camera,float x,float y,float z,
    float radiusX,float radiusZ,const std::array<float,3> &color,
    float alpha,float rotation,int mask);

float rainNow=0,wetNow=0,glassWetNow=0;
float stormFlashNow=0,stormBoltNow=0,stormBoltAzimuthNow=0,stormBoltSeedNow=0;
float stormCurtainNow=0,stormHazeNow=0;
int episodeAtmosphereNow=0; // doom_episode::Profile; shared by geometry and sky.
float deimosBlizzardNow=0,deimosGustNow=0; // D2.1: cosmetic-only whiteout progression.
float starportDustNow=0,starportGustNow=0; // E1: dry sandstorm intensity and gusts.
float citySmokeNow=0,bloodHellMistNow=0; // E2/E3: themed haze, without water rain.
float rainWind[2]={0,0};
namespace {
unsigned weatherLevelSerial=~0u,soakPaneSerial=~0u;
uint32_t weatherSeed=0;
doom_storm::State stormState;
// Only coordinates survive an event: never retain a mobj_t* across tics.
struct TreeStrike {
    float x=0,y=0,ground=0,top=0;
    int born=-1,sector=-1,type=-1,sprite=-1;
    std::uint32_t seed=0;
};
struct BurningTree {
    float x=0,y=0,ground=0,height=0;
    int born=-1,sector=-1,type=-1,sprite=-1;
    std::uint32_t seed=0;
};
struct CharredTree {
    float x=0,y=0;
    int sector=-1,type=-1,sprite=-1;
};
// Bounded per-map history: prefer never-hit trees, then those struck least
// recently. This distributes strikes across TRE1 and TRE2 instead of repeatedly
// favoring whichever tree is nearest the player. No retained actor pointers.
struct TreeStrikeHistory {
    float x=0,y=0;
    int sector=-1,type=-1,sprite=-1;
    unsigned hits=0,order=0;
};
TreeStrike treeStrike;
std::array<BurningTree,doom_strike::maxBurningTrees> treeFires{};
std::array<CharredTree,doom_strike::maxCharredTrees> charredTrees{};
int charredCount=0;
std::array<TreeStrikeHistory,doom_strike::maxStrikeHistory> treeHistory{};
int historyCount=0;
unsigned strikeOrder=0;
doom_strike::CloseThunder treeCloseThunder{};
float windSpeed=0;
int weatherTic=-1;
std::vector<float> soak;
bool rainy() {return settings.weather==2||(settings.weather==1&&levelMood().rain);}
// Conservative authored gate: use the existing game-profile authority rather
// than guessing from sky color or assigning a custom PWAD the classic E3 look.
doom_episode::Profile episodeProfile() {
    // SKY1/2/3 are logical wall-texture names, normally backed by RSKY1/2/3.
    // Map numbers alone are not sufficient: PWADs and sky-transfer effects
    // can select different textures. Never guess a profile from RGB colors.
    const int sky=skytexture;
    const int skyKind=sky>=0&&sky==R_CheckTextureNumForName("SKY1")?1:
        sky>=0&&sky==R_CheckTextureNumForName("SKY2")?2:
        sky>=0&&sky==R_CheckTextureNumForName("SKY3")?3:0;
    return doom_episode::selectSky(settings.episodeAtmosphere,
        settings.authored!=0,Game_HasEffect(GAME_EFFECT_AUTHORED_EPISODES)!=0,
        gamemode!=commercial,gameepisode,gamemode==commercial,skyKind);
}
uint32_t levelWeatherSeed() {
    return hash32((uint32_t)gameepisode*7919u+(uint32_t)gamemap*104729u+(uint32_t)skytexture*31u+17u);
}
// How hard it rains: a downpour, or less as the mood says (rainAmount), the
// gusts varying it a little.
float downpour(float t) {return (0.8f+0.2f*valueNoise(t,0,6,weatherSeed^0x5bd1e995u))*levelMood().rainAmount;}
void newLevel() {
    weatherLevelSerial=r_levelserial;weatherTic=-1;
    weatherSeed=levelWeatherSeed();
    doom_storm::reset(stormState);
    treeStrike=TreeStrike{};
    treeFires={};charredTrees={};charredCount=0;
    treeHistory={};historyCount=0;strikeOrder=0;
    treeCloseThunder={};
    stormFlashNow=stormBoltNow=stormBoltAzimuthNow=stormBoltSeedNow=0;
    stormCurtainNow=stormHazeNow=0;
    deimosBlizzardNow=deimosGustNow=0;
    starportDustNow=starportGustNow=0;
    citySmokeNow=bloodHellMistNow=0;
    episodeAtmosphereNow=static_cast<int>(episodeProfile());
    storm_audio::muteQueued();
    windSpeed=90+50*hashUnit(3,4,weatherSeed);
    rainNow=rainy()?downpour(0):0;wetNow=rainNow>0?1:0;glassWetNow=rainNow;
    soak.clear();soakPaneSerial=~0u;
    fprintf(stderr,"3D weather: %s.\n",rainy()?levelMood().rain?"authored rain":"rain everywhere":"dry");
}
// A broken pane's share of the wind blowing in through it: none when the
// rain slants away from it or past it, so only windows facing the rain let
// it in.
float inward(const PaneView &p) {
    float speed=std::hypot(rainWind[0],rainWind[1]);
    if(speed<1) return 0;
    return std::max(0.0f,(rainWind[0]*p.inX+rainWind[1]*p.inY)/speed);
}
bool letsRainIn(const PaneView &p) {return p.broken&&p.sky&&rainNow>0.02f&&inward(p)>0.05f;}
// Map-local eligibility. Trees already alight or burned to charcoal must
// never receive another local strike. Normal sky lightning continues if no
// untouched tree is available. These tests are deliberately type-aware.
bool treeAlreadyBurned(int sector,int type,int sprite,float x,float y) {
    for(const BurningTree &fire:treeFires)
        if(fire.born>=0&&
           doom_strike::sameTree(sector,type,sprite,x,y,
               fire.sector,fire.type,fire.sprite,fire.x,fire.y))return true;
    for(int n=0;n<charredCount;++n) {
        const CharredTree &charred=charredTrees[n];
        if(doom_strike::sameTree(sector,type,sprite,x,y,
                charred.sector,charred.type,charred.sprite,charred.x,charred.y))return true;
    }
    return false;
}
const TreeStrikeHistory *treeHistoryFor(int sector,int type,int sprite,float x,float y) {
    for(int n=0;n<historyCount;++n) {
        const TreeStrikeHistory &entry=treeHistory[n];
        if(doom_strike::sameTree(sector,type,sprite,x,y,
                entry.sector,entry.type,entry.sprite,entry.x,entry.y))return &entry;
    }
    return nullptr;
}
void rememberTreeStrike(const TreeStrike &target) {
    int index=-1;
    for(int n=0;n<historyCount;++n) {
        const TreeStrikeHistory &entry=treeHistory[n];
        if(doom_strike::sameTree(target.sector,target.type,target.sprite,target.x,target.y,
                entry.sector,entry.type,entry.sprite,entry.x,entry.y)) {index=n;break;}
    }
    if(index<0) {
        if(historyCount<doom_strike::maxStrikeHistory)index=historyCount++;
        else {
            // Bounded memory even on maps with hundreds of tree decorations.
            // Evict the least recently hit entry, not any game actor.
            index=0;
            for(int n=1;n<historyCount;++n)
                if(treeHistory[n].order<treeHistory[index].order)index=n;
        }
        treeHistory[index]=TreeStrikeHistory{};
        treeHistory[index].x=target.x;treeHistory[index].y=target.y;
        treeHistory[index].sector=target.sector;
        treeHistory[index].type=target.type;treeHistory[index].sprite=target.sprite;
    }
    TreeStrikeHistory &entry=treeHistory[index];
    entry.hits=std::min(entry.hits+1u,0xfffffffeu);
    entry.order=++strikeOrder;
}
// A rare fork can connect to a nearby, sky-exposed stock dead tree.
// Candidate selection runs only when the approved storm scheduler flashes.
void chooseTreeStrike(std::uint32_t eventSeed) {
    treeStrike=TreeStrike{};
    if(!doom_strike::selectEvent(eventSeed)||gamestate!=GS_LEVEL||
       !players[displayplayer].mo)return;
    const mobj_t *viewer=players[displayplayer].mo;
    const float vx=units(viewer->x),vy=units(viewer->y);
    const float angle=float(double(viewer->angle)*(2.0*doomPi/4294967296.0));
    const float fx=std::cos(angle),fy=std::sin(angle);
    unsigned bestHits=~0u,bestOrder=~0u;
    float bestTie=2.f;
    for(int s=0;s<numsectors;++s) {
        const sector_t *sector=&sectors[s];
        if(sector->ceilingpic!=skyflatnum)continue;
        for(const mobj_t *thing=sector->thinglist;thing;thing=thing->snext) {
            const bool small=thing->type==MT_MISC40&&thing->sprite==SPR_TRE1;
            if(!doom_strike::stockTree(thing->type,thing->sprite,
                    MT_MISC40,SPR_TRE1,MT_MISC76,SPR_TRE2))continue;
            const float x=units(thing->x),y=units(thing->y);
            const float dx=x-vx,dy=y-vy,dist=std::hypot(dx,dy);
            const float base=units(thing->z),ceil=ceilZ(sector);
            if(!doom_strike::eligibleTree(thing->type,thing->sprite,
                  small?MT_MISC40:MT_MISC76,small?SPR_TRE1:SPR_TRE2,
                  true,dist,ceil-base,small))continue;
            // Keep the established camera-facing and sector ownership rules.
            if(dx*fx+dy*fy<-.18f*dist)continue;
            if(!thing->subsector||thing->subsector->sector!=sector)continue;
            if(treeAlreadyBurned(s,thing->type,thing->sprite,x,y))continue;
            const TreeStrikeHistory *history=treeHistoryFor(s,thing->type,thing->sprite,x,y);
            const unsigned hits=history?history->hits:0;
            const unsigned order=history?history->order:0;
            const std::uint32_t h=doom_strike::hash(eventSeed^
                std::uint32_t(s)*0x9e3779b9u^
                std::uint32_t(thing->x)^std::uint32_t(thing->y));
            // Fair rotation: always favor a tree with fewer hits. When counts
            // match, pick the one least recently struck. Hash breaks ties.
            // This lets TRE1 be struck even beside a prominent TRE2.
            const float tie=doom_strike::unit(h)*.85f+
                .15f*dist/doom_strike::maxTargetDistance;
            if(hits>bestHits||(hits==bestHits&&(order>bestOrder||
                (order==bestOrder&&tie>=bestTie))))continue;
            bestHits=hits;bestOrder=order;bestTie=tie;
            treeStrike.x=x;treeStrike.y=y;treeStrike.ground=base;
            const float desired=small?std::max(24.f,.48f*(ceil-base)):
                                      std::max(55.f,.75f*(ceil-base));
            treeStrike.top=std::max(base+8.f,std::min(ceil-5.f,base+desired));
            treeStrike.seed=h;treeStrike.born=leveltime;
            treeStrike.sector=s;treeStrike.type=thing->type;treeStrike.sprite=thing->sprite;
        }
    }
    if(treeStrike.born>=0)rememberTreeStrike(treeStrike);
}

// Store coordinates only, not actor pointers. A cosmetic fire lasts at most
// 24 seconds and never edits actor health, collision, sprites, or the WAD.
void tryIgniteTree() {
    if(treeStrike.born<0||!doom_strike::ignite(treeStrike.seed))return;
    int burning=0;
    for(const BurningTree &fire:treeFires) {
        // Includes a just-expired fire awaiting this tic's cleanup pass.
        if(fire.born<0)continue;
        ++burning;
        if(doom_strike::sameTree(treeStrike.sector,treeStrike.type,treeStrike.sprite,
               treeStrike.x,treeStrike.y,fire.sector,fire.type,fire.sprite,fire.x,fire.y))return;
    }
    // Every new ignition reserves room for a permanent cosmetic charred entry.
    // Never start fires we cannot later mark as burned-out and ineligible.
    if(charredCount+burning>=doom_strike::maxCharredTrees||
       treeAlreadyBurned(treeStrike.sector,treeStrike.type,treeStrike.sprite,
           treeStrike.x,treeStrike.y))return;
    for(BurningTree &fire:treeFires) {
        if(doom_strike::fireActive(fire.born,leveltime))continue;
        fire.x=treeStrike.x;fire.y=treeStrike.y;fire.ground=treeStrike.ground;
        const bool small=treeStrike.type==MT_MISC40;
        fire.height=small?std::clamp((treeStrike.top-treeStrike.ground)*.48f,22.f,46.f):
            std::clamp((treeStrike.top-treeStrike.ground)*.45f,36.f,76.f);
        fire.sector=treeStrike.sector;fire.born=leveltime;
        fire.type=treeStrike.type;fire.sprite=treeStrike.sprite;
        fire.seed=doom_strike::hash(treeStrike.seed^0x7589de31u);
        break;
    }
}

// A destroyed/replaced decoration stops burning. At most four sector-list
// checks per second, with no retained mobj_t pointer or actor mutation.
bool fireHasTree(const BurningTree &fire) {
    if(fire.sector<0||fire.sector>=numsectors)return false;
    const sector_t *sector=&sectors[fire.sector];
    if(sector->ceilingpic!=skyflatnum)return false;
    for(const mobj_t *thing=sector->thinglist;thing;thing=thing->snext)
        if(thing->type==fire.type&&thing->sprite==fire.sprite&&
           doom_strike::stockTree(thing->type,thing->sprite,
               MT_MISC40,SPR_TRE1,MT_MISC76,SPR_TRE2)&&
           thing->subsector&&thing->subsector->sector==sector&&
           std::abs(units(thing->x)-fire.x)<1.f&&
           std::abs(units(thing->y)-fire.y)<1.f)return true;
    return false;
}
// This map-local registry is bounded, and never stores mobj_t pointers.
// A tree becomes charcoal only once an actual 24-second fire has ended.
void finishTreeFire(const BurningTree &fire) {
    if(!fireHasTree(fire))return;
    for(int n=0;n<charredCount;++n) {
        const CharredTree &c=charredTrees[n];
        if(c.sector==fire.sector&&c.type==fire.type&&c.sprite==fire.sprite&&
           std::abs(c.x-fire.x)<1.f&&std::abs(c.y-fire.y)<1.f)return;
    }
    if(charredCount>=doom_strike::maxCharredTrees)return;
    CharredTree &c=charredTrees[charredCount++];
    c.x=fire.x;c.y=fire.y;c.sector=fire.sector;
    c.type=fire.type;c.sprite=fire.sprite;
}
}

// Read-only from geometry's sprite-rendering path. The active tree is verified
// against its sector and sprite, so merely sharing an old coordinate is not enough.
bool infernoTreeCharred(const mobj_t *thing) {
    if(!thing||charredCount<=0||!settings.infernoTreeStrikes||
       (episodeAtmosphereNow!=int(doom_episode::Profile::Inferno)&&
        episodeAtmosphereNow!=int(doom_episode::Profile::BloodHell))||
       !doom_strike::stockTree(thing->type,thing->sprite,
           MT_MISC40,SPR_TRE1,MT_MISC76,SPR_TRE2)||
       !thing->subsector||!thing->subsector->sector)return false;
    const int sector=int(thing->subsector->sector-sectors);
    const float x=units(thing->x),y=units(thing->y);
    for(int n=0;n<charredCount;++n) {
        const CharredTree &c=charredTrees[n];
        if(c.sector==sector&&c.type==thing->type&&c.sprite==thing->sprite&&
           std::abs(c.x-x)<1.f&&std::abs(c.y-y)<1.f)return true;
    }
    return false;
}

float paneSoak(int pane) {return pane>=0&&(size_t)pane<soak.size()?soak[pane]:0.0f;}

// Where the rain blows (unit): the episode mood's rain angle, or else the
// level's own, picked by its hash. Never the sun's: the two are apart.
void rainHeading(float &x,float &y) {
    float authored=levelMood().rainAngle;
    float angle=authored>=0?authored*(float)doomPi/180:hashUnit(1,2,levelWeatherSeed())*2*(float)doomPi;
    x=std::cos(angle);y=std::sin(angle);
}

namespace {
// The ground's wetness per map cell under the sky (1 + wetness * 254; 0:
// not open to the sky), for the level and rain heading it was baked for.
std::vector<uint8_t> wetCells;
unsigned wetLevelSerial=~0u;
float wetHeading[2]={0,0};
}
// Bakes the wetness of the ground under the sky into the floor light's
// spare alpha (bake_atlas.cpp's floorLight layer; wear.glsl reads it). The
// rain comes in slanted on the wind, falling about four and a half units for
// one across: the foot of a wall facing it runs soaked, the lee of a wall,
// roof or ledge stays nearly dry; elsewhere the ground lies in patches damp,
// wet and soaked with puddles between. Steps of wetness, not a gradient, and
// the same all map long: the rain has been falling a while.
void bakeWetness() {
    if(flatLightCells.size()<(size_t)mapWidth*mapHeight*4||seamCells.size()<(size_t)mapWidth*mapHeight)return;
    float hx,hy;rainHeading(hx,hy);
    if(wetLevelSerial!=r_levelserial||wetCells.size()!=seamCells.size()||hx!=wetHeading[0]||hy!=wetHeading[1]) {
        wetLevelSerial=r_levelserial;wetHeading[0]=hx;wetHeading[1]=hy;
        wetCells.assign(seamCells.size(),0);
        uint32_t seed=levelWeatherSeed()^0x27d4eb2fu;
        constexpr float fall=4.5f,shelter=96;
        auto sectorIn=[&](float x,float y)->int {
            int cx=(int)std::floor((x-mapOrigin[0])/mapCell),cy=(int)std::floor((y-mapOrigin[1])/mapCell);
            if(cx<0||cy<0||cx>=mapWidth||cy>=mapHeight)return -1;
            int own=seamCells[(size_t)cy*mapWidth+cx].own;
            return own==noSector?-1:own;
        };
        // Whether rain at height z over (x, y) is stopped there: by the
        // void, a room (its roof runs up to the sky), or higher ground.
        auto stops=[&](float x,float y,float z) {
            int s=sectorIn(x,y);
            return s<0||sectors[s].ceilingpic!=skyflatnum||units(sectors[s].floorheight)>z;
        };
        for(int cy=0;cy<mapHeight;++cy)for(int cx=0;cx<mapWidth;++cx) {
            size_t i=(size_t)cy*mapWidth+cx;
            int own=seamCells[i].own;
            if(own==noSector||sectors[own].ceilingpic!=skyflatnum)continue;
            if((size_t)own<sectorMist.size()&&sectorMist[own].liquid) {wetCells[i]=1;continue;}
            float x=mapOrigin[0]+(cx+0.5f)*mapCell,y=mapOrigin[1]+(cy+0.5f)*mapCell,z=units(sectors[own].floorheight);
            bool lee=false,windward=false;
            for(float d=mapCell;d<=shelter&&!lee;d+=mapCell)lee=stops(x-hx*d,y-hy*d,z+fall*d);
            for(float d=mapCell;d<=std::max(mapCell,8.0f)&&!windward;d+=mapCell)windward=stops(x+hx*d,y+hy*d,z+24);
            float n=valueNoise(x,y,72,seed),wet=n<0.4f?0.35f:n<0.68f?0.6f:0.85f;
            if(valueNoise(x,y,40,seed^0x165667b1u)>0.7f)wet=1;
            if(windward)wet=std::max(wet,0.9f);
            if(lee)wet=0.12f;
            wetCells[i]=(uint8_t)(1+std::lround(wet*254));
        }
    }
    for(size_t i=0;i<wetCells.size();++i)flatCell(floorLight,i)[3]=wetCells[i];
}

// Each tic: the rain, the wind's gusts, whether the ground is wet (it is
// while it rains), how wet the glass gets (soaking in about 6 seconds,
// drying over one minute), and how much rain each broken window has let in:
// quick, the floor under it soaked in about 8 seconds with the wind blowing
// straight in, the wet spreading out from the sill as it does; four minutes
// to dry. Only a window facing the rain lets it in. One already broken when
// the level starts has been letting it in all along.
void watchWeather() {
    if(gamestate!=GS_LEVEL||levelSerial!=r_levelserial) return;
    if(weatherLevelSerial!=r_levelserial) newLevel();
    if(leveltime==weatherTic) return;
    weatherTic=leveltime;
    float t=leveltime/(float)TICRATE,gust=valueNoise(t,0,4,weatherSeed^0x68e31da4u);
    const bool rainEnabled=rainy();
    episodeAtmosphereNow=static_cast<int>(episodeProfile());
    // Independent slow storm front; regular rain is unchanged when the
    // advanced atmosphere setting is disabled (or storms are Off).
    const doom_storm::Atmosphere atmosphere=doom_storm::atmosphere(
        leveltime,TICRATE,settings.stormMode,rainEnabled&&settings.accelerated,
        settings.stormAtmosphere!=0,weatherSeed^0x43b817a9u);
    const float baseRain=rainEnabled?downpour(t):0.f;
    // A/B behavior is unchanged when atmospheric progression is disabled.
    rainNow=atmosphere.curtain>0.f?
        std::clamp(baseRain*atmosphere.rainScale,0.f,1.f):baseRain;
    stormCurtainNow=atmosphere.curtain;
    stormHazeNow=atmosphere.haze;
    // Deimos is a dry, abrasive blizzard even with Rain=Never or Storm=Off.
    // Its extra gusts only change wind speed; heading stays map-authored.
    const bool deimos=episodeAtmosphereNow==int(doom_episode::Profile::Deimos);
    const doom_episode::Blizzard blizzard=doom_episode::deimosBlizzard(
        deimos?leveltime:-1,TICRATE,weatherSeed^0x2dcf1357u);
    deimosBlizzardNow=blizzard.intensity;
    deimosGustNow=blizzard.gust;
    const bool starport=episodeAtmosphereNow==int(doom_episode::Profile::Starport);
    const doom_episode::Sandstorm sand=doom_episode::starportStorm(
        starport?leveltime:-1,TICRATE,weatherSeed^0x53b7a41du);
    starportDustNow=sand.intensity;
    starportGustNow=sand.gust;
    const doom_episode::CityAir cityAir=doom_episode::burningCity(
        episodeAtmosphereNow==int(doom_episode::Profile::BurningCity)?leveltime:-1,
        TICRATE,weatherSeed^0x83e70d91u);
    const doom_episode::BloodWeather bloodAir=doom_episode::bloodHellWeather(
        episodeAtmosphereNow==int(doom_episode::Profile::BloodHell)?leveltime:-1,
        TICRATE,weatherSeed^0x426eb7d3u);
    citySmokeNow=cityAir.intensity;
    bloodHellMistNow=bloodAir.intensity;
    // All other episodes retain the original rain/wind calculation.
    float wx,wy;rainHeading(wx,wy);
    const float speed=windSpeed*(0.6f+0.8f*gust)*atmosphere.windScale*
                      (deimos?blizzard.windScale:starport?sand.windScale:
                       episodeAtmosphereNow==int(doom_episode::Profile::BurningCity)?cityAir.windScale:
                       episodeAtmosphereNow==int(doom_episode::Profile::BloodHell)?bloodAir.windScale:1.f);
    rainWind[0]=wx*speed;rainWind[1]=wy*speed;
    // The heading turned (the authored layer switched on or off): the ground's wetness anew.
    if(flatLightTexture&&wetLevelSerial==r_levelserial&&(wx!=wetHeading[0]||wy!=wetHeading[1])) {
        bakeWetness();
        gpuUpdateTexture(flatLightTexture,GpuFormat::RGBA8,{{0,floorLight*mapHeight,mapWidth,mapHeight}},flatLightCells.data(),mapWidth*4);
    }
    constexpr float tic=1.0f/TICRATE;
    wetNow=rainNow>0?1:0;
    glassWetNow=rainNow>glassWetNow?glassWetNow+(rainNow-glassWetNow)*tic/6:std::max(rainNow,glassWetNow-tic/60);
    if(soakPaneSerial!=windowPaneSerial()) {
        soakPaneSerial=windowPaneSerial();soak.assign(windowPaneCount(),0);
        for(size_t i=0;i<soak.size();++i) {PaneView p;soak[i]=windowPane((int)i,p)&&letsRainIn(p)?1.0f:0.0f;}
    }
    for(size_t i=0;i<soak.size();++i) {
        PaneView p;
        if(!windowPane((int)i,p)) continue;
        if(letsRainIn(p)) soak[i]=std::min(1.0f,soak[i]+rainNow*(0.4f+0.6f*inward(p))*tic/8);
        else soak[i]=std::max(0.0f,soak[i]-tic/240);
    }
    I_AmbienceRain(settings.accelerated?rainNow:0);
    // Thunder lives on the game-tic clock, not camera distance or wall time.
    // This does not consume Doom gameplay RNG or alter rain simulation.
    // Themed electrical storms may run in dry Inferno and Deimos without
    // enabling water rain, wet floors, splashes or window droplets. Their
    // sound schedule is the original Storm A/B scheduler and sound samples.
    const bool themedDryStorm=(episodeAtmosphereNow==int(doom_episode::Profile::Inferno)||
        episodeAtmosphereNow==int(doom_episode::Profile::Deimos)||
        episodeAtmosphereNow==int(doom_episode::Profile::BloodHell))&&settings.episodeLightning!=0;
    const bool active=settings.accelerated&&(
        (rainNow>.01f&&settings.weather!=0)||themedDryStorm);
    doom_storm::Frame storm=doom_storm::advance(stormState,leveltime,TICRATE,
        settings.stormMode,active,settings.stormStrongFlashes!=0,
        weatherSeed^0x6a2b9b51u,settings.stormBolts!=0);
    stormFlashNow=storm.flash;
    stormBoltNow=storm.bolt;
    stormBoltAzimuthNow=storm.azimuth;
    stormBoltSeedNow=storm.boltSeed;
    const bool treeMode=(episodeAtmosphereNow==int(doom_episode::Profile::Inferno)||
        episodeAtmosphereNow==int(doom_episode::Profile::BloodHell))&&
        settings.infernoTreeStrikes&&settings.episodeLightning&&settings.stormBolts&&
        settings.stormMode>0&&settings.accelerated;
    if(!treeMode||!active) {
        treeStrike=TreeStrike{};
        treeFires={};
        treeCloseThunder={};
    } else {
        // The local strike can appear on any lightning flash, including
        // distant events without a separate sky bolt; audio stays unchanged.
        if(storm.flash>0.f&&stormState.flashTic==leveltime) {
            chooseTreeStrike(doom_strike::hash(weatherSeed^
                std::uint32_t(stormState.event)*0x4b7d1365u));
            if(treeStrike.born==leveltime) {
                tryIgniteTree();
                // A ground impact is local: prioritize the close recording and
                // suppress the event's normal, potentially distant thunder.
                doom_strike::scheduleClose(treeCloseThunder,leveltime,TICRATE);
            }
        }
        for(BurningTree &fire:treeFires) {
            if(fire.born<0)continue;
            if(!doom_strike::fireActive(fire.born,leveltime)) {
                finishTreeFire(fire);
                fire=BurningTree{};
            } else if(leveltime%TICRATE==0&&!fireHasTree(fire))fire=BurningTree{};
        }
    }
    if(!active||settings.stormMode<=0)storm_audio::muteQueued();
    else {
        // Never double-play close + ordinary thunder for one tree impact.
        if(storm.thunder!=doom_storm::Kind::None&&
           doom_strike::useOrdinaryThunder(treeCloseThunder))
            storm_audio::play(storm.thunder,settings.stormVolume/100.f,snd_SfxVolume);
        if(doom_strike::closeReady(treeCloseThunder,leveltime)) {
            // The 14-second distant sample may still be playing from a prior
            // event. Clear it so the local impact always gets audio priority.
            storm_audio::muteQueued();
            storm_audio::play(doom_storm::Kind::Close,
                settings.stormVolume/100.f,snd_SfxVolume);
        }
    }
}

namespace {
// Rain is pale grey-blue catching the sky's tint, unlit; the flashlight
// beam lights it from this too.
std::array<float,3> rainAlbedo() {return {0.5f+0.12f*skyTint[0],0.5f+0.12f*skyTint[1],0.53f+0.12f*skyTint[2]};}
// What a streak of rain at (x, y, z) looks like: in its sector's light, a
// little brighter than the walls behind it so it reads against them, and
// the baked lamp light of the floor under it, which it catches more the
// nearer the ground: a glowing pool or a lamp's pool of light below takes
// the rain falling into it over to its own color, by its share of the
// light there, and brightens it a little in that color. One lookup a drop.
std::array<float,3> rainColor(const sector_t &sector,float x,float y,float z) {
    float light=lighting(&sector),glow[3]={0,0,0};
    if(bakedLightsActive&&!flatLightCells.empty()) {
        int cx=std::clamp((int)std::floor((x-mapOrigin[0])/mapCell),0,mapWidth-1);
        int cy=std::clamp((int)std::floor((y-mapOrigin[1])/mapCell),0,mapHeight-1);
        const uint8_t *baked=flatCell(floorLight,(size_t)cy*mapWidth+cx);
        float near=1-0.85f*std::clamp((z-floorZ(&sector))/128,0.0f,1.0f);
        for(int c=0;c<3;++c)glow[c]=baked[c]/255.0f*2*near;
    }
    auto albedo=rainAlbedo();
    float peak=std::max({glow[0],glow[1],glow[2]}),share=peak>0.01f?std::min(1.0f,peak/(peak+light*0.5f)):0;
    std::array<float,3> color;
    for(int c=0;c<3;++c) {
        float tint=peak>0.01f?glow[c]/peak:1;
        color[c]=std::min(1.0f,albedo[c]*light*1.15f*(1+(tint-1)*share)+glow[c]*0.15f+0.04f);
    }
    return color;
}
const sector_t *sectorAt(float x,float y) {
    int cx=(int)std::floor((x-mapOrigin[0])/mapCell),cy=(int)std::floor((y-mapOrigin[1])/mapCell);
    if(cx<0||cy<0||cx>=mapWidth||cy>=mapHeight) return nullptr;
    int own=seamCells[(size_t)cy*mapWidth+cx].own;
    return own==noSector?nullptr:&sectors[own];
}
// A streak of rain whose head is at (x, y, z), moving along (vx, vy, vz):
// length units of it trail behind the head, drawn in pieces of whole Doom
// pixels, so a slant shows as a stair the way an old sprite would draw it.
void streak(const Uniforms &camera,float x,float y,float z,float vx,float vy,float vz,float length,int pieces,
            const std::array<float,3> &color,float alpha) {
    float speed=std::sqrt(vx*vx+vy*vy+vz*vz);
    if(speed<1) return;
    float step=length/pieces,dx=vx/speed,dy=vy/speed,dz=vz/speed;
    for(int k=0;k<pieces;++k) {
        float back=step*(k+0.5f);
        static std::array<float,3> albedo;albedo=rainAlbedo();
        pixelBlock(particleVertices,camera,x-dx*back,y-dy*back,z-dz*back,0.55f,step/0.55f,color,false,&albedo,true,nullptr,alpha);
    }
}
// A drop landing at (x, y, z): on a floor two specks jump aside over three
// tics; on a liquid (or a puddle) a ring of pixels grows round it.
void splash(const Uniforms &camera,float x,float y,float z,float tics,const sector_t &sector,uint32_t seed,bool puddle=false) {
    size_t index=&sector-sectors;
    bool liquid=puddle||(index<sectorMist.size()&&sectorMist[index].liquid);
    auto color=rainColor(sector,x,y,z);
    int tic=(int)tics;
    if(liquid) {
        if(tic>3) return;
        float radius=1.5f+1.5f*tic;
        int count=4+2*tic;
        for(int n=0;n<count;++n) {
            float a=(n+hashUnit(n,tic,seed)*0.5f)*2*(float)doomPi/count;
            pixelBlock(particleVertices,camera,x+std::cos(a)*radius,y+std::sin(a)*radius,z+0.3f,0.5f,1,color,true,nullptr,true,nullptr,0.55f-0.1f*tic);
        }
        return;
    }
    if(tic>2) return;
    float angle=hashUnit(5,6,seed)*2*(float)doomPi,out=1.0f+1.2f*tic,up=tic==1?1.5f:0.5f;
    for(int side=-1;side<=1;side+=2)
        pixelBlock(particleVertices,camera,x+std::cos(angle)*out*side,y+std::sin(angle)*out*side,z+up,0.5f,1,color,true,nullptr,true,nullptr,0.6f);
}
}

// The rain around the eye: one drop per 16-unit cell in a share of the
// cells as heavy as the rain (the same cells first as it thickens, so it
// grows rather than shuffles), out to 640 units, in sky sectors only, each
// falling from the sky down its sector's height and splashing. Then the
// broken windows the wind blows it in by.
// World-anchored short gusts: no persistent actors or accumulating particles.
// Check the full drift path so specks fade instead of passing through shelter.
void buildOutdoorWind(const Uniforms &camera) {
    if(!settings.outdoorWind||camera.effects[2]!=0||seamCells.empty()||levelSerial!=r_levelserial)return;
    constexpr float cell=64,reach=448,life=2.4f;
    float now=gameSeconds(),wx,wy;rainHeading(wx,wy);
    float density=std::clamp(settings.outdoorWindDensity,.25f,2.f)*.38f*(1-.85f*std::clamp(rainNow,0.f,1.f));
    int cx=int(std::floor(camera.eye[0]/cell)),cy=int(std::floor(camera.eye[1]/cell)),count=0;
    for(int gy=cy-8;gy<=cy+8&&count<128;++gy)for(int gx=cx-8;gx<=cx+8&&count<128;++gx){
        uint32_t seed=levelWeatherSeed()^0x871ab9u;
        if(hashUnit(gx,gy,seed)>density)continue;
        float clock=now+hashUnit(gx,gy,seed^1)*life;
        int cycle=int(std::floor(clock/life));float age=clock-cycle*life;
        seed^=uint32_t(cycle)*2654435761u;
        float x0=(gx+hashUnit(gx,gy,seed^2))*cell,y0=(gy+hashUnit(gx,gy,seed^3))*cell;
        const sector_t *room=sectorAt(x0,y0);
        if(!room||room->ceilingpic!=skyflatnum)continue;
        int own=int(room-sectors);bool ground=hashUnit(gx,gy,seed^4)<.35f;
        if(ground&&sectorMist[own].liquid)continue;
        float speed=(ground?105.f:85.f)+70*hashUnit(gx,gy,seed^5),phase=hashUnit(gx,gy,seed^6)*2*doomPi;
        // Integrate shared gusts so nearby specks surge together without
        // jumping when the wind changes speed. Small eddies add crosswind.
        float born=now-age;
        auto drift=[&](float t,float &dx,float &dy){
            float travel=speed*(t+.32f*(std::sin((born+t)*2)-std::sin(born*2)));
            float swirl=(ground?7.f:13.f)*(std::sin(t*4+phase)-std::sin(phase));
            dx=wx*travel-wy*swirl;dy=wy*travel+wx*swirl;
        };
        float driftX,driftY;drift(age,driftX,driftY);
        float x=x0+driftX,y=y0+driftY;
        bool sheltered=false;
        int steps=std::clamp(int(std::ceil(speed*1.7f*age/8)),1,96);
        for(int k=1;k<=steps;++k){float sx,sy;drift(age*k/steps,sx,sy);
            if(sectorAt(x0+sx,y0+sy)!=room){sheltered=true;break;}}
        if(sheltered)continue;
        float floor=floorZ(room),z=floor+(ground?2.f:12+100*hashUnit(gx,gy,seed^7));
        z+=ground?4*std::abs(std::sin(age*7+phase)):9*std::sin(age*4+phase);
        if(z>=ceilZ(room)-2)continue;
        float dx=x-camera.eye[0],dy=y-camera.eye[1],dist=std::hypot(dx,dy);
        if(dist>reach||dist<16)continue;
        float fade=std::min({1.f,age*2,(life-age)*2,(reach-dist)/96,(dist-16)/24});
        Image &flat=lumpImage(firstflat+flattranslation[room->floorpic],true);
        auto tint=averageColor(flat);
        float light=lighting(room);
        for(float &c:tint)c=(.32f+c*.45f)*light;
        pixelBlock(particleVertices,camera,x,y,z,ground?1.2f:.55f,ground?.65f:1.f,tint,false,nullptr,false,nullptr,fade*(ground?.65f:.38f));
        ++count;
    }
}

extern "C" void I_TestOutdoorWind() {
    auto saved=settings;float oldRain=rainNow;auto savedVertices=std::move(particleVertices);
    settings.outdoorWind=1;settings.outdoorWindDensity=2;rainNow=0;
    Uniforms camera={};camera.forward[0]=1;camera.right[1]=1;camera.up[2]=1;camera.projection[0]=camera.projection[1]=1;
    size_t dry=0;
    for(int y=0;y<mapHeight&&!dry;y+=4)for(int x=0;x<mapWidth&&!dry;x+=4){
        float px=mapOrigin[0]+(x+.5f)*mapCell,py=mapOrigin[1]+(y+.5f)*mapCell;
        const sector_t *s=sectorAt(px,py);if(!s||s->ceilingpic!=skyflatnum)continue;
        camera.eye[0]=px;camera.eye[1]=py;camera.eye[2]=floorZ(s)+40;
        particleVertices.clear();buildOutdoorWind(camera);dry=particleVertices.size();
    }
    if(!dry||dry>128*6)I_Error("Outdoor wind missing or over particle budget");
    particleVertices.clear();rainNow=1;buildOutdoorWind(camera);
    if(particleVertices.size()>dry)I_Error("Rain increased wind dust");
    particleVertices.clear();settings.outdoorWind=0;buildOutdoorWind(camera);
    if(!particleVertices.empty())I_Error("Wind toggle failed");
    settings=saved;rainNow=oldRain;particleVertices=std::move(savedVertices);
    puts("PASS outdoor wind renders under sky, respects particle cap, rain reduction and toggle");
}

// Milestone C: world-anchored distant rain curtains under actual sky sectors.
// Fixed 12-curtain / 96-streak ceiling, with no dynamic actors or persistent
// geometry. This adds to (not replaces) the existing per-cell rain and splash.
// Each streak is validated against sky exposure so it cannot enter a room.
static void buildStormCurtains(const Uniforms &camera,float now) {
    if(stormCurtainNow<0.10f||!settings.stormAtmosphere)return;
    constexpr float cell=160.f,reach=640.f;
    constexpr int maxCurtains=12,streaksPerCurtain=8;
    const int cx=int(std::floor(camera.eye[0]/cell));
    const int cy=int(std::floor(camera.eye[1]/cell));
    const float windLength=std::hypot(rainWind[0],rainWind[1]);
    if(windLength<1.f)return;
    const float wx=rainWind[0]/windLength,wy=rainWind[1]/windLength;
    const uint32_t salt=weatherSeed^0x7dbbe321u;
    int rendered=0;
    for(int gy=cy-4;gy<=cy+4&&rendered<maxCurtains;++gy)
    for(int gx=cx-4;gx<=cx+4&&rendered<maxCurtains;++gx) {
        if(hashUnit(gx,gy,salt)>.12f+.17f*stormCurtainNow)continue;
        const float centerX=(gx+.25f+.50f*hashUnit(gx,gy,salt^1u))*cell;
        const float centerY=(gy+.25f+.50f*hashUnit(gx,gy,salt^2u))*cell;
        const float dist=std::hypot(centerX-camera.eye[0],centerY-camera.eye[1]);
        if(dist<190.f||dist>reach)continue;
        const sector_t *sector=sectorAt(centerX,centerY);
        if(!sector||sector->ceilingpic!=skyflatnum)continue;
        const float bottom=floorZ(sector)+12.f;
        const float top=std::max(ceilZ(sector),camera.eye[2]+100.f)-14.f;
        if(top-bottom<36.f)continue;
        const float phase=hashUnit(gx,gy,salt^3u);
        // Horizontal sweep repeats per cell; fade the curtain near its wrap.
        const float motion=std::fmod(now*(28.f+20.f*stormCurtainNow)+phase*cell,cell)/cell;
        const float sweep=(motion-.5f)*cell*.42f;
        const float wrapFade=std::clamp(std::min(motion,1.f-motion)*8.f,0.f,1.f);
        const float distanceFade=std::clamp((reach-dist)/120.f,0.f,1.f);
        const float alpha=stormCurtainNow*wrapFade*distanceFade*.21f;
        if(alpha<.015f)continue;
        auto tint=rainAlbedo();
        for(float &component:tint)component=std::min(1.f,component*1.18f);
        for(int n=0;n<streaksPerCurtain;++n) {
            // A sparse narrow vertical sheet, perpendicular to wind.
            const float across=(float(n)-3.5f)*10.f;
            const float x=centerX-wy*across+wx*sweep;
            const float y=centerY+wx*across+wy*sweep;
            const sector_t *at=sectorAt(x,y);
            if(!at||at->ceilingpic!=skyflatnum)continue;
            const float localFloor=floorZ(at)+10.f;
            const float localTop=std::max(ceilZ(at),camera.eye[2]+100.f)-14.f;
            if(localTop-localFloor<32.f)continue;
            const float speed=440.f+70.f*hashUnit(n,gx,salt^4u);
            const float fall=localTop-localFloor;
            const float z=localFloor+std::fmod(now*speed+fall*hashUnit(n,gy,salt^5u),fall);
            const float opacity=alpha*(.65f+.35f*hashUnit(n,gy,salt^6u));
            streak(camera,x,y,z,rainWind[0],rainWind[1],-speed,
                   12.f,1,tint,opacity);
        }
        ++rendered;
    }
}

void buildRain(const Uniforms &camera) {
    if(rainNow<=0.01f||camera.effects[2]!=0||seamCells.empty()||levelSerial!=r_levelserial||!settings.weather) return;
    constexpr float cell=16,reach=640;
    float now=std::floor(gameSeconds()*TICRATE)/TICRATE;
    float density=0.7f*rainNow;
    int gx0=(int)std::floor((camera.eye[0]-reach)/cell),gx1=(int)std::floor((camera.eye[0]+reach)/cell);
    int gy0=(int)std::floor((camera.eye[1]-reach)/cell),gy1=(int)std::floor((camera.eye[1]+reach)/cell);
    float flat=std::hypot(camera.forward[0],camera.forward[1]);
    float fx=flat>1e-3f?camera.forward[0]/flat:0,fy=flat>1e-3f?camera.forward[1]/flat:0;
    for(int gy=gy0;gy<=gy1;++gy)for(int gx=gx0;gx<=gx1;++gx) {
        if(hashUnit(gx,gy,weatherSeed)>=density) continue;
        float x0=(gx+hashUnit(gx,gy,weatherSeed^1u))*cell,y0=(gy+hashUnit(gx,gy,weatherSeed^2u))*cell;
        float dx=x0-camera.eye[0],dy=y0-camera.eye[1],distance=std::hypot(dx,dy);
        // Behind the eye only near it, where a glance down still shows them.
        if(distance>reach||(distance>96&&(dx*fx+dy*fy)<-0.3f*distance&&flat>0.5f)) continue;
        const sector_t *sector=sectorAt(x0,y0);
        if(!sector||sector->ceilingpic!=skyflatnum) continue;
        float floor=floorZ(sector),top=std::max(ceilZ(sector),camera.eye[2]+160),height=top-floor;
        if(height<8) continue;
        uint32_t seed=hash32((uint32_t)gx*73856093u^(uint32_t)gy*19349663u^weatherSeed);
        float speed=520*(0.85f+0.3f*hashUnit(gx,gy,weatherSeed^3u)),fall=height/speed,period=fall+0.15f;
        float clock=now+hashUnit(gx,gy,weatherSeed^4u)*period*7;
        int cycle=(int)std::floor(clock/period);
        float age=clock-cycle*period;
        uint32_t own=seed^(uint32_t)cycle*2654435761u;
        // It lands in its cell, coming in on the wind from upwind of it; on
        // the way it may cross other open sky, or pass over a roof, never
        // through a room.
        float lx=x0+(hashUnit(cycle,1,own)-0.5f)*cell*0.5f,ly=y0+(hashUnit(cycle,2,own)-0.5f)*cell*0.5f;
        float early=std::min(age,fall)-fall;
        float x=lx+rainWind[0]*early,y=ly+rainWind[1]*early;
        const sector_t *landing=sectorAt(lx,ly);
        if(landing!=sector) continue;
        const sector_t *at=sectorAt(x,y);
        float zHere=top-speed*std::min(age,fall);
        if(!at||(at!=sector&&at->ceilingpic!=skyflatnum&&zHere<ceilZ(at))) continue;
        if(age<fall) {
            float z=zHere;
            if(z<floor+1) continue;
            float alpha=0.32f+0.25f*hashUnit(cycle,3,own);
            streak(camera,x,y,z,rainWind[0],rainWind[1],-speed,10,distance<320?3:distance<520?2:1,rainColor(*sector,x,y,z),alpha*std::min(1.0f,rainNow*2));
        }
        else if(distance<384) splash(camera,lx,ly,floor,(age-fall)*TICRATE,*sector,own);
    }
    // A capped, distant rain layer adds depth only while advanced storms
    // are enabled; existing window/rain effects retain their normal behavior.
    buildStormCurtains(camera,now);
    // Through broken windows: drops blown in across the opening, slow at
    // first and falling ever faster, to splash on the sill or the floor
    // inside. As many as the window is wide, the rain heavy and the wind
    // blowing in; none through a window the rain slants away from.
    for(int i=0;i<windowPaneCount();++i) {
        PaneView p;
        if(!windowPane(i,p)||!letsRainIn(p)||p.top-p.bottom<4) continue;
        float ex=p.bx-p.ax,ey=p.by-p.ay,len=std::hypot(ex,ey);
        float mx=(p.ax+p.bx)/2,my=(p.ay+p.by)/2;
        if(len<4||std::hypot(mx-camera.eye[0],my-camera.eye[1])>reach) continue;
        float in=inward(p),gust=std::hypot(rainWind[0],rainWind[1]);
        int count=std::min(32,(int)std::lround(len/5*rainNow*in));
        for(int n=0;n<count;++n) {
            uint32_t seed=p.seed^(uint32_t)n*0x9e3779b9u;
            float period=0.55f+0.3f*hashUnit(n,0,seed),clock=now+hashUnit(n,1,seed)*period*5;
            int cycle=(int)std::floor(clock/period);
            float age=clock-cycle*period;
            uint32_t own=seed^(uint32_t)cycle*2654435761u;
            float u=hashUnit(cycle,1,own)*len,z0=p.bottom+(p.top-p.bottom)*(0.2f+0.8f*hashUnit(cycle,2,own));
            float speed=(30+gust*1.6f)*in*(0.7f+0.6f*hashUnit(cycle,3,own));
            float x=p.ax+ex/len*u+p.inX*speed*age,y=p.ay+ey/len*u+p.inY*speed*age,z=z0-450*age*age-60*age;
            const sector_t *at=sectorAt(x,y);
            if(!at||(at!=&sectors[p.frame]&&at!=&sectors[p.inner>=0?p.inner:p.frame]&&at!=&sectors[p.outer])) continue;
            float floor=floorZ(at);
            if(z>floor+1) {
                streak(camera,x,y,z,p.inX*speed,p.inY*speed,-(900*age+60),6,2,rainColor(*at,x,y,z),0.45f);
                continue;
            }
            // Landed: when it reached the floor, then the splash from there.
            float landed=(-60+std::sqrt(3600+1800*std::max(0.0f,z0-floor)))/900;
            float lx=p.ax+ex/len*u+p.inX*speed*landed,ly=p.ay+ey/len*u+p.inY*speed*landed;
            const sector_t *under=sectorAt(lx,ly);
            if(under)splash(camera,lx,ly,floorZ(under),(age-landed)*TICRATE,*under,own);
        }
    }
    // Leaks (wear.cpp): a drop swells at the crack, lets go, falls and
    // rings the puddle below, one about every second.
    for(const CeilingLeak &l:ceilingLeaks()) {
        if(std::hypot(l.x-camera.eye[0],l.y-camera.eye[1])>reach)continue;
        const sector_t &sector=sectors[l.sector];
        float top=ceilZ(&sector),floor=floorZ(&sector),fall=std::sqrt(2*std::max(0.0f,top-floor)/800);
        float period=0.8f+0.5f*hashUnit(3,0,l.seed)+fall,clock=now+hashUnit(4,0,l.seed)*period,age=clock-std::floor(clock/period)*period;
        float hang=period-fall-0.15f;
        auto color=rainColor(sector,l.x,l.y,top);
        if(age<hang) {
            if(age>hang*0.4f)pixelBlock(particleVertices,camera,l.x,l.y,top-0.6f,0.5f,1,color,false,nullptr,true,nullptr,0.6f);
        }
        else if(age<hang+fall) {
            float t=age-hang,z=top-400*t*t;
            streak(camera,l.x,l.y,z,0,0,-800*t-1,3,1,rainColor(sector,l.x,l.y,z),0.6f);
        }
        else splash(camera,l.x,l.y,floor,(age-hang-fall)*TICRATE,sector,l.seed,true);
    }
}

// D2.1 — Whiteout: Deimos has a near-field windblown layer and a thinner
// drifting powder layer. Uses deterministic world cells; no gameplay actors,
// persistent allocation or new shader. Every candidate and the swept motion
// path must remain under open sky. Ring order prioritizes near-camera flecks
// when the hard particle budget is reached, avoiding a far-side bias.
static void buildDeimosAtmosphere(const Uniforms &camera) {
    if(camera.effects[2]!=0||levelSerial!=r_levelserial||seamCells.empty())return;
    constexpr float cell=28.f,reach=455.f;
    const int density=std::clamp(settings.episodeParticles,0,2);
    const int crystalLimit=doom_episode::crystalBudget(density);
    const int dustLimit=doom_episode::frostDustBudget(density);
    const float now=gameSeconds();
    const float windLength=std::hypot(rainWind[0],rainWind[1]);
    const float wx=windLength>1.f?rainWind[0]/windLength:1.f;
    const float wy=windLength>1.f?rainWind[1]/windLength:0.f;
    const float gust=std::clamp(deimosGustNow,0.f,1.f);
    const float storm=std::clamp(deimosBlizzardNow,0.f,1.f);
    // The same depth extinction as outdoor surfaces: far crystals blend
    // into the cold veil instead of floating sharply over obscured scenery.
    const float frostStrength=settings.fog?
        doom_episode::deimosFogStrength(settings.deimosFog,storm):0.f;
    const float wind=std::clamp(windLength,90.f,270.f);
    const uint32_t salt=weatherSeed^0x65ba19d3u;
    const int cx=(int)std::floor(camera.eye[0]/cell);
    const int cy=(int)std::floor(camera.eye[1]/cell);
    int crystals=0,dust=0;
    // Bounded 33x33 neighboring cells. The inner rings are processed first,
    // so foreground density does not disappear when the cap is reached.
    for(int ring=0;ring<=16&&(crystals<crystalLimit||dust<dustLimit);++ring)
    for(int gy=cy-ring;gy<=cy+ring&&(crystals<crystalLimit||dust<dustLimit);++gy)
    for(int gx=cx-ring;gx<=cx+ring&&(crystals<crystalLimit||dust<dustLimit);++gx) {
        if(std::max(std::abs(gx-cx),std::abs(gy-cy))!=ring)continue;
        const uint32_t seed=hash32(uint32_t(gx)*0x9f28a713u^
                                   uint32_t(gy)*0x6c8e9cf5u^salt);
        for(int kind=0;kind<2;++kind) {
            const bool fine=kind!=0;
            if(fine?dust>=dustLimit:crystals>=crystalLimit)continue;
            const float selection=hashUnit(gx,gy,seed^(fine?0x293a16u:0xb1ce49u));
            if(selection>(fine?.75f+.10f*storm:.91f+.07f*storm))continue;
            const float life=fine?3.5f:2.8f;
            const float phase=hashUnit(gx,gy,seed^0x4c741fu);
            const float clock=now+phase*life;
            const int cycle=(int)std::floor(clock/life);
            const float age=clock-float(cycle)*life;
            const uint32_t own=hash32(seed^uint32_t(cycle)*0x9e3779b9u);
            const float x0=(gx+.12f+.76f*hashUnit(gx,gy,own^0x14u))*cell;
            const float y0=(gy+.12f+.76f*hashUnit(gx,gy,own^0x25u))*cell;
            const sector_t *origin=sectorAt(x0,y0);
            if(!origin||origin->ceilingpic!=skyflatnum)continue;
            const float floor=floorZ(origin),ceiling=ceilZ(origin),height=ceiling-floor;
            if(height<18.f)continue;
            const float start=floor+std::min(height-5.f,8.f+height*
                (.18f+.68f*hashUnit(gx,gy,own^0x41u)));
            const float vz=fine?-(3.f+7.f*hashUnit(gx,gy,own^0x63u)):
                -(11.f+15.f*hashUnit(gx,gy,own^0x54u));
            // Ice cuts across the air with sustained sideways motion. Powder
            // blows slower and broadens the nearby field between gusts.
            const float drift=wind*(fine?.35f:.61f)*(0.80f+.36f*gust);
            const float swirl=(fine?8.f:4.f)*std::sin(age*(fine?1.5f:2.2f)+phase*6.2831853f);
            const float dx=wx*drift*age-wy*swirl;
            const float dy=wy*drift*age+wx*swirl;
            const float x=x0+dx,y=y0+dy,z=start+vz*age;
            const sector_t *at=sectorAt(x,y);
            if(!at||at->ceilingpic!=skyflatnum||z<=floorZ(at)+2.f||
               z>=ceilZ(at)-2.f)continue;
            bool blocked=false;
            // Up to five conservative samples prevent wind transport through
            // indoor rooms (including a narrow roof along the swept path).
            for(int step=1;step<=5;++step) {
                const float u=float(step)/6.f;
                const sector_t *through=sectorAt(x0+dx*u,y0+dy*u);
                const float hz=start+vz*age*u;
                if(!through||through->ceilingpic!=skyflatnum||
                   hz<=floorZ(through)+2.f||hz>=ceilZ(through)-2.f) {
                    blocked=true;break;
                }
            }
            if(blocked)continue;
            const float dist=std::hypot(x-camera.eye[0],y-camera.eye[1]);
            if(dist<12.f||dist>reach)continue;
            const float fade=std::clamp(std::min({age*2.7f,(life-age)*2.7f,
                       (reach-dist)/80.f,(dist-12.f)/18.f}),0.f,1.f);
            if(fade<.025f)continue;
            const float light=std::clamp(lighting(at),.22f,1.1f);
            const float hue=hashUnit(gx,gy,own^0x78u);
            std::array<float,3> color=fine?
                std::array<float,3>{(.65f+.10f*hue)*(.75f+.25f*light),
                                    (.68f+.10f*hue)*(.75f+.25f*light),
                                    (.78f+.10f*hue)*(.75f+.25f*light)}:
                std::array<float,3>{(.72f+.13f*hue)*(.75f+.25f*light),
                                    (.83f+.10f*hue)*(.75f+.25f*light),
                                    (.98f+.02f*hue)*(.75f+.25f*light)};
            const float spatialDistance=std::hypot(dist,z-camera.eye[2]);
            const float fogAmount=doom_episode::deimosFogOpacity(spatialDistance,frostStrength);
            const std::array<float,3> frost={.43f,.46f,.56f};
            for(int c=0;c<3;++c)color[c]=color[c]*(1.f-fogAmount*.58f)+frost[c]*(fogAmount*.58f);
            const float alpha=fade*(fine?.43f:.72f)*(0.82f+.18f*storm)*
                (1.f-.48f*fogAmount);
            // Separate bright head and dimmer trailing shard make a short,
            // oblique gust streak without adding any new renderer primitives.
            pixelBlock(particleVertices,camera,x,y,z,
                       fine?.75f:.77f,fine?.85f:1.80f,color,
                       false,nullptr,false,nullptr,alpha);
            if(!fine) {
                // Tail stays close to the validated head; the full path to
                // the current head was already conservatively checked.
                const float trail=2.3f+2.4f*gust;
                const float tx=x-wx*trail,ty=y-wy*trail,tz=z+.55f;
                const sector_t *tail=sectorAt(tx,ty);
                if(tail&&tail->ceilingpic==skyflatnum&&
                   tz>floorZ(tail)+2.f&&tz<ceilZ(tail)-2.f)
                    pixelBlock(particleVertices,camera,tx,ty,tz,
                               .48f,.90f,color,false,nullptr,false,nullptr,alpha*.35f);
                ++crystals;
            } else ++dust;
        }
    }
}

// E1 — Starport dusk duststorm. Analytic world-cell billboards; dry, cosmetic,
// and bounded. A swept open-sky check rejects particles crossing an indoor
// sector. Near dust and larger gust flecks share the same moving wind field.
static void buildStarportAtmosphere(const Uniforms &camera) {
    if(camera.effects[2]!=0||levelSerial!=r_levelserial||seamCells.empty())return;
    constexpr float cell=35.f,reach=480.f;
    const int density=std::clamp(settings.episodeParticles,0,2);
    const int fineCap=doom_episode::sandBudget(density);
    const int gritCap=doom_episode::sandGustBudget(density);
    const float now=gameSeconds();
    const float windLength=std::hypot(rainWind[0],rainWind[1]);
    const float wx=windLength>1.f?rainWind[0]/windLength:1.f;
    const float wy=windLength>1.f?rainWind[1]/windLength:0.f;
    const float speed=std::clamp(windLength,95.f,275.f);
    const float intensity=std::clamp(starportDustNow,0.f,1.f);
    const uint32_t salt=weatherSeed^0x45f6a12du;
    const int cx=(int)std::floor(camera.eye[0]/cell);
    const int cy=(int)std::floor(camera.eye[1]/cell);
    int fine=0,grit=0;
    // Nearest rings first: no distant-field bias if the hard cap is reached.
    for(int ring=0;ring<=15&&(fine<fineCap||grit<gritCap);++ring)
    for(int gy=cy-ring;gy<=cy+ring&&(fine<fineCap||grit<gritCap);++gy)
    for(int gx=cx-ring;gx<=cx+ring&&(fine<fineCap||grit<gritCap);++gx) {
        if(std::max(std::abs(gx-cx),std::abs(gy-cy))!=ring)continue;
        const uint32_t seed=hash32(uint32_t(gx)*0x769d5bbdu^
                                    uint32_t(gy)*0x2bb4e31bu^salt);
        for(int type=0;type<2;++type) {
            const bool heavy=type!=0;
            if(heavy?grit>=gritCap:fine>=fineCap)continue;
            if(hashUnit(gx,gy,seed^(heavy?0x91ca13u:0x0dc4e7u))>
                (heavy?.20f+.08f*intensity:.66f+.24f*intensity))continue;
            const float life=heavy?2.5f:3.6f;
            const float phase=hashUnit(gx,gy,seed^0x541cc5u);
            const float clock=now+phase*life;
            const int cycle=(int)std::floor(clock/life);
            const float age=clock-float(cycle)*life;
            const uint32_t own=hash32(seed^uint32_t(cycle)*0x9e3779b9u);
            const float x0=(gx+.12f+.76f*hashUnit(gx,gy,own^0x1fu))*cell;
            const float y0=(gy+.12f+.76f*hashUnit(gx,gy,own^0x26u))*cell;
            const sector_t *origin=sectorAt(x0,y0);
            if(!origin||origin->ceilingpic!=skyflatnum)continue;
            const float floor=floorZ(origin),ceiling=ceilZ(origin);
            const float room=ceiling-floor;
            if(room<17.f)continue;
            const float start=floor+std::min(room-5.f,5.f+room*
                (.11f+.58f*hashUnit(gx,gy,own^0x45u)));
            const float velocity=heavy?-(4.f+7.f*hashUnit(gx,gy,own^0x35u)):
                -(1.5f+5.f*hashUnit(gx,gy,own^0x41u));
            const float travel=speed*(heavy?.65f:.42f)*age;
            const float eddy=(heavy?9.f:14.f)*
                (std::sin(age*(heavy?2.4f:1.9f)+phase*6.2831853f)-
                 std::sin(phase*6.2831853f));
            const float dx=wx*travel-wy*eddy;
            const float dy=wy*travel+wx*eddy;
            const float x=x0+dx,y=y0+dy,z=start+velocity*age;
            const sector_t *at=sectorAt(x,y);
            if(!at||at->ceilingpic!=skyflatnum||z<=floorZ(at)+2.f||
               z>=ceilZ(at)-2.f)continue;
            bool sheltered=false;
            for(int k=1;k<=5;++k) {
                const float u=float(k)/6.f;
                const sector_t *along=sectorAt(x0+dx*u,y0+dy*u);
                const float zz=start+velocity*age*u;
                if(!along||along->ceilingpic!=skyflatnum||
                   zz<=floorZ(along)+2.f||zz>=ceilZ(along)-2.f) {sheltered=true;break;}
            }
            if(sheltered)continue;
            const float dist=std::hypot(x-camera.eye[0],y-camera.eye[1]);
            if(dist<12.f||dist>reach)continue;
            const float fade=std::clamp(std::min({age*2.4f,(life-age)*2.4f,
                (reach-dist)/85.f,(dist-12.f)/20.f}),0.f,1.f);
            if(fade<.025f)continue;
            const float light=std::clamp(lighting(at),.26f,1.f);
            const float variation=hashUnit(gx,gy,own^0x73u);
            std::array<float,3> tint=heavy?
                std::array<float,3>{.51f+.13f*variation,.32f+.11f*variation,.18f+.07f*variation}:
                std::array<float,3>{.56f+.14f*variation,.39f+.12f*variation,.24f+.07f*variation};
            for(float &c:tint)c*=.68f+.32f*light;
            const float fog=1.f-std::exp(-std::max(dist-75.f,0.f)/430.f);
            const float alpha=fade*(heavy?.72f:.49f)*(.78f+.22f*intensity)*
                (1.f-.42f*fog);
            pixelBlock(particleVertices,camera,x,y,z,
                heavy?1.5f:.85f,heavy?1.4f:1.0f,tint,
                false,nullptr,false,nullptr,alpha);
            if(heavy)++grit;else ++fine;
        }
    }
}

// E2.2 — Burning City: textured Fluid smoke masks, soot and rare embers.
// Two alpha-masked quads per puff (NSHSF0 and NSHSA0), not rectangular
// pixelBlock tiers. Reuses the existing Fluid GPU image cache and effect
// pipeline, with no persistent actors, gameplay RNG, rain, or wetness.
// Maximum at High: 2*96 smoke + 56 ash = 248 quads.
// Renderer order: buildImpacts clears effectBatches BEFORE this function. 
static void buildBurningCityAtmosphere(const Uniforms &camera) {
    if(camera.effects[2]!=0||levelSerial!=r_levelserial||seamCells.empty())return;
    constexpr float cell=38.f,reach=490.f;
    const int density=std::clamp(settings.episodeParticles,0,2);
    const int smokeCap=doom_episode::citySmokeBudget(density);
    const int ashCap=doom_episode::cityAshBudget(density);
    const float now=gameSeconds();
    const float windLength=std::hypot(rainWind[0],rainWind[1]);
    const float wx=windLength>1.f?rainWind[0]/windLength:1.f;
    const float wy=windLength>1.f?rainWind[1]/windLength:0.f;
    const float intensity=std::clamp(citySmokeNow,0.f,1.f);
    const uint32_t salt=weatherSeed^0x4bb29f7du;
    const int cx=int(std::floor(camera.eye[0]/cell));
    const int cy=int(std::floor(camera.eye[1]/cell));
    int smoke=0,ash=0;
    for(int ring=0;ring<=15&&(smoke<smokeCap||ash<ashCap);++ring)
    for(int gy=cy-ring;gy<=cy+ring&&(smoke<smokeCap||ash<ashCap);++gy)
    for(int gx=cx-ring;gx<=cx+ring&&(smoke<smokeCap||ash<ashCap);++gx) {
        if(std::max(std::abs(gx-cx),std::abs(gy-cy))!=ring)continue;
        const uint32_t seed=hash32(uint32_t(gx)*0x6e8feb86u^
                                    uint32_t(gy)*0x49b1ca15u^salt);
        for(int kind=0;kind<2;++kind) {
            const bool soot=kind!=0;
            if(soot?ash>=ashCap:smoke>=smokeCap)continue;
            if(hashUnit(gx,gy,seed^(soot?0x71d13au:0xe54c3bu))>
               (soot?.48f:.76f+.12f*intensity))continue;
            const float life=soot?4.8f:6.3f;
            const float phase=hashUnit(gx,gy,seed^0x83a31du);
            const float clock=now+phase*life;
            const int cycle=int(std::floor(clock/life));
            const float age=clock-float(cycle)*life;
            const uint32_t own=hash32(seed^uint32_t(cycle)*0x9e3779b9u);
            const float x0=(gx+.12f+.76f*hashUnit(gx,gy,own^0x31u))*cell;
            const float y0=(gy+.12f+.76f*hashUnit(gx,gy,own^0x42u))*cell;
            const sector_t *origin=sectorAt(x0,y0);
            if(!origin||origin->ceilingpic!=skyflatnum)continue;
            const float floor=floorZ(origin),ceiling=ceilZ(origin);
            const float height=ceiling-floor;
            if(height<24.f)continue;
            const float start=floor+std::min(height-6.f,8.f+height*
                (.07f+.48f*hashUnit(gx,gy,own^0x53u)));
            const bool ember=soot&&hashUnit(gx,gy,own^0x64u)<.28f;
            const float rise=soot?(ember?7.f:-4.f):8.f+6.f*hashUnit(gx,gy,own^0x75u);
            const float drift=(soot?13.f:19.f)*age;
            const float swirl=(soot?5.f:11.f)*
                (std::sin(age*(soot?1.7f:.84f)+phase*6.2831853f)-
                 std::sin(phase*6.2831853f));
            const float dx=wx*drift-wy*swirl,dy=wy*drift+wx*swirl;
            const float x=x0+dx,y=y0+dy,z=start+rise*age;
            const sector_t *at=sectorAt(x,y);
            if(!at||at->ceilingpic!=skyflatnum||z<=floorZ(at)+2.f||
               z>=ceilZ(at)-2.f)continue;
            bool blocked=false;
            for(int k=1;k<=5;++k) {
                const float u=float(k)/6.f;
                const sector_t *through=sectorAt(x0+dx*u,y0+dy*u);
                const float zz=start+rise*age*u;
                if(!through||through->ceilingpic!=skyflatnum||
                   zz<=floorZ(through)+2.f||zz>=ceilZ(through)-2.f) {blocked=true;break;}
            }
            if(blocked)continue;
            const float dist=std::hypot(x-camera.eye[0],y-camera.eye[1]);
            if(dist<15.f||dist>reach)continue;
            const float fade=std::clamp(std::min({age*1.4f,(life-age)*1.4f,
                (reach-dist)/90.f,(dist-15.f)/25.f}),0.f,1.f);
            if(fade<.03f)continue;
            const float hue=hashUnit(gx,gy,own^0x86u);
            const float light=std::clamp(lighting(at),.20f,1.0f);
            if(!soot) {
                // The original Fluid mask NSHSF0 (13) has a dense soft centre,
                // while NSHSA0 (16) provides wispy, feathered secondary edges.
                // World-cell position and all variation remain deterministic.
                // Build no weapon particles: append directly to the existing
                // alpha-masked texture batches, shared with Fluid effects.
                const std::array<float,3> gray={
                    (.23f+.10f*hue)*(.75f+.25f*light),
                    (.215f+.085f*hue)*(.75f+.25f*light),
                    (.20f+.075f*hue)*(.75f+.25f*light)};
                const float opacity=fade*(.35f+.18f*intensity);
                // Clamp the textured quad to the local sky-sector clearance
                // rather than allowing the soft sprite to cross a low roof.
                const float clearance=std::min(z-floorZ(at),ceilZ(at)-z)-3.5f;
                const float height=std::min(clearance,
                    (5.4f+2.8f*hashUnit(gx,gy,own^0x29f17u))*(.82f+.045f*age));
                if(height>1.5f) {
                    const float radius=height*(1.08f+.25f*hue);
                    const float roll=(hashUnit(gx,gy,own^0x7d912u)-.5f)*.60f;
                    fluidWeatherSmoke(camera,x,y,z,radius,height,gray,opacity,roll,13);
                    // A smaller displaced lobe makes the silhouette irregular.
                    // The lobe remains inside the outdoor sector if the wind
                    // drifts it near a wall or a covered neighboring sector.
                    const float side=(hashUnit(gx,gy,own^0x47b2u)-.5f)*radius*.72f;
                    const float px=x-wy*side,py=y+wx*side;
                    const float offsetZ=height*.20f;
                    const sector_t *wispy=sectorAt(px,py);
                    if(wispy&&wispy->ceilingpic==skyflatnum) {
                        const float wispyClearance=std::min(
                            z+offsetZ-floorZ(wispy),ceilZ(wispy)-z-offsetZ)-3.5f;
                        const float smallHeight=std::min(height*.75f,wispyClearance);
                        if(smallHeight>1.5f) {
                            const std::array<float,3> wispTint={
                                gray[0]*.88f,gray[1]*.90f,gray[2]*.92f};
                            fluidWeatherSmoke(camera,px,py,z+offsetZ,
                                radius*.79f,smallHeight,wispTint,opacity*.66f,
                                -roll*.67f,16);
                        }
                    }
                }
                ++smoke;
            } else {
                const std::array<float,3> color=ember?
                    std::array<float,3>{.87f+.10f*hue,.28f+.17f*hue,.05f+.07f*hue}:
                    std::array<float,3>{.23f+.08f*hue,.20f+.07f*hue,.18f+.05f*hue};
                pixelBlock(particleVertices,camera,x,y,z,ember?.55f:.75f,
                    ember?.80f:1.1f,color,false,nullptr,false,nullptr,
                    fade*(ember?.72f:.54f));
                ++ash;
            }
        }
    }
}

// E3 — Blood Hell: atmospheric *blood-colored* droplets plus cold embers.
// Droplets vanish at the floor; they never use buildRain(), splash(), blood
// decals, wetNow, glassWetNow or any gameplay blood pool / damage code.
// Geometry <= 3*bloodRainBudget + bloodEmberBudget (392 at High).
static void buildBloodHellAtmosphere(const Uniforms &camera) {
    if(camera.effects[2]!=0||levelSerial!=r_levelserial||seamCells.empty())return;
    constexpr float cell=30.f,reach=455.f;
    const int density=std::clamp(settings.episodeParticles,0,2);
    const int dropCap=doom_episode::bloodRainBudget(density);
    const int emberCap=doom_episode::bloodEmberBudget(density);
    const float now=gameSeconds();
    const float speed=std::hypot(rainWind[0],rainWind[1]);
    const float wx=speed>1.f?rainWind[0]/speed:1.f;
    const float wy=speed>1.f?rainWind[1]/speed:0.f;
    const float intensity=std::clamp(bloodHellMistNow,0.f,1.f);
    const uint32_t salt=weatherSeed^0xbd19f8a3u;
    const int cx=int(std::floor(camera.eye[0]/cell));
    const int cy=int(std::floor(camera.eye[1]/cell));
    int drops=0,embers=0;
    for(int ring=0;ring<=16&&(drops<dropCap||embers<emberCap);++ring)
    for(int gy=cy-ring;gy<=cy+ring&&(drops<dropCap||embers<emberCap);++gy)
    for(int gx=cx-ring;gx<=cx+ring&&(drops<dropCap||embers<emberCap);++gx) {
        if(std::max(std::abs(gx-cx),std::abs(gy-cy))!=ring)continue;
        const uint32_t seed=hash32(uint32_t(gx)*0x17e9c1fbu^
                                    uint32_t(gy)*0x4cb29a37u^salt);
        for(int kind=0;kind<2;++kind) {
            const bool ember=kind!=0;
            if(ember?embers>=emberCap:drops>=dropCap)continue;
            if(hashUnit(gx,gy,seed^(ember?0x154u:0x276u))>
               (ember?.18f:.66f+.18f*intensity))continue;
            const float life=ember?4.2f:2.1f;
            const float phase=hashUnit(gx,gy,seed^0x4d1u);
            const float clock=now+phase*life;
            const int cycle=int(std::floor(clock/life));
            const float age=clock-float(cycle)*life;
            const uint32_t own=hash32(seed^uint32_t(cycle)*0x9e3779b9u);
            const float x0=(gx+.13f+.74f*hashUnit(gx,gy,own^0x386u))*cell;
            const float y0=(gy+.13f+.74f*hashUnit(gx,gy,own^0x497u))*cell;
            const sector_t *origin=sectorAt(x0,y0);
            if(!origin||origin->ceilingpic!=skyflatnum)continue;
            const float floor=floorZ(origin),ceiling=ceilZ(origin);
            const float height=ceiling-floor;
            if(height<25.f)continue;
            const float start=floor+std::min(height-5.f,10.f+height*
                (.30f+.56f*hashUnit(gx,gy,own^0x508u)));
            const float vz=ember?5.f+7.f*hashUnit(gx,gy,own^0x619u):
                -(100.f+70.f*hashUnit(gx,gy,own^0x72au));
            const float drift=(ember?15.f:17.f)*age;
            const float swirl=3.4f*(std::sin(age*1.7f+phase*6.2831853f)-
                                     std::sin(phase*6.2831853f));
            const float dx=wx*drift-wy*swirl,dy=wy*drift+wx*swirl;
            const float x=x0+dx,y=y0+dy,z=start+vz*age;
            const sector_t *at=sectorAt(x,y);
            if(!at||at->ceilingpic!=skyflatnum||z<=floorZ(at)+1.f||
               z>=ceilZ(at)-2.f)continue;
            bool sheltered=false;
            for(int k=1;k<=5;++k) {
                const float u=float(k)/6.f;
                const sector_t *through=sectorAt(x0+dx*u,y0+dy*u);
                const float zz=start+vz*age*u;
                if(!through||through->ceilingpic!=skyflatnum||
                   zz<=floorZ(through)+1.f||zz>=ceilZ(through)-2.f) {sheltered=true;break;}
            }
            if(sheltered)continue;
            const float dist=std::hypot(x-camera.eye[0],y-camera.eye[1]);
            if(dist<11.f||dist>reach)continue;
            const float fade=std::clamp(std::min({age*2.2f,(life-age)*2.2f,
                (reach-dist)/80.f,(dist-11.f)/19.f}),0.f,1.f);
            if(fade<.03f)continue;
            const float light=std::clamp(lighting(at),.23f,1.f);
            const float variation=hashUnit(gx,gy,own^0x83bu);
            if(ember) {
                const std::array<float,3> color={.83f+.15f*variation,
                    .18f+.13f*variation,.07f+.045f*variation};
                pixelBlock(particleVertices,camera,x,y,z,.44f,.72f,color,
                    false,nullptr,false,nullptr,fade*.70f);
                ++embers;
            } else {
                // Dark crimson drips, two short separated segments.
                const std::array<float,3> red={(.43f+.22f*variation)*light,
                    (.055f+.07f*variation)*light,
                    (.060f+.065f*variation)*light};
                pixelBlock(particleVertices,camera,x,y,z,.73f,2.9f,red,
                    false,nullptr,false,nullptr,fade*.86f);
                pixelBlock(particleVertices,camera,x-wx*.75f,y-wy*.75f,z+2.f,
                    .48f,1.8f,red,false,nullptr,false,nullptr,fade*.43f);
                // Tiny transient droplet break-up at ground level, not a decal.
                if(z-floorZ(at)<10.f)
                    pixelBlock(particleVertices,camera,x,y,floorZ(at)+1.1f,
                        1.25f,.55f,red,false,nullptr,false,nullptr,fade*.42f);
                ++drops;
            }
        }
    }
}

// D1: Inferno particles are ephemeral analytic billboards, not persistent actors.
// The original rain, dripping-window effects and wetness remain unchanged.
// One candidate per 48-unit world cell; hard caps bound all generated geometry.
// Each current position and its swept path must be under an actual sky ceiling.
void buildEpisodeAtmosphere(const Uniforms &camera) {
    if(episodeAtmosphereNow==int(doom_episode::Profile::Starport)) {
        buildStarportAtmosphere(camera);return;
    }
    if(episodeAtmosphereNow==int(doom_episode::Profile::BurningCity)) {
        buildBurningCityAtmosphere(camera);return;
    }
    if(episodeAtmosphereNow==int(doom_episode::Profile::BloodHell)) {
        buildBloodHellAtmosphere(camera);return;
    }
    if(episodeAtmosphereNow==int(doom_episode::Profile::Deimos)) {
        buildDeimosAtmosphere(camera);return;
    }
    if(episodeAtmosphereNow!=int(doom_episode::Profile::Inferno)||
       camera.effects[2]!=0||levelSerial!=r_levelserial||seamCells.empty())return;
    constexpr float cell=48.f,reach=390.f;
    const int density=std::clamp(settings.episodeParticles,0,2);
    const int ashLimit=doom_episode::ashBudget(density);
    const int emberLimit=doom_episode::emberBudget(density);
    const float now=gameSeconds();
    const float windLength=std::hypot(rainWind[0],rainWind[1]);
    const float wx=windLength>1.f?rainWind[0]/windLength:1.f;
    const float wy=windLength>1.f?rainWind[1]/windLength:0.f;
    const uint32_t salt=weatherSeed^0x4a71f2e9u;
    const int cx=(int)std::floor(camera.eye[0]/cell);
    const int cy=(int)std::floor(camera.eye[1]/cell);
    int ash=0,embers=0;
    for(int gy=cy-9;gy<=cy+9&&(ash<ashLimit||embers<emberLimit);++gy)
    for(int gx=cx-9;gx<=cx+9&&(ash<ashLimit||embers<emberLimit);++gx) {
        const uint32_t seed=hash32(uint32_t(gx)*0x632be59bu^
                                   uint32_t(gy)*0x85157af5u^salt);
        for(int kind=0;kind<2;++kind) {
            const bool ember=kind!=0;
            if(ember?embers>=emberLimit:ash>=ashLimit)continue;
            const float selection=hashUnit(gx,gy,seed^(ember?0x874c13u:0x96e72bu));
            if(selection>(ember?0.20f:0.65f))continue;
            const float life=ember?5.5f:7.0f;
            const float phase=hashUnit(gx,gy,seed^0x1c74d35bu);
            const float clock=now+phase*life;
            const int cycle=(int)std::floor(clock/life);
            const float age=clock-float(cycle)*life;
            const uint32_t own=hash32(seed^uint32_t(cycle)*0x9e3779b9u);
            const float x0=(gx+0.10f+0.80f*hashUnit(gx,gy,own^0x18u))*cell;
            const float y0=(gy+0.10f+0.80f*hashUnit(gx,gy,own^0x29u))*cell;
            const sector_t *origin=sectorAt(x0,y0);
            if(!origin||origin->ceilingpic!=skyflatnum)continue;
            const float floor=floorZ(origin),ceiling=ceilZ(origin);
            const float height=ceiling-floor;
            if(height<16.f)continue;
            const float start=floor+std::min(height-5.f,8.f+height*
                (0.10f+0.65f*hashUnit(gx,gy,own^0x4bu)));
            const float velocity=ember?(6.f+6.f*hashUnit(gx,gy,own^0x5cu)):
                -(5.f+6.f*hashUnit(gx,gy,own^0x6du));
            const float speed=ember?7.f:12.f;
            const float turbulence=4.f*std::sin(age*1.4f+phase*6.2831853f);
            const float deltaX=wx*(speed*age)-wy*turbulence;
            const float deltaY=wy*(speed*age)+wx*turbulence;
            const float x=x0+deltaX,y=y0+deltaY,z=start+velocity*age;
            const sector_t *at=sectorAt(x,y);
            if(!at||at->ceilingpic!=skyflatnum||z<=floorZ(at)+2.f||
               z>=ceilZ(at)-2.f)continue;
            // Short conservative sweep: no particles traveling through rooms.
            bool blocked=false;
            for(int step=1;step<4;++step) {
                const float u=float(step)/4.f;
                const sector_t *through=sectorAt(x0+deltaX*u,y0+deltaY*u);
                const float hz=start+velocity*age*u;
                if(!through||through->ceilingpic!=skyflatnum||
                   hz<=floorZ(through)+2.f||hz>=ceilZ(through)-2.f) {
                    blocked=true;break;
                }
            }
            if(blocked)continue;
            const float dist=std::hypot(x-camera.eye[0],y-camera.eye[1]);
            if(dist<14.f||dist>reach)continue;
            const float fade=std::clamp(std::min({age*1.6f,(life-age)*1.6f,
                         (reach-dist)/70.f,(dist-14.f)/25.f}),0.f,1.f);
            if(fade<0.025f)continue;
            const float light=std::clamp(lighting(at),0.22f,1.1f);
            const float hue=hashUnit(gx,gy,own^0x7eu);
            std::array<float,3> color=ember?
                std::array<float,3>{.76f+.19f*hue,.19f+.21f*hue,.035f+.07f*hue}:
                std::array<float,3>{.19f+.11f*hue,.13f+.08f*hue,.12f+.06f*hue};
            for(float &c:color)c*=ember?(.74f+.26f*light):(.50f+.50f*light);
            pixelBlock(particleVertices,camera,x,y,z,
                       ember?.48f:.85f,ember?.75f:.93f,color,
                       false,nullptr,false,nullptr,fade*(ember?.83f:.57f));
            if(ember)++embers;else ++ash;
        }
    }
}

// D3.2: both vanilla dead-tree sprites, prioritized close thunder, and
// bounded embers. The 20% ignition decision remains deterministic, and
// a finished fire leaves a cached charcoal-colored tree sprite.
void buildInfernoTreeStrikes(const Uniforms &camera) {
    if((episodeAtmosphereNow!=int(doom_episode::Profile::Inferno)&&
        episodeAtmosphereNow!=int(doom_episode::Profile::BloodHell))||
       !settings.infernoTreeStrikes||!settings.episodeLightning||!settings.stormBolts||
       !settings.stormMode||camera.effects[2]!=0||levelSerial!=r_levelserial)return;
    const bool strikeVisible=treeStrike.born>=0&&
        std::hypot(camera.eye[0]-treeStrike.x,camera.eye[1]-treeStrike.y)<=
        doom_strike::maxTargetDistance+100.f;
    if(strikeVisible) {
        const int age=leveltime-treeStrike.born;
        const float fade=doom_strike::impactFade(age);
        if(fade>0.f) {
            const float ground=treeStrike.ground;
            const std::uint32_t seed=treeStrike.seed;
            const std::array<float,3> outer={.96f,.15f,.05f};
            const std::array<float,3> core={1.f,.72f,.44f};
            if(doom_strike::drawBolt(age)) {
                // The user's reduced-flash preference also limits the local
                // glow. Geometry is thicker, but sky brightness is unchanged.
                float light=std::clamp(stormFlashNow*2.8f+.20f,0.f,.94f);
                if(!settings.stormStrongFlashes)light=std::min(light,.22f);
                const float height=treeStrike.top-ground;
                const float step=height/float(doom_strike::boltSegments);
                for(int i=0;i<doom_strike::boltSegments;++i) {
                    const float u=(i+.5f)/float(doom_strike::boltSegments);
                    const float dx=doom_strike::lateral(i,doom_strike::boltSegments,seed);
                    const float dy=doom_strike::lateral(i,doom_strike::boltSegments,seed^0xb941d15fu);
                    const float x=treeStrike.x+dx,y=treeStrike.y+dy;
                    const float z=treeStrike.top-height*u;
                    pixelBlock(particleVertices,camera,x,y,z,3.8f,step/2.f,
                               outer,false,nullptr,false,nullptr,.62f*light);
                    pixelBlock(particleVertices,camera,x,y,z,1.55f,step/1.20f,
                               core,false,nullptr,false,nullptr,.94f*light);
                }
                for(int b=0;b<2;++b)for(int i=0;i<doom_strike::branchSegments;++i) {
                    const float u=(i+.5f)/float(doom_strike::branchSegments);
                    const float mainT=(b==0?.22f:.52f)+u*.26f;
                    const int mainIndex=int(mainT*doom_strike::boltSegments);
                    const float mainDx=doom_strike::lateral(mainIndex,doom_strike::boltSegments,seed);
                    const float mainDy=doom_strike::lateral(mainIndex,doom_strike::boltSegments,seed^0xb941d15fu);
                    const float spread=(b==0?1.f:-1.f)*u*(16.f+7.f*doom_strike::unit(seed^0x47u));
                    const float x=treeStrike.x+mainDx+spread;
                    const float y=treeStrike.y+mainDy+u*9.f;
                    const float z=treeStrike.top-height*mainT;
                    pixelBlock(particleVertices,camera,x,y,z,1.45f,2.5f,
                               core,false,nullptr,false,nullptr,.78f*light*(1.f-u*.65f));
                }
                const std::array<float,3> halo={.92f,.16f,.045f};
                pixelBlock(particleVertices,camera,treeStrike.x,treeStrike.y,ground+4.f,
                           13.f,.55f,halo,false,nullptr,false,nullptr,.34f*light);
            }
            const float seconds=float(std::max(age,0))/float(TICRATE);
            for(int n=0;n<doom_strike::emberCount;++n) {
                const float phase=6.28318530718f*doom_strike::unit(seed+
                    std::uint32_t(n)*0x9e3779b9u);
                const float speed=12.f+24.f*doom_strike::unit(seed^
                    (std::uint32_t(n)*0x5a821b9du));
                const float x=treeStrike.x+std::cos(phase)*speed*seconds;
                const float y=treeStrike.y+std::sin(phase)*speed*seconds;
                const float z=ground+2.f+seconds*(9.f+18.f*doom_strike::unit(seed+
                    std::uint32_t(n)*0x3c6ef372u));
                const std::array<float,3> tint=n%3==0?
                    std::array<float,3>{1.f,.56f,.13f}:
                    std::array<float,3>{.84f,.20f,.035f};
                pixelBlock(particleVertices,camera,x,y,z,.62f,1.0f,tint,
                           false,nullptr,false,nullptr,.72f*fade);
            }
        }
    }
    // Up to four independent fires. Billboards never affect actor health,
    // map geometry or sound; each fire expires after 24 simulation seconds.
    for(const BurningTree &fire:treeFires) {
        if(!doom_strike::fireActive(fire.born,leveltime))continue;
        if(std::hypot(camera.eye[0]-fire.x,camera.eye[1]-fire.y)>
           doom_strike::maxTargetDistance+100.f)continue;
        const float seconds=float(leveltime-fire.born)/float(TICRATE);
        const float fade=doom_strike::fireFade(fire.born,leveltime);
        const float flicker=.75f+.25f*std::sin(seconds*9.0f+
            6.2831853f*doom_strike::unit(fire.seed));
        for(int n=0;n<doom_strike::flameStrands;++n) {
            const float seedPhase=6.2831853f*doom_strike::unit(fire.seed+
                std::uint32_t(n)*0x32c91u);
            const float phase=seedPhase+seconds*(2.2f+.15f*n);
            const float radius=3.5f+2.f*doom_strike::unit(fire.seed^
                std::uint32_t(n)*0x3a1c15u);
            const float x=fire.x+radius*std::cos(phase);
            const float y=fire.y+radius*std::sin(phase);
            const float height=fire.height*(.30f+.55f*doom_strike::unit(
                fire.seed+std::uint32_t(n)*0x76d45u));
            const float z=fire.ground+12.f+height*.52f+
                3.4f*std::sin(seconds*6.8f+seedPhase);
            const std::array<float,3> red={.96f,.19f,.035f};
            const std::array<float,3> orange={1.f,.49f,.095f};
            pixelBlock(particleVertices,camera,x,y,z,3.f,height*.80f,
                       red,false,nullptr,false,nullptr,.46f*fade*flicker);
            pixelBlock(particleVertices,camera,x,y,z,1.45f,height*.57f,
                       orange,false,nullptr,false,nullptr,.72f*fade*flicker);
        }
        for(int n=0;n<doom_strike::flameSparks;++n) {
            const float phase=6.2831853f*doom_strike::unit(fire.seed+
                std::uint32_t(n)*0x6ac17u);
            const float t=std::fmod(seconds*(.7f+.1f*n)+
                doom_strike::unit(fire.seed^std::uint32_t(n)*0x1649b5u),1.f);
            const float x=fire.x+(3.f+9.f*t)*std::cos(phase);
            const float y=fire.y+(3.f+9.f*t)*std::sin(phase);
            const float z=fire.ground+12.f+fire.height*(.3f+.9f*t);
            const std::array<float,3> gold={1.f,.66f,.19f};
            pixelBlock(particleVertices,camera,x,y,z,.57f,.90f,gold,
                       false,nullptr,false,nullptr,.65f*fade*(1.f-t));
        }
        // D3.2: low, slow orange-red coals orbit and drift away from the
        // burning branches, at different heights and phases. Pure geometry.
        for(int n=0;n<doom_strike::fireEmberMotes;++n) {
            const float offset=doom_strike::unit(fire.seed+
                std::uint32_t(n)*0x7feb352du);
            const float t=std::fmod(seconds*(.29f+.045f*(n%4))+offset,1.f);
            const float a=6.2831853f*doom_strike::unit(fire.seed^
                (std::uint32_t(n)*0x846ca68bu))+.55f*seconds;
            const float radius=2.5f+13.f*t;
            const float x=fire.x+radius*std::cos(a);
            const float y=fire.y+radius*std::sin(a);
            const float z=fire.ground+5.f+fire.height*(.15f+.95f*t);
            const std::array<float,3> ember=n%3==0?
                std::array<float,3>{1.f,.60f,.20f}:
                std::array<float,3>{.88f,.28f,.065f};
            pixelBlock(particleVertices,camera,x,y,z,.38f,.60f,ember,
                       false,nullptr,false,nullptr,.73f*fade*(1.f-t));
        }
    }
}
void weatherShutdown() {
    rainNow=wetNow=glassWetNow=stormFlashNow=stormBoltNow=stormBoltAzimuthNow=stormBoltSeedNow=0;
    stormCurtainNow=stormHazeNow=0;
    deimosBlizzardNow=deimosGustNow=0;
    starportDustNow=starportGustNow=0;
    citySmokeNow=bloodHellMistNow=0;
    episodeAtmosphereNow=0;
    soak.clear();weatherLevelSerial=~0u;soakPaneSerial=~0u;
    doom_storm::reset(stormState);treeStrike=TreeStrike{};treeFires={};
    charredTrees={};charredCount=0;treeHistory={};historyCount=0;strikeOrder=0;
    treeCloseThunder={};
    storm_audio::stop();I_AmbienceRain(0);
}
} // namespace doom3d

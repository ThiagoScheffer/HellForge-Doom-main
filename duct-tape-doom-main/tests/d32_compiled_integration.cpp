#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "lightning_interactions.h"
namespace doom3d {
constexpr float doomPi=3.14159265358979323846f;
constexpr int MT_MISC75=75, MT_MISC76=76, SPR_TRE1=51, SPR_TRE2=52, GS_LEVEL=1, TICRATE=35;
struct sector_t;
struct subsector_t {sector_t *sector=nullptr;};
struct mobj_t {int type=0,sprite=0,x=0,y=0,z=0;unsigned angle=0;const mobj_t *snext=nullptr;subsector_t *subsector=nullptr;};
struct sector_t {int ceilingpic=0;const mobj_t *thinglist=nullptr;};
struct Player {const mobj_t *mo=nullptr;};
struct Settings {int infernoTreeStrikes=1,episodeLightning=1,stormBolts=1,stormMode=3,accelerated=1,stormStrongFlashes=1;} settings;
struct Uniforms {float eye[4]={},effects[4]={};};
struct TreeStrike {float x=0,y=0,ground=0,top=0;int born=-1,sector=-1,type=-1,sprite=-1;std::uint32_t seed=0;};
struct BurningTree {float x=0,y=0,ground=0,height=0;int born=-1,sector=-1,type=-1,sprite=-1;std::uint32_t seed=0;};
struct CharredTree {float x=0,y=0;int sector=-1,type=-1,sprite=-1;};
TreeStrike treeStrike;
std::array<BurningTree,doom_strike::maxBurningTrees> treeFires{};
std::array<CharredTree,doom_strike::maxCharredTrees> charredTrees{};
int charredCount=0;
doom_strike::CloseThunder treeCloseThunder{};
int gamestate=GS_LEVEL,displayplayer=0,numsectors=2,skyflatnum=42;
int episodeAtmosphereNow=1,leveltime=50,levelSerial=3,r_levelserial=3;
float stormFlashNow=.43f;
sector_t sectors[2];subsector_t subsectors[2];mobj_t trees[3],viewer;
Player players[1];
std::vector<int> particleVertices;
float units(int n){return n/65536.f;}
float ceilZ(const sector_t*) {return 200.f;}
void pixelBlock(std::vector<int>&out,const Uniforms&,float,float,float,float size,float tall,
                const std::array<float,3>&,bool,const std::array<float,3>*,bool,void*,float alpha) {
    if(!(size>0 && tall>0 && alpha>=0 && alpha<=1.0001f))std::abort();
    out.insert(out.end(),{1,2,3,4,5,6});
}
namespace doom_episode {enum class Profile:int {None=0,Inferno=1,Deimos=2};}

void chooseTreeStrike(std::uint32_t eventSeed) {
    treeStrike=TreeStrike{};
    if(!doom_strike::selectEvent(eventSeed)||gamestate!=GS_LEVEL||
       !players[displayplayer].mo)return;
    const mobj_t *viewer=players[displayplayer].mo;
    const float vx=units(viewer->x),vy=units(viewer->y);
    const float angle=float(double(viewer->angle)*(2.0*doomPi/4294967296.0));
    const float fx=std::cos(angle),fy=std::sin(angle);
    float best=2.f;
    for(int s=0;s<numsectors;++s) {
        const sector_t *sector=&sectors[s];
        if(sector->ceilingpic!=skyflatnum)continue;
        for(const mobj_t *thing=sector->thinglist;thing;thing=thing->snext) {
            const bool small=thing->type==MT_MISC75&&thing->sprite==SPR_TRE1;
            if(!doom_strike::stockTree(thing->type,thing->sprite,
                    MT_MISC75,SPR_TRE1,MT_MISC76,SPR_TRE2))continue;
            const float x=units(thing->x),y=units(thing->y);
            const float dx=x-vx,dy=y-vy,dist=std::hypot(dx,dy);
            const float base=units(thing->z),ceil=ceilZ(sector);
            if(!doom_strike::eligibleTree(thing->type,thing->sprite,
                  small?MT_MISC75:MT_MISC76,small?SPR_TRE1:SPR_TRE2,
                  true,dist,ceil-base))continue;
            // Favor the player's forward field while allowing peripheral strikes.
            if(dx*fx+dy*fy<-.18f*dist)continue;
            // Current actor must really occupy this sky sector (not just a
            // stale sector-list position while moving or being destroyed).
            if(!thing->subsector||thing->subsector->sector!=sector)continue;
            const std::uint32_t h=doom_strike::hash(eventSeed^
                std::uint32_t(s)*0x9e3779b9u^
                std::uint32_t(thing->x)^std::uint32_t(thing->y));
            const float score=doom_strike::unit(h)*.65f+
                .35f*dist/doom_strike::maxTargetDistance;
            if(score>=best)continue;
            best=score;
            treeStrike.x=x;treeStrike.y=y;treeStrike.ground=base;
            treeStrike.top=std::min(ceil-5.f,base+
                (small?std::max(40.f,.48f*(ceil-base)):
                       std::max(55.f,.75f*(ceil-base))));
            treeStrike.seed=h;treeStrike.born=leveltime;
            treeStrike.sector=s;treeStrike.type=thing->type;treeStrike.sprite=thing->sprite;
        }
    }
}

void tryIgniteTree() {
    if(treeStrike.born<0||!doom_strike::ignite(treeStrike.seed))return;
    for(const BurningTree &fire:treeFires) {
        if(doom_strike::fireActive(fire.born,leveltime)&&
           fire.sector==treeStrike.sector&&
           std::hypot(fire.x-treeStrike.x,fire.y-treeStrike.y)<1.f)return;
    }
    for(BurningTree &fire:treeFires) {
        if(doom_strike::fireActive(fire.born,leveltime))continue;
        fire.x=treeStrike.x;fire.y=treeStrike.y;fire.ground=treeStrike.ground;
        const bool small=treeStrike.type==MT_MISC75;
        fire.height=small?std::clamp((treeStrike.top-treeStrike.ground)*.48f,22.f,46.f):
            std::clamp((treeStrike.top-treeStrike.ground)*.45f,36.f,76.f);
        fire.sector=treeStrike.sector;fire.born=leveltime;
        fire.type=treeStrike.type;fire.sprite=treeStrike.sprite;
        fire.seed=doom_strike::hash(treeStrike.seed^0x7589de31u);
        break;
    }
}

bool fireHasTree(const BurningTree &fire) {
    if(fire.sector<0||fire.sector>=numsectors)return false;
    const sector_t *sector=&sectors[fire.sector];
    if(sector->ceilingpic!=skyflatnum)return false;
    for(const mobj_t *thing=sector->thinglist;thing;thing=thing->snext)
        if(thing->type==fire.type&&thing->sprite==fire.sprite&&
           doom_strike::stockTree(thing->type,thing->sprite,
               MT_MISC75,SPR_TRE1,MT_MISC76,SPR_TRE2)&&
           thing->subsector&&thing->subsector->sector==sector&&
           std::abs(units(thing->x)-fire.x)<1.f&&
           std::abs(units(thing->y)-fire.y)<1.f)return true;
    return false;
}

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

bool infernoTreeCharred(const mobj_t *thing) {
    if(!thing||charredCount<=0||!settings.infernoTreeStrikes||
       episodeAtmosphereNow!=int(doom_episode::Profile::Inferno)||
       !doom_strike::stockTree(thing->type,thing->sprite,
           MT_MISC75,SPR_TRE1,MT_MISC76,SPR_TRE2)||
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

void buildInfernoTreeStrikes(const Uniforms &camera) {
    if(episodeAtmosphereNow!=int(doom_episode::Profile::Inferno)||
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

void tickFires() {
    for(BurningTree &fire:treeFires) {
        if(fire.born<0)continue;
        if(!doom_strike::fireActive(fire.born,leveltime)) {
            finishTreeFire(fire);
            fire=BurningTree{};
        } else if(leveltime%TICRATE==0&&!fireHasTree(fire))fire=BurningTree{};
    }
}
}
int main() {
using namespace doom3d;
auto require=[](bool ok,const char *name){if(!ok){std::fprintf(stderr,"FAILED: %s\n",name);std::exit(1);}};
require(doom_strike::stockTree(MT_MISC75,SPR_TRE1,MT_MISC75,SPR_TRE1,MT_MISC76,SPR_TRE2),"small stock tree eligible");
require(doom_strike::stockTree(MT_MISC76,SPR_TRE2,MT_MISC75,SPR_TRE1,MT_MISC76,SPR_TRE2),"large stock tree eligible");
require(!doom_strike::stockTree(MT_MISC75,SPR_TRE2,MT_MISC75,SPR_TRE1,MT_MISC76,SPR_TRE2),"mismatched pair rejected");
require(!doom_strike::eligibleTree(MT_MISC75,SPR_TRE1,MT_MISC75,SPR_TRE1,false,40,200),"indoor tree forbidden");
require(!doom_strike::eligibleTree(MT_MISC76,SPR_TRE2,MT_MISC76,SPR_TRE2,true,900,200),"distant tree forbidden");
int lit=0;for(unsigned seed=1;seed<=20000;++seed)lit+=doom_strike::ignite(seed);
require(lit>3600&&lit<4400,"20 percent deterministic ignition");
doom_strike::CloseThunder thunder{};
require(doom_strike::useOrdinaryThunder(thunder),"ordinary storms retain thunder");
doom_strike::scheduleClose(thunder,100,35);
require(!doom_strike::closeReady(thunder,109),"close thunder deferred");
require(!doom_strike::useOrdinaryThunder(thunder),"tree strike replaces normal thunder");
require(doom_strike::useOrdinaryThunder(thunder),"later storms use ordinary thunder");
require(doom_strike::closeReady(thunder,110),"strong recording ready after 10 tics");
require(!doom_strike::closeReady(thunder,111),"close sound never duplicates");
sectors[0].ceilingpic=skyflatnum;sectors[1].ceilingpic=skyflatnum;
subsectors[0].sector=&sectors[0];subsectors[1].sector=&sectors[1];
viewer.x=0;viewer.y=0;viewer.angle=0;players[0].mo=&viewer;
trees[0].type=MT_MISC75;trees[0].sprite=SPR_TRE1;trees[0].x=60*65536;trees[0].subsector=&subsectors[0];
trees[1].type=MT_MISC76;trees[1].sprite=SPR_TRE2;trees[1].x=120*65536;trees[1].subsector=&subsectors[0];
trees[0].snext=&trees[1];sectors[0].thinglist=&trees[0];
unsigned smallSeed=0,bigSeed=0,igniteSmall=0,igniteBig=0;
for(unsigned seed=1;seed<400000;++seed){
    chooseTreeStrike(seed);
    if(treeStrike.born<0)continue;
    if(treeStrike.type==MT_MISC75) {
        smallSeed=seed;
        if(doom_strike::ignite(treeStrike.seed))igniteSmall=seed;
    }
    if(treeStrike.type==MT_MISC76){
        bigSeed=seed;
        if(doom_strike::ignite(treeStrike.seed))igniteBig=seed;
    }
    if(smallSeed&&bigSeed&&igniteSmall&&igniteBig)break;
}
require(smallSeed&&bigSeed&&igniteSmall&&igniteBig,"both tree types are selected and burnable");
leveltime=50;chooseTreeStrike(smallSeed);
require(treeStrike.type==MT_MISC75&&treeStrike.sprite==SPR_TRE1,"small tree chosen");
require(treeStrike.top>treeStrike.ground+30,"small bolt reaches small tree height");
Uniforms camera{};particleVertices.clear();buildInfernoTreeStrikes(camera);
require(int(particleVertices.size()/6)==doom_strike::peakStrikeBlocks,"small tree complete strike geometry");
leveltime=50;chooseTreeStrike(igniteSmall);tryIgniteTree();
require(treeFires[0].born==50&&treeFires[0].type==MT_MISC75,"small tree ignited");
require(!infernoTreeCharred(&trees[0]),"no charcoal before burning finishes");
leveltime=50+doom_strike::emberLifetimeTics;
particleVertices.clear();buildInfernoTreeStrikes(camera);
require(int(particleVertices.size()/6)==doom_strike::flameStrands*2+doom_strike::flameSparks+doom_strike::fireEmberMotes,"fire has additional drifting coals");
leveltime=50+doom_strike::flameLifetimeTics;
tickFires();
require(charredCount==1&&infernoTreeCharred(&trees[0]),"small tree stays charred after fire");
require(!infernoTreeCharred(&trees[1]),"large fresh tree unchanged");
particleVertices.clear();buildInfernoTreeStrikes(camera);require(particleVertices.empty(),"no fire particles after expiry");
leveltime=1000;chooseTreeStrike(igniteBig);tryIgniteTree();
require(treeFires[0].born==1000&&treeFires[0].type==MT_MISC76,"large tree ignited");
leveltime=1000+doom_strike::flameLifetimeTics;tickFires();
require(charredCount==2&&infernoTreeCharred(&trees[1]),"large tree stays charred");
require(infernoTreeCharred(&trees[0]),"small charcoal persists concurrently");
settings.infernoTreeStrikes=0;require(!infernoTreeCharred(&trees[0]),"toggle hides cosmetic char");settings.infernoTreeStrikes=1;
episodeAtmosphereNow=int(doom_episode::Profile::Deimos);require(!infernoTreeCharred(&trees[0]),"Deimos unaffected");episodeAtmosphereNow=1;
trees[0].sprite=SPR_TRE2;require(!infernoTreeCharred(&trees[0]),"replaced tree no char");trees[0].sprite=SPR_TRE1;
sectors[0].ceilingpic=0;chooseTreeStrike(igniteSmall);require(treeStrike.born<0,"roof stops tree strikes");
require(doom_strike::maxFlameBlocks==4*(7*2+9+12),"updated particle budget");
std::printf("D3.2 integration passed; ignition=%d/20000; charred=%d; maxFlameBlocks=%d\n",lit,charredCount,doom_strike::maxFlameBlocks);
}

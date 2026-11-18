#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>
#include "lightning_interactions.h"
namespace doom3d {
constexpr float doomPi=3.14159265358979323846f;
constexpr int MT_MISC76=76,SPR_TRE2=54,GS_LEVEL=1,TICRATE=35;
struct sector_t;
struct subsector_t {sector_t *sector=nullptr;};
struct mobj_t {int type=0,sprite=0,x=0,y=0,z=0;unsigned angle=0;const mobj_t *snext=nullptr;subsector_t *subsector=nullptr;};
struct sector_t {int ceilingpic=0;const mobj_t *thinglist=nullptr;};
struct Player {const mobj_t *mo=nullptr;};
struct Settings {int infernoTreeStrikes=1,episodeLightning=1,stormBolts=1,stormMode=3,accelerated=1;} settings;
struct Uniforms {float eye[4]={},effects[4]={};};
struct TreeStrike {float x=0,y=0,ground=0,top=0;int born=-1;std::uint32_t seed=0;};
TreeStrike treeStrike;
int gamestate=GS_LEVEL,displayplayer=0,numsectors=1,skyflatnum=42;
int episodeAtmosphereNow=1,leveltime=50,levelSerial=3,r_levelserial=3;
float stormBoltNow=.65f;
sector_t sectors[2];subsector_t subsectors[2];mobj_t trees[3],viewer;
Player players[1];
std::vector<int> particleVertices;
float units(int n){return n/65536.f;}
float ceilZ(const sector_t*) {return 200.f;}
void pixelBlock(std::vector<int>&out,const Uniforms&,float,float,float,float size,float tall,
                const std::array<float,3>&,bool,const std::array<float,3>*,bool,void*,float alpha) {
    if(!(size>0 && tall>0 && alpha>=0 && alpha<=1.0001f))__builtin_trap();
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
            if(thing->type!=MT_MISC76||thing->sprite!=SPR_TRE2)continue;
            const float x=units(thing->x),y=units(thing->y);
            const float dx=x-vx,dy=y-vy,dist=std::hypot(dx,dy);
            const float base=units(thing->z),ceil=ceilZ(sector);
            if(!doom_strike::eligibleTree(thing->type,thing->sprite,
                  MT_MISC76,SPR_TRE2,true,dist,ceil-base))continue;
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
            treeStrike.top=std::min(ceil-5.f,base+std::max(55.f,.75f*(ceil-base)));
            treeStrike.seed=h;treeStrike.born=leveltime;
        }
    }
}
void buildInfernoTreeStrikes(const Uniforms &camera) {
    if(treeStrike.born<0||episodeAtmosphereNow!=int(doom_episode::Profile::Inferno)||
       !settings.infernoTreeStrikes||!settings.episodeLightning||!settings.stormBolts||
       !settings.stormMode||camera.effects[2]!=0||levelSerial!=r_levelserial)return;
    const int age=leveltime-treeStrike.born;
    const float fade=doom_strike::impactFade(age);
    if(fade<=0.f||std::hypot(camera.eye[0]-treeStrike.x,camera.eye[1]-treeStrike.y)>
       doom_strike::maxTargetDistance+100.f)return;
    const float ground=treeStrike.ground;
    const std::uint32_t seed=treeStrike.seed;
    const std::array<float,3> outer={.92f,.18f,.075f};
    const std::array<float,3> core={1.f,.63f,.33f};
    if(doom_strike::drawBolt(age)) {
        // Same flash limiter as sky lightning; extra local geometry is a
        // colored visual accent and does not illuminate indoor world meshes.
        const float light=std::clamp(stormBoltNow*1.8f,0.f,.88f);
        if(light>.02f) {
            const float height=treeStrike.top-ground;
            const float step=height/float(doom_strike::boltSegments);
            for(int i=0;i<doom_strike::boltSegments;++i) {
                const float u=(i+.5f)/float(doom_strike::boltSegments);
                const float dx=doom_strike::lateral(i,doom_strike::boltSegments,seed);
                const float dy=doom_strike::lateral(i,doom_strike::boltSegments,seed^0xb941d15fu);
                const float x=treeStrike.x+dx,y=treeStrike.y+dy;
                const float z=treeStrike.top-height*u;
                pixelBlock(particleVertices,camera,x,y,z,2.5f,step/2.5f,
                           outer,false,nullptr,false,nullptr,.42f*light);
                pixelBlock(particleVertices,camera,x,y,z,.95f,step/.95f,
                           core,false,nullptr,false,nullptr,.82f*light);
            }
            // Two short branches separate from the main bolt in opposite
            // directions, with no retained geometry or per-frame RNG calls.
            for(int b=0;b<2;++b)for(int i=0;i<doom_strike::branchSegments;++i) {
                const float u=(i+.5f)/float(doom_strike::branchSegments);
                const float mainT=(b==0?.27f:.54f)+u*.20f;
                const int mainIndex=int(mainT*doom_strike::boltSegments);
                const float mainDx=doom_strike::lateral(mainIndex,doom_strike::boltSegments,seed);
                const float mainDy=doom_strike::lateral(mainIndex,doom_strike::boltSegments,seed^0xb941d15fu);
                const float spread=(b==0?1.f:-1.f)*u*(12.f+4.f*doom_strike::unit(seed^0x47u));
                const float x=treeStrike.x+mainDx+spread;
                const float y=treeStrike.y+mainDy+u*7.f;
                const float z=treeStrike.top-height*mainT;
                pixelBlock(particleVertices,camera,x,y,z,1.f,2.1f,
                           core,false,nullptr,false,nullptr,.55f*light*(1.f-u*.7f));
            }
            // Brief localized impact halo at the burnt tree's base.
            const std::array<float,3> halo={.85f,.13f,.045f};
            pixelBlock(particleVertices,camera,treeStrike.x,treeStrike.y,ground+3.f,
                       10.f,.38f,halo,false,nullptr,false,nullptr,.28f*light);
        }
    }
    // Small, stable arc of embers lasting ~0.5 s after impact. No sound
    // duplication: thunder remains exclusively scheduled by Storm A/B.
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
            std::array<float,3>{1.f,.55f,.12f}:
            std::array<float,3>{.80f,.18f,.035f};
        pixelBlock(particleVertices,camera,x,y,z,.52f,.85f,tint,
                   false,nullptr,false,nullptr,.65f*fade);
    }
}
}
int main() {
    using namespace doom3d;
    sectors[0].ceilingpic=skyflatnum;
    subsectors[0].sector=&sectors[0];
    viewer.x=0;viewer.y=0;viewer.angle=0;
    players[0].mo=&viewer;
    trees[0].type=MT_MISC76;trees[0].sprite=SPR_TRE2;trees[0].x=60*65536;
    trees[0].subsector=&subsectors[0];sectors[0].thinglist=&trees[0];
    std::uint32_t chosen=0;
    for(std::uint32_t x=1;x<1000;++x)if(doom_strike::selectEvent(x)){chosen=x;break;}
    if(!chosen)return 1;
    chooseTreeStrike(chosen);
    if(treeStrike.born!=leveltime||treeStrike.top<60) {std::fprintf(stderr,"eligible tree not selected\n");return 2;}
    Uniforms c{};c.eye[0]=0;c.eye[1]=0;
    buildInfernoTreeStrikes(c);
    const int blocks=int(particleVertices.size()/6);
    if(blocks<50||blocks>2*doom_strike::boltSegments+2*doom_strike::branchSegments+doom_strike::emberCount+1) {
        std::fprintf(stderr,"unexpected render budget %d\n",blocks);return 3;
    }
    particleVertices.clear();leveltime+=8;
    buildInfernoTreeStrikes(c);
    if(int(particleVertices.size()/6)!=doom_strike::emberCount) return 4;
    particleVertices.clear();leveltime+=10;
    buildInfernoTreeStrikes(c);
    if(!particleVertices.empty())return 5;
    leveltime=50;settings.infernoTreeStrikes=0;particleVertices.clear();
    buildInfernoTreeStrikes(c);if(!particleVertices.empty())return 6;
    settings.infernoTreeStrikes=1;episodeAtmosphereNow=2;
    buildInfernoTreeStrikes(c);if(!particleVertices.empty())return 7;
    episodeAtmosphereNow=1;sectors[0].ceilingpic=0;
    chooseTreeStrike(chosen);if(treeStrike.born!=-1)return 8;
    sectors[0].ceilingpic=skyflatnum;trees[0].sprite=0;
    chooseTreeStrike(chosen);if(treeStrike.born!=-1)return 9;
    trees[0].sprite=SPR_TRE2;trees[0].x=900*65536;
    chooseTreeStrike(chosen);if(treeStrike.born!=-1)return 10;
    std::printf("D3 tests passed: tree selection, %d billboards, fade, options, episode gate, sky, sprite and distance checks\\n",blocks);
}

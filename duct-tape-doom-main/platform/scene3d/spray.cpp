/* Blood spray: what hits and kills throw. Drops fly as Doom-pixel specks
   and land as splats on floors, walls and ceilings; wounds drip; bodies
   sliding across the floor smear it, bodies thrown into walls splash them.
   Drawing only: own random numbers, nothing the game sees. */
#include "internal.h"
#include "gore_policy.h"
#include "../decoration_ambience_rules.h"
extern "C" {
#include "../audio/impact_synth.h"
#include "p_solid.h"
#include "../audio/sound_options.h"
}

namespace doom3d {
// A drop's flight is worked out once, when it is thrown: where it lands
// (the floor under it, a wall in its way, the ceiling it rises into) and
// when. Its splat is laid then, shown from that moment, and the speck flies
// until it gets there, moving at the tic rate like the debris.
constexpr float sprayGravity=1225; // Doom's: a unit a tic, each tic.
enum class DropVisual : uint8_t { Blood, GoreBlood, GoreDrip, Flesh, Tissue, Organ, BlueGut, Bone };
struct Drop {
    float x,y,z,vx,vy,vz,born,end,size,gravity,flight;
    std::array<float,3> color;
    bool decoration=false;
    DropVisual visual=DropVisual::Blood;
    LiquidMaterial material=LiquidMaterial::Blood;
};
static std::vector<Drop> drops;
constexpr size_t maxDrops=768;
// Dying and dead bodies watched for sliding (smears) and stopping short (a wall).
struct Slide { float x,y,speed,dx,dy,since;bool slammed,seen; };
static std::unordered_map<const mobj_t*,Slide> slides;
static unsigned sprayLevelSerial;
static unsigned goreEventOrdinal;

static bool throwDrop(float,float,float,float,float,float,float,float,int,int*,const uint32_t*,DropVisual,float,int*,LiquidMaterial);
struct AcidCoating {float localYaw;int born,next;uint32_t seed;};
static std::unordered_map<const mobj_t*,AcidCoating> acidCoatings;
void forgetAcidActor(const mobj_t *actor) {acidCoatings.erase(actor);}
void clearAcidVisuals() {
    acidCoatings.clear();
    drops.erase(std::remove_if(drops.begin(),drops.end(),[](const Drop &d){return d.material==LiquidMaterial::Acid;}),drops.end());
    clearAcidMarks();
}
void coatAcidActor(mobj_t *actor,fixed_t x,fixed_t y) {
    if(!acidLiquidOn()||!settings.acidMonsterCoating||!actor||actor->player||
       !(actor->flags&MF_SHOOTABLE)||!actor->info||
       (!(actor->info->flags&MF_COUNTKILL)&&Game_ThingRole(actor->type)!=GAME_THING_UNCOUNTED_MONSTER))return;
    if(acidCoatings.size()>=24&&!acidCoatings.count(actor))return;
    float facing=float(actor->angle*(2*doomPi/4294967296.0));
    float direction=std::atan2(units(y)-units(actor->y),units(x)-units(actor->x));
    uint32_t seed=hash32((uint32_t)actor->x^(uint32_t)actor->y^(uint32_t)leveltime);
    acidCoatings[actor]={direction-facing,leveltime,leveltime+12+int(seed%13),seed};
}
bool acidCoatingPosition(const mobj_t *actor,float eyeX,float eyeY,float &u,float &v) {
    if(!acidLiquidOn()||!settings.acidMonsterCoating)return false;
    auto it=acidCoatings.find(actor);if(it==acidCoatings.end()||leveltime-it->second.born>=10*TICRATE)return false;
    float facing=float(actor->angle*(2*doomPi/4294967296.0))+it->second.localYaw;
    float view=std::atan2(eyeY-units(actor->y),eyeX-units(actor->x));
    float delta=view-facing;if(std::cos(delta)<.1f)return false;
    u=.5f+.22f*std::sin(delta);v=.42f;return true;
}
static void tickAcidCoatings() {
    if(!acidLiquidOn()||!settings.acidMonsterCoating){acidCoatings.clear();return;}
    for(auto it=acidCoatings.begin();it!=acidCoatings.end();) {
        auto &c=it->second;const mobj_t *actor=it->first;
        if(leveltime<c.born||leveltime-c.born>=10*TICRATE){it=acidCoatings.erase(it);continue;}
        if(leveltime>=c.next&&leveltime-c.born<6*TICRATE) {
            float a=float(actor->angle*(2*doomPi/4294967296.0))+c.localYaw;
            float x=units(actor->x)+std::cos(a)*units(actor->radius)*.7f;
            float y=units(actor->y)+std::sin(a)*units(actor->radius)*.7f;
            float z=units(actor->z)+units(actor->height)*.55f;
            auto saved=impactRandom;impactRandom.seed(c.seed?c.seed:1);
            BloodSourceScope tint(LIQUID_TINT_ACID);
            throwDrop(x,y,z,units(actor->momx)*TICRATE,units(actor->momy)*TICRATE,0,1.f,4.f,BLOOD_SPECK,nullptr,nullptr,DropVisual::Blood,sprayGravity,nullptr,LiquidMaterial::Acid);
            impactRandom=saved;c.seed=hash32(c.seed+1);c.next=leveltime+12+int(c.seed%13);
        }
        ++it;
    }
}
static void checkSprayLevel() {
    if(sprayLevelSerial==r_levelserial)return;
    clearAcidVisuals();drops.clear();slides.clear();clearGiblets();goreEventOrdinal=0;sprayLevelSerial=r_levelserial;
}
static const sector_t *sectorAt(float x,float y) {return R_PointInSubsector((fixed_t)(x*FRACUNIT),(fixed_t)(y*FRACUNIT))->sector;}
static bool bleeds(const mobj_t &thing) {
    if(!thing.info||(thing.flags&MF_NOBLOOD))return false;
    return (thing.info->flags&MF_COUNTKILL)||Game_ThingRole(thing.type)==GAME_THING_UNCOUNTED_MONSTER;
}
static bool sprayOn() {return settings.bloodSpray&&bloodOn();}

// Throws a drop of size (its speck, units) from (x,y,z) at (vx,vy,vz) units
// a second; splat: the size of what it leaves (0 none).
static bool throwDrop(float x,float y,float z,float vx,float vy,float vz,float size,float splat,int kind,int *wallBudget=nullptr,
                      const uint32_t *decorationSeed=nullptr,DropVisual visual=DropVisual::Blood,float gravity=sprayGravity,
                      int *flatBudget=nullptr,LiquidMaterial material=LiquidMaterial::Blood) {
    float now=decorationSeed?leveltime/float(TICRATE):gameSeconds();
    const sector_t *start=sectorAt(x,y);
    if(ceilZ(start)-floorZ(start)<1.f)return false;
    z=std::clamp(z,floorZ(start)+0.5f,ceilZ(start)-0.5f);
    auto fallTime=[&](float floor) {
        float d=z-floor;
        return d<0?-1.0f:(vz+std::sqrt(vz*vz+2*gravity*d))/gravity;
    };
    // The floor: the start's, then the landing point's (a step's face in
    // between is found as a wall below).
    float t=fallTime(floorZ(start));
    const sector_t *landing=start;
    for(int pass=0;pass<2;++pass) {
        const sector_t *s=sectorAt(x+vx*t,y+vy*t);
        if(s==landing)break;
        float again=fallTime(floorZ(s));
        if(again<0)break;
        landing=s;t=again;
    }
    t=std::max(t,0.03f);
    enum { FLOOR,WALL,CEILING } where=FLOOR;
    // The ceiling, when it rises that high first.
    if(vz>0&&start->ceilingpic!=skyflatnum) {
        float rise=ceilZ(start)-0.5f-z;
        if(rise>0&&vz*vz>2*gravity*rise) {
            float up=(vz-std::sqrt(vz*vz-2*gravity*rise))/gravity;
            const sector_t *above=sectorAt(x+vx*up,y+vy*up);
            if(up<t&&std::abs(ceilZ(above)-ceilZ(start))<1) {t=up;where=CEILING;landing=above;}
        }
    }
    // A wall in the way, along the chord of its arc.
    float h=std::hypot(vx,vy);WallHit hit;
    if(h>1) {
        const int segments=visual>=DropVisual::Tissue?4:1;
        const float step=t/segments;
        const auto height=[&](float at){return z+vz*at-.5f*gravity*at*at;};
        for(int i=0;i<segments;++i) {
            const float a=step*i,reach=h*step,za=height(a),zb=height(a+step);
            if(firstSolidWall(x+vx*a,y+vy*a,za,vx/h,vy/h,(zb-za)/reach,reach,hit)) {
                t=a+hit.t/h;where=WALL;break;
            }
        }
    }
    if(t>2.5f)splat=0; // Down a pit.
    float lx=x+vx*t,ly=y+vy*t,lz=z+vz*t-0.5f*gravity*t*t,fall=vz-gravity*t;
    const bool canRest=visual==DropVisual::Bone&&size>=3.f&&where==FLOOR;
    const sector_t *endSector=where==CEILING?landing:
        (where==FLOOR&&(splat>0||canRest))?sectorAt(lx,ly):nullptr;
    const size_t endIndex=endSector?endSector-sectors:size_t(numsectors);
    const bool liquid=endIndex<sectorMist.size()&&sectorMist[endIndex].liquid;
    const bool restBone=canRest&&
                        endIndex<(size_t)numsectors&&endSector->floorpic!=skyflatnum&&!liquid;
    bool floorSplat=false;
    if(splat>0) {
        if(where==WALL) {
            // Glancing drops streak along the wall.
            const line_t &line=lines[hit.line];
            float ex=units(line.v2->x-line.v1->x),ey=units(line.v2->y-line.v1->y),el=std::max(1e-3f,std::hypot(ex,ey));
            float facing=std::abs((vx*ey-vy*ex)/(h*el));
            WallSpan span;bool valid=!wallBudget||(*wallBudget>0&&wallSpan(line,hit.side,hit.section,span)&&span.texture>0&&span.texture!=skytexture&&lz>=span.bottom&&lz<=span.top);
            if(valid&&wallBudget&&SV_Active()&&SV_Trace(x,y,z,lx-x,ly-y,lz-z,nullptr,nullptr)<.98f)valid=false;
            if(valid&&bloodWallSplat(hit,lx,ly,lz,splat,std::clamp(1/std::max(facing,0.35f),1.0f,2.4f),now+t,
                                    kind,kind==BLOOD_SPLAT&&splat>=18,!wallBudget,material)&&wallBudget) {
                --*wallBudget;if(material==LiquidMaterial::Blood)++combatFeedbackStats.wallSplats;
            }
        } else {
            const bool sky=(where==CEILING?endSector->ceilingpic:endSector->floorpic)==skyflatnum;
            const bool edited=(wallBudget||material==LiquidMaterial::Acid)&&(SV_Edited(lx,ly,lz)||SV_Edited(lx,ly,lz+(where==CEILING?2.f:-2.f)));
            if((where==CEILING||!liquid)&&(!(wallBudget||material==LiquidMaterial::Acid)||!sky)&&!edited&&(!flatBudget||*flatBudget>0)) {
                // Drops coming in low stretch along their way.
                float stretch=where==CEILING?1:1+std::clamp(h/std::max(60.0f,std::abs(fall))*0.6f,0.0f,1.2f);
                const float angle=decorationSeed?doom_decor::unit(*decorationSeed)*float(2*doomPi):std::atan2(vy,vx);
                const bool admitted=bloodFloorSplat(lx,ly,(int)endIndex,splat,angle,stretch,where==CEILING,now+t,
                                                    kind,decorationSeed,!wallBudget,material);
                if(admitted&&flatBudget)--*flatBudget;
                floorSplat=admitted&&where==FLOOR&&!sky;
            }
        }
    }
    if(decorationSeed&&!settings.bloodSpray)return floorSplat;
    if(material==LiquidMaterial::Acid) {
        if(std::count_if(drops.begin(),drops.end(),[](const Drop &d){return d.material==LiquidMaterial::Acid;})>=128)return floorSplat;
    } else {
        auto count=std::count_if(drops.begin(),drops.end(),[](const Drop &d){return d.material==LiquidMaterial::Blood;});
        if(count>=maxDrops) {auto it=std::find_if(drops.begin(),drops.end(),[](const Drop &d){return d.material==LiquidMaterial::Blood;});if(it!=drops.end())drops.erase(it);}
    }
    const float shade=decorationSeed?.25f+.2f*doom_decor::unit(*decorationSeed^0xc63u):
                      visual==DropVisual::BlueGut?randomRange(.68f,.95f):randomRange(.35f,.7f);
    const auto color=visual==DropVisual::Bone?paletteColor({shade+.25f,shade+.20f,shade+.12f}):bloodColor(shade);
    drops.push_back({x,y,z,vx,vy,vz,now,now+t+(restBone?.42f:0),size,gravity,t,color,decorationSeed!=nullptr,visual,material});
    return floorSplat;
}

void emitAcidBurst(fixed_t fx,fixed_t fy,fixed_t fz,fixed_t height) {
    if(!acidLiquidOn())return;
    checkSprayLevel();
    const auto oldRandom=impactRandom;
    const uint32_t seed=hash32((uint32_t)fx^((uint32_t)fy*33u)^(uint32_t)leveltime);
    impactRandom.seed(seed?seed:1);
    BloodSourceScope tint(LIQUID_TINT_ACID);
    int walls=6,flats=18;
    float x=units(fx),y=units(fy),z=units(fz)+units(height)*.5f;
    for(int i=0;i<18;++i) {
        float a=(i+randomRange(-.35f,.35f))*float(2*doomPi/18),speed=randomRange(85,210);
        bool heavy=i<6;
        throwDrop(x,y,z,std::cos(a)*speed,std::sin(a)*speed,randomRange(70,240),
                  heavy?1.6f:1.f,heavy?randomRange(18,25):randomRange(5,9),
                  heavy?BLOOD_SPLAT:BLOOD_SPECK,&walls,nullptr,DropVisual::Blood,sprayGravity,&flats,LiquidMaterial::Acid);
    }
    impactRandom=oldRandom;
}

void decorationBloodDrop(const mobj_t &source,float x,float y,float z,uint32_t seed) {
    checkSprayLevel();
    const sector_t *sector=sectorAt(x,y);
    if(z<=floorZ(sector)+.5f||z>=ceilZ(sector)-.5f)return;
    // Check the offset from the actor to the visible flesh. Once detached,
    // gravity moves the speck vertically through that point's sector.
    const float dx=x-units(source.x),dy=y-units(source.y),reach=std::hypot(dx,dy);
    WallHit hit;
    if(reach>.01f&&firstSolidWall(x-dx,y-dy,z,dx/reach,dy/reach,0,reach,hit))return;
    if(settings.bloodSpray) {
        const float now=leveltime/float(TICRATE);
        size_t active=std::count_if(drops.begin(),drops.end(),[&](const Drop &d){return d.decoration&&d.end>now;});
        if(active>=doom_decor::maxDecorationDrops||drops.size()>=maxDrops)return;
    }
    BloodSourceScope tint(&source);
    throwDrop(x,y,z,0,0,-6,.72f,3.f+2.f*doom_decor::unit(seed^0x129du),BLOOD_SPECK,nullptr,&seed);
}
// A spray of count drops from (x,y,z) around the heading (dx,dy), spread
// that far across it, speeds and rises in those ranges; the first big ones
// carry splats of the big size range, the rest specks now and then.
struct Burst { int count,big;float spread,speed[2],rise[2],bigSize[2],speckChance; };
static void burst(float x,float y,float z,float dx,float dy,const Burst &b) {
    for(int i=0;i<b.count;++i) {
        float a=std::atan2(dy,dx)+randomRange(-b.spread,b.spread),speed=randomRange(b.speed[0],b.speed[1]);
        bool big=i<b.big;
        float splat=big?randomRange(b.bigSize[0],b.bigSize[1]):randomRange(0,1)<b.speckChance?randomRange(4.5f,7.5f):0;
        throwDrop(x,y,z,std::cos(a)*speed,std::sin(a)*speed,randomRange(b.rise[0],b.rise[1]),big?1.6f:randomRange(0.8f,1.3f),
                  splat,big?BLOOD_SPLAT:BLOOD_SPECK);
    }
}

// Styles share collision, decal ownership and the same drop vector.
struct GoreStyle {
    DropVisual visual;
    int living,fatal,cores;
    float size,coreSize,speed,spread,rise,gravity,mark;
};
static constexpr GoreStyle humanFlesh={DropVisual::Flesh,3,4,0,2.15f,0,92,.60f,40,sprayGravity,15};
static constexpr GoreStyle impTissue={DropVisual::Tissue,4,6,1,2.8f,4.1f,91,.58f,52,1120,16};
static constexpr GoreStyle revenantBone={DropVisual::Bone,3,6,1,2.1f,3.7f,98,.58f,57,1160,0};
static constexpr GoreStyle mancubusHeavy={DropVisual::Organ,5,8,3,3.6f,6.9f,82,.52f,57,1000,23};
static constexpr GoreStyle pinkyViscera={DropVisual::Organ,4,7,2,3.1f,5.5f,92,.51f,53,1040,20};
static constexpr GoreStyle cacoBlueViscera={DropVisual::BlueGut,6,9,2,3.8f,8.0f,95,.48f,83,850,25};
static const GoreStyle &goreStyle(int type) {
    switch(type) {
    case MT_TROOP:return impTissue;
    case MT_UNDEAD:return revenantBone;
    case MT_FATSO:return mancubusHeavy;
    case MT_SERGEANT:return pinkyViscera;
    case MT_HEAD:return cacoBlueViscera;
    default:return humanFlesh;
    }
}
GoreBurstReport goreBurst(const mobj_t &target,float x,float y,float z,float dx,float dy,GoreOutcome outcome,uint64_t discharge,GoreEmission emission) {
    using namespace hellforge::giblets;
    GoreBurstReport report;
    if(!settings.accelerated||settings.goreLevel!=1||totalkills>=200||!bloodOn()||
       (!settings.goreWallBlood&&!settings.goreFragments))return report;
    checkSprayLevel();
    BloodSourceScope tint(&target);
    const auto &style=goreStyle(target.type);
    const bool medium=emission.tier==GoreTier::Medium,routine=emission.tier==GoreTier::Routine;
    const bool fatal=!medium&&!routine&&target.health<=0,bone=style.visual==DropVisual::Bone;
    const bool m1head=goreHuman(target.type)&&outcome==GoreOutcome::HeadBurst;
    const auto kind=m1head?Kind::BrainLike:bone?Kind::Bone:target.type==MT_FATSO?(fatal&&!emission.radial?Kind::Viscera:Kind::HeavyTissue):
        target.type==MT_SERGEANT||target.type==MT_HEAD?Kind::Viscera:Kind::Flesh;
    const int species=monsterBloodTint(&target);
    const auto materialTint=bone?Tint::PaleBone:species==1?Tint::Blue:species==2?Tint::Green:Tint::Red;
    static constexpr const char *names[]={"flesh","heavy_tissue","viscera","bone","brain_like"};
    static constexpr const char *tints[]={"red","blue","green","pale_bone"};
    report.eventId=(uint64_t(r_levelserial)<<32)|++goreEventOrdinal;
    report.material=names[unsigned(kind)];report.tint=tints[unsigned(materialTint)];
    const float heading=std::atan2(dy,dx);
    int wallBudget=settings.goreWallBlood?3:0;
    int flatBudget=emission.tier==GoreTier::Extreme?9:medium?1:routine?2:fatal?6:4;
    if(settings.goreWallBlood&&!bone)for(int n=0;n<(medium||emission.radial?0:routine?1:m1head?4:fatal?3:2);++n) {
        const float a=heading+randomRange(-.43f,.43f),speed=randomRange(68,126);
        throwDrop(x,y,z,std::cos(a)*speed,std::sin(a)*speed,
            randomRange(-18,45),randomRange(.9f,1.4f),randomRange(14,23),BLOOD_SPLAT,&wallBudget,nullptr,
            DropVisual::GoreBlood,sprayGravity,&flatBudget);
    }
    if(!settings.goreFragments){report.path="fragments_off";return report;}
    const float now=gameSeconds();
    int active=0;
    for(const auto &drop:drops)if(drop.visual>=DropVisual::Flesh&&drop.end>now)++active;
    report.requested=medium?1:routine?std::max(2,style.living-1):m1head?6:fatal?style.fatal:style.living;
    if(emission.radial&&fatal)
        report.requested=std::min(12,report.requested+(emission.tier==GoreTier::Extreme?4:2));
    int count=std::min({report.requested,emission.limit,std::max(0,int(System::MaxPieces)-active-activeGiblets()+restingGiblets())});
    if(!count){report.path="piece_budget";return report;}
    {
        hellforge::giblets::Burst burst;
        burst.origin={x,y,z};burst.direction={dx,dy,0};
        burst.kind=kind;burst.tint=materialTint;
        burst.intensity=medium?.38f:routine?.76f:m1head?.92f:target.type==MT_HEAD?(fatal?1.75f:1.48f):fatal?1.14f:1.f;
        burst.pieces=uint8_t(count);burst.mapKillCount=totalkills;burst.eligible=true;
        burst.radial=emission.radial;
        burst.brief=medium;
        burst.floorMarks=uint8_t(flatBudget);
        burst.extraFloorMarks=medium||routine?0:fatal?3:1;
        burst.maxActive=uint8_t(std::max(0,int(System::MaxPieces)-active));
        if(emission.radial)burst.intensity=fatal?1.65f:1.10f;
        burst.eventId=report.eventId;
        burst.cosmeticSeed=hash32(unsigned(discharge)^unsigned(discharge>>32)^goreEventOrdinal^hash32(r_levelserial));
        report.admitted=emitGiblets(burst,settings.goreWallBlood?wallBudget:0,report);
        if(report.admitted)return report;
    }
    count=std::min(count,std::max(0,int(System::MaxPieces)-active-activeGiblets()));
    const auto *fallbackSector=sectorAt(x,y);
    if(ceilZ(fallbackSector)-floorZ(fallbackSector)<1.f)return report;
    const bool markFallback=settings.goreWallBlood&&std::strcmp(report.path,"solid_geometry")!=0&&
        std::strcmp(report.path,"missing_blockmap")!=0;
    if(!markFallback)wallBudget=0;
    const int cores=style.cores+(fatal&&target.type==MT_HEAD?1:0);
    for(int n=0;n<count;++n) {
        if(m1head) {
            const float a=heading+randomRange(-.60f,.60f),speed=randomRange(54,132);
            const float rise=randomRange(-25,105),mark=markFallback?randomRange(9,17):0;
            throwDrop(x,y,z,std::cos(a)*speed,std::sin(a)*speed,rise,randomRange(1.45f,2.6f),
                      mark,BLOOD_SPLAT,&wallBudget,nullptr,DropVisual::Flesh,sprayGravity,&flatBudget);
            continue;
        }
        const bool core=n<cores;
        const float spread=emission.radial?1.35f:style.spread*(core?.50f:1.f);
        const float a=heading+randomRange(-spread,spread);
        const float speed=style.speed*randomRange(core?.84f:.70f,core?1.06f:1.31f)*(medium?.5f:1.f);
        const float rise=style.rise*randomRange(core?.75f:-.18f,1.28f);
        const float size=(core?style.coreSize:style.size)*randomRange(.86f,1.14f)*(medium?.6f:fatal?1.14f:1.f);
        const float mark=markFallback&&!bone?style.mark*randomRange(.74f,1.25f)*(core?1.f:.64f):0;
        const DropVisual visual=(!core&&(style.visual==DropVisual::Organ||style.visual==DropVisual::BlueGut))?
                                DropVisual::Tissue:style.visual;
        throwDrop(x,y,z,std::cos(a)*speed,std::sin(a)*speed,rise,size,mark,BLOOD_SPLAT,
                  &wallBudget,nullptr,visual,style.gravity,&flatBudget);
    }
    report.admitted=count;
    return report;
}

namespace {
struct BlastVictim {
    uintptr_t token=0;
    mobjtype_t type{};
    float x=0,y=0,z=0,radius=0;
    int before=0,after=0;
    int64_t damage=0;
    bool extreme=false,direct=false,splash=false;
};
struct Blast {
    const mobj_t *spot=nullptr;
    uint64_t id=0;
    size_t count=0;
    std::array<BlastVictim,199> victims;
};
struct DirectImpact {const mobj_t *rocket=nullptr; BlastVictim victim;};
std::array<Blast,4> blasts;
std::array<DirectImpact,8> directImpacts;
unsigned blastLevel=0,blastOrdinal=0;
size_t blastDepth=0;

void resetBlastCapture() {
    blastDepth=0;directImpacts={};
}
bool blastEnabled() {
    return settings.accelerated&&settings.blood&&settings.goreFragments&&settings.goreLevel==1&&
           levelSerial==r_levelserial&&totalkills<200&&!netgame&&!demoplayback&&!demorecording;
}
BlastVictim snapshot(const mobj_t &victim,int before,bool direct) {
    if(!goreSupported(victim.type)||!victim.info||victim.flags&MF_NOBLOOD||
       (criticalVoxelModel&&criticalVoxelModel(victim)))return {};
    return {reinterpret_cast<uintptr_t>(&victim),victim.type,units(victim.x),units(victim.y),
            units(victim.z)+units(victim.info->height)*.52f,units(victim.radius),
            before,victim.health,int64_t(before)-victim.health,
            victim.info->xdeathstate!=S_NULL&&victim.state==&states[victim.info->xdeathstate],direct,!direct};
}
void addBlastVictim(Blast &blast,const BlastVictim &victim) {
    if(!victim.token||victim.damage<=0)return;
    const auto end=blast.victims.begin()+blast.count;
    const auto it=std::find_if(blast.victims.begin(),end,[&](const BlastVictim &value){return value.token==victim.token;});
    if(it==end) {
        if(blast.count<blast.victims.size())blast.victims[blast.count++]=victim;
        return;
    }
    it->after=victim.after;it->damage+=victim.damage;it->extreme=victim.extreme;
    it->direct|=victim.direct;it->splash|=victim.splash;
}
void finishBlast(Blast &blast,const mobj_t &spot) {
    auto end=blast.victims.begin()+blast.count;
    end=std::remove_if(blast.victims.begin(),end,[](const BlastVictim &v){return v.after>0&&v.damage<15;});
    const float x=units(spot.x),y=units(spot.y);
    const auto distance=[&](const BlastVictim &v){return std::hypot(v.x-x,v.y-y);};
    const auto tier=[](const BlastVictim &v){return v.extreme?GoreTier::Extreme:v.after<=0?GoreTier::Fatal:GoreTier::Major;};
    // Stable insertion order breaks exact ties without actor addresses or RNG.
    std::array<size_t,199> order{};
    const size_t eligible=size_t(end-blast.victims.begin());
    for(size_t i=0;i<eligible;++i)order[i]=i;
    std::sort(order.begin(),order.begin()+eligible,[&](size_t a,size_t b){
        const auto &left=blast.victims[a],&right=blast.victims[b];
        if(tier(left)!=tier(right))return tier(left)>tier(right);
        const float d1=distance(left),d2=distance(right);
        if(d1!=d2)return d1<d2;
        if(left.damage!=right.damage)return left.damage>right.damage;
        return a<b;
    });
    const bool debug=M_CheckParm((char*)"-goredebug")!=0;
    int remaining=24,requested=0,deduped=0;
    const size_t selected=std::min(eligible,size_t(24));
    for(size_t i=0;i<selected&&remaining>0;++i) {
        const auto &v=blast.victims[order[i]];
        float dx=v.x-x,dy=v.y-y;
        const float d=distance(v);
        if(d>.01f){dx/=d;dy/=d;}
        else {
            const float radians=float(hash32(unsigned(blast.id)^unsigned(i)))*(2.f*float(doomPi)/4294967296.f);
            dx=std::cos(radians);dy=std::sin(radians);
        }
        mobj_t visual{};
        visual.type=v.type;visual.health=v.after;
        const int share=std::max(1,remaining/int(selected-i));
        const auto origin=gibletBlastOrigin({v.x,v.y,v.z},dx,dy,v.radius*.42f);
        const auto result=goreBurst(visual,origin.x,origin.y,origin.z,dx,dy,
                                    GoreOutcome::None,blast.id,{tier(v),share,true});
        remaining-=result.admitted;requested+=result.requested;deduped+=v.direct&&v.splash;
        if(debug)std::fprintf(stderr,"GORE G1.2 id=%llu exp=%llu cause=%s type=%d dmg=%lld hp=%d->%d tier=%u requested=%d admitted=%d dropped=%d path=%s tint=%s origin=%.1f,%.1f,%.1f\n",
            (unsigned long long)result.eventId,(unsigned long long)blast.id,spot.type==MT_BARREL?"barrel":v.direct?"rocket_direct":"rocket_splash",
            v.type,(long long)v.damage,v.before,v.after,unsigned(tier(v)),result.requested,result.admitted,
            result.requested-result.admitted,result.path,result.tint,origin.x,origin.y,origin.z);
    }
    if(debug)std::fprintf(stderr,"GORE G1.2 EXPLOSION exp=%llu victims=%zu requested=%d admitted=%d dedupedDirectSplash=%d\n",
        (unsigned long long)blast.id,eligible,requested,24-remaining,deduped);
}
}

void goreBlast(gore_blast_op_t op,const mobj_t *spot,const mobj_t *victim,int before) {
    if(blastLevel!=r_levelserial) {
        resetBlastCapture();blastLevel=r_levelserial;blastOrdinal=0;
    }
    if(!blastEnabled()) {resetBlastCapture();return;}
    const auto pending=std::find_if(directImpacts.begin(),directImpacts.end(),
                                    [spot](const DirectImpact &p){return p.rocket==spot;});
    if(op==GORE_BLAST_FORGET) {if(pending!=directImpacts.end())*pending={};return;}
    if(op==GORE_BLAST_DIRECT) {
        const auto value=snapshot(*victim,before,true);
        if(!value.token||pending!=directImpacts.end())return;
        const auto free=std::find_if(directImpacts.begin(),directImpacts.end(),[](const DirectImpact &p){return !p.rocket;});
        if(free!=directImpacts.end())*free={spot,value};
        return;
    }
    if(op==GORE_BLAST_COMPLETE&&pending==directImpacts.end())return;
    if(op==GORE_BLAST_BEGIN||op==GORE_BLAST_COMPLETE) {
        if(blastDepth++<blasts.size()) {
            auto &blast=blasts[blastDepth-1];
            blast.spot=spot;blast.count=0;
            blast.id=(uint64_t(r_levelserial)<<32)|++blastOrdinal;
            if(pending!=directImpacts.end())addBlastVictim(blast,pending->victim);
        }
        if(pending!=directImpacts.end())*pending={};
        if(op==GORE_BLAST_BEGIN)return;
    }
    if(!blastDepth)return;
    if(op==GORE_BLAST_SPLASH) {
        if(blastDepth<=blasts.size()&&blasts[blastDepth-1].spot==spot)
            addBlastVictim(blasts[blastDepth-1],snapshot(*victim,before,false));
        return;
    }
    if(blastDepth--<=blasts.size()&&blasts[blastDepth].spot==spot)finishBlast(blasts[blastDepth],*spot);
}

void combatHeadBlood(const wp_damage_event_t &event) {
    if(!bloodOn())return;
    checkSprayLevel();BloodSourceScope source(event.target);++combatFeedbackStats.headBursts;
    float x=event.impact.x,y=event.impact.y,z=event.impact.z;
    float dx=x-units(event.source->x),dy=y-units(event.source->y),len=std::hypot(dx,dy);
    if(len<.001f)return;dx/=len;dy/=len;
    int wallBudget=p_wp_balance.headWallMarks;
    float scale=p_wp_balance.headBloodScale/100.f;
    if(settings.bloodSpray) {
        for(int i=0;i<(int)std::ceil(6*scale);++i) {
            bool fast=i%6==5;
            float spread=fast?0.42f:1.45f;
            float a=std::atan2(dy,dx)+randomRange(-spread,spread)+(i%3==0?doomPi:0);
            float speed=fast?randomRange(100,124):randomRange(36,88);
            throwDrop(x,y,z,std::cos(a)*speed,std::sin(a)*speed,randomRange(fast?-5:-65,48),
                randomRange(0.75f,1.6f)*scale,randomRange(8,16)*scale,BLOOD_SPLAT,&wallBudget);
        }
        return;
    }
    // First solid face only; no decals on absent/sky textures or through nearer geometry.
    for(float angle:{0.f,-.35f,.35f,-.8f,.8f}) {
        if(wallBudget<=0)break;
        float a=std::atan2(dy,dx)+angle,ux=std::cos(a),uy=std::sin(a);WallHit hit;
        if(!firstSolidWall(x,y,z,ux,uy,0,(float)p_wp_balance.headWallReach,hit))continue;
        WallSpan span;if(!wallSpan(lines[hit.line],hit.side,hit.section,span)||span.texture<=0||span.texture==skytexture||z<span.bottom||z>span.top)continue;
        if(SV_Active()&&SV_Trace(x,y,z,ux*hit.t,uy*hit.t,0,nullptr,nullptr)<.98f)continue;
        bloodWallSplat(hit,x+ux*hit.t,y+uy*hit.t,z,randomRange(12,19)*scale,1,gameSeconds(),BLOOD_SPLAT,true);
        --wallBudget;++combatFeedbackStats.wallSplats;
    }
}
// Wound coordinates follow the current actor pose and the recorded shot side.
static void woundOrigin(const mobj_t &thing,const WoundSpot &w,float &x,float &y,float &z,float &yaw) {
    yaw=(float)(thing.angle*(2*doomPi/4294967296.0))+w.yaw;
    float vx=std::cos(yaw),vy=std::sin(yaw),r=units(thing.radius)*0.7f;
    x=units(thing.x)-vx*r+vy*w.across;
    y=units(thing.y)-vy*r-vx*w.across;
    z=units(thing.z)+w.height;
}

void combatBleed(mobj_t *actor,int justApplied) {
    if(!actor||actor->health<=0||!bloodOn()||!bleeds(*actor))return;
    int tier=P_WeakpointBleedTier(actor);
    if(!tier)return;
    checkSprayLevel();
    static unsigned serial=~0u;
    static int budgetTic=-1,budget=0;
    if(serial!=r_levelserial||budgetTic!=leveltime){serial=r_levelserial;budgetTic=leveltime;budget=0;}
    if(budget>=96)return;

    // One emission in each window, staggered by actor and varied by window.
    // This keeps the pressure irregular without retaining actor pointers.
    const int window=tier==3?5:tier==2?8:12;
    unsigned seed=hash32((uint32_t)(reinterpret_cast<uintptr_t>(actor)>>4));
    unsigned phase=(unsigned)leveltime+seed%window;
    unsigned pattern=hash32(seed^(phase/window));
    if(!justApplied&&phase%window!=pattern%(window-1))return;

    BloodSourceScope bloodSource(actor);
    float ox=units(actor->x),oy=units(actor->y),feet=units(actor->z);
    if(!settings.bloodSpray) {
        bool pool=tier==3&&(justApplied||pattern%4==0);
        const sector_t *sector=sectorAt(ox,oy);
        size_t index=sector-sectors;
        if((pool||actor->momx||actor->momy)&&sector->floorpic!=skyflatnum&&
           (index>=sectorMist.size()||!sectorMist[index].liquid)&&std::abs(feet-floorZ(sector))<32){
            bloodFloorSplat(ox,oy,(int)index,pool?randomRange(15,24):randomRange(3,6)*tier,
                            0,1,false,gameSeconds(),pool?BLOOD_SPLAT:BLOOD_SPECK);
            ++budget;++combatFeedbackStats.bleedDrops;
            if(pool)++combatFeedbackStats.bleedPools;
        }
        return;
    }

    WoundSpot spots[10];
    int count=woundSpots(actor,spots,10);
    float height=units(actor->height);
    WoundSpot fallback={height*0.54f,0,doomPi*0.75f,1,leveltime};
    const WoundSpot *w=&fallback;
    for(int i=0;i<count;++i)if(w==&fallback||spots[i].tic>=w->tic)w=&spots[i];
    float x,y,z,yaw;
    woundOrigin(*actor,*w,x,y,z,yaw);
    z=std::clamp(z,feet+0.5f,feet+std::max(0.5f,height-0.5f));
    float reach=std::hypot(x-ox,y-oy);
    WallHit wall;
    if(reach>0.01f&&((firstSolidWall(ox,oy,z,(x-ox)/reach,(y-oy)/reach,0,reach,wall))||
                    (SV_Active()&&SV_Trace(ox,oy,z,x-ox,y-oy,0,nullptr,nullptr)<0.98f))) {x=ox;y=oy;}

    bool gush=justApplied||randomRange(0,1)<tier*0.19f;
    float carryX=std::clamp(units(actor->momx)*TICRATE*0.28f,-22.0f,22.0f);
    float carryY=std::clamp(units(actor->momy)*TICRATE*0.28f,-22.0f,22.0f);
    int wallBudget=1;
    int emitted=gush?tier+1:1+(tier==3);
    for(int n=0;n<emitted&&budget<96;++n) {
        bool heavy=gush&&n==0&&tier>=2;
        float a=yaw+doomPi+randomRange(-1.25f,1.25f),speed=gush?randomRange(12,49):randomRange(4,24);
        float size=randomRange(0.7f,1.2f)+0.24f*(tier-1)+(heavy?0.35f:0);
        float splat=heavy?randomRange(12,18)+tier*2:randomRange(3,6)*tier;
        bool floor=throwDrop(x,y,z,carryX+std::cos(a)*speed,carryY+std::sin(a)*speed,
                            randomRange(-115,gush?10:-35),size,splat,heavy?BLOOD_SPLAT:BLOOD_SPECK,&wallBudget);
        ++budget;++combatFeedbackStats.bleedDrops;
        if(heavy&&tier==3&&floor)++combatFeedbackStats.bleedPools;
    }
}
void sprayHit(const mobj_t *target,float x,float y,float z,float dx,float dy,float slope,int damage) {
    BloodSourceScope bloodSource(target);
    if(!target||!bleeds(*target))return;
    checkSprayLevel();
    // The game puts the hit 10 units short of the body. The exit spray
    // leaves the far side of it, along the shot, the harder the faster.
    float r=units(target->radius),through=(units(target->x)-x)*dx+(units(target->y)-y)*dy+r*0.6f;
    float ex=x+dx*through,ey=y+dy*through,ez=z+slope*through,force=0.7f+std::min(damage,30)/30.0f;
    int count=std::clamp(1+damage/5,2,7),big=1+(randomRange(0,30)<damage);
    for(int i=0;i<count;++i) {
        float across=randomRange(-0.3f,0.3f),speed=randomRange(110,280)*force;
        float vx=(dx-dy*across)*speed,vy=(dy+dx*across)*speed,vz=randomRange(-30,110)+slope*speed;
        float splat=i<big?randomRange(14,24)*(damage>=15?1.25f:1.0f):randomRange(0,1)<0.6f?randomRange(4.5f,7.5f):0;
        throwDrop(ex,ey,ez,vx,vy,vz,i<big?1.6f:randomRange(0.8f,1.3f),splat,i<big?BLOOD_SPLAT:BLOOD_SPECK);
    }
    // Back spatter at the entry, falling short toward the shooter.
    if(randomRange(0,1)<0.5f) {
        float near=std::max(0.0f,through-2*r);
        burst(x+dx*near,y+dy*near,z,-dx,-dy,{1+(damage>=10),0,0.6f,{30,90},{0,80},{0,0},0.7f});
    }
}
void spraySawCut(const mobj_t &target,float x,float y,float z,float dx,float dy) {
    checkSprayLevel();
    // Both sides of the blade shed blood from the entry side of the torso.
    // Use the same gravity, wall impacts and floor splats as bullet spatter.
    const float along=(units(target.x)-x)*dx+(units(target.y)-y)*dy;
    const float entry=std::max(0.f,along-units(target.radius)*.46f);
    x+=dx*entry;y+=dy*entry;
    const Burst spatter={4,1,.90f,{44,144},{-40,90},{9,16},.78f};
    burst(x,y,z,-dy-dx*.30f, dx-dy*.30f,spatter);
    burst(x,y,z, dy-dx*.30f,-dx-dy*.30f,spatter);
}
void sprayKill(const mobj_t &thing,float dx,float dy,float speed,float hitZ,bool gib,int overkill) {
    BloodSourceScope bloodSource(&thing);
    if(!sprayOn()||!bleeds(thing))return;
    checkSprayLevel();
    float x=units(thing.x),y=units(thing.y),feet=units(thing.z),tall=std::max(16.0f,units(thing.info->height)),r=units(thing.radius);
    float z=feet+(hitZ>=0?std::clamp(hitZ,tall*0.2f,tall*0.9f):tall*0.6f);
    if(gib) {
        // Burst all around and high, up to a low ceiling.
        burst(x,y,z,1,0,{32,6,doomPi,{60,230},{40,330},{18,30},0.7f});
        return;
    }
    float force=std::clamp((speed+overkill/8.0f)/4,0.4f,2.0f);
    int count=std::clamp(6+overkill/3+(int)(speed*2),6,26),big=std::min(5,2+overkill/20);
    bool pushed=speed>=0.6f;
    float ox=x+(pushed?dx*r*0.5f:0),oy=y+(pushed?dy*r*0.5f:0);
    burst(ox,oy,z,dx,dy,{count,big,pushed?0.7f:doomPi,{60*force,200*force},{-20,140*std::max(0.6f,force)},{18,30},0.7f});
    if(count>=10)I_MadeSound(IMPACT_GORE_BURST,x,y,z);
}
void sprayHurt(const mobj_t &thing,float dx,float dy,float hitZ,int damage) {
    BloodSourceScope bloodSource(&thing);
    if(!sprayOn()||!bleeds(thing))return;
    checkSprayLevel();
    float tall=std::max(16.0f,units(thing.info->height)),z=units(thing.z)+(hitZ>=0?std::clamp(hitZ,tall*0.2f,tall*0.9f):tall*0.6f);
    float r=units(thing.radius),x=units(thing.x)+dx*r*0.5f,y=units(thing.y)+dy*r*0.5f;
    burst(x,y,z,dx,dy,{std::clamp(2+damage/8,3,8),1,0.8f,{60,180},{-10,120},{14,22},0.6f});
}
static bool onLiquid(float x,float y) {
    size_t index=sectorAt(x,y)-sectors;
    return index<sectorMist.size()&&sectorMist[index].liquid;
}
// The slap is heard with or without blood spray (it went quiet with the spray switch).
void sprayLanding(float x,float y,float z,float speed) {
    if(onLiquid(x,y))return;
    if(speed>1.5f)I_MadeSound(IMPACT_GORE_SLAP,x,y,z);
    if(!sprayOn())return;
    checkSprayLevel();
    size_t index=sectorAt(x,y)-sectors;
    bloodFloorSplat(x,y,(int)index,std::clamp(16+speed*4,18.0f,32.0f),randomRange(0,2*doomPi),1,false,gameSeconds(),BLOOD_SPLAT);
    burst(x,y,z+2,1,0,{std::clamp((int)(2+speed*1.2f),3,8),0,doomPi,{30,110},{60,150},{0,0},0.8f});
}
// A splash on the first wall within reach from (x,y,z) along (dx,dy), a burst back off it, the slap.
static bool wallSlam(float x,float y,float z,float dx,float dy,float reach,float size,int specks) {
    WallHit hit;
    if(!firstSolidWall(x,y,z,dx,dy,0,reach,hit))return false;
    I_MadeSound(IMPACT_GORE_SLAP,x,y,z);
    if(!sprayOn())return true;
    float now=gameSeconds();
    bloodWallSplat(hit,x+dx*hit.t,y+dy*hit.t,z,size,1.3f,now,BLOOD_SPLAT,true);
    burst(x,y,z,-dx,-dy,{specks,0,0.9f,{30,100},{0,90},{0,0},0.7f});
    return true;
}
void sprayWallHit(float x,float y,float z,float dx,float dy,float speed) {
    if(sprayOn())checkSprayLevel();
    wallSlam(x,y,z,dx,dy,16,std::clamp(18+speed*4,20.0f,34.0f),std::clamp((int)(2+speed*1.5f),3,8));
}
void spraySmear(float x,float y,float angle,float size) {
    if(!sprayOn()||onLiquid(x,y))return;
    checkSprayLevel();
    bloodFloorSplat(x,y,(int)(sectorAt(x,y)-sectors),size,angle,1,false,gameSeconds(),BLOOD_SMEAR);
}

// Wounds drip while they are fresh: about one drop a second from a big
// hole, thinning out over twenty seconds, falling with the monster's step.
static void drip(const mobj_t &thing) {
    BloodSourceScope bloodSource(&thing);
    WoundSpot spots[10];
    int count=woundSpots(&thing,spots,10,true);
    if(!count)return;
    for(int n=0;n<count;++n) {
        const WoundSpot &w=spots[n];
        float age=(leveltime-w.tic)/(float)TICRATE,fresh=std::clamp(1-age/20,0.0f,1.0f);
        if(randomRange(0,1)>=0.02f*w.radius*fresh)continue;
        float x,y,z,yaw;
        woundOrigin(thing,w,x,y,z,yaw);
        throwDrop(x,y,z,units(thing.momx)*TICRATE+randomRange(-6,6),units(thing.momy)*TICRATE+randomRange(-6,6),0,
                  randomRange(0.7f,1.0f),randomRange(0,1)<0.5f?randomRange(4,6):0,BLOOD_SPECK,
                  nullptr,nullptr,w.critical?DropVisual::GoreDrip:DropVisual::Blood);
    }
}
void watchSpray() {
    checkSprayLevel();
    if(!acidLiquidOn())clearAcidVisuals();
    tickAcidCoatings();
    const bool goreOnly=settings.accelerated&&settings.goreLevel&&settings.blood&&totalkills<200;
    if(levelSerial!=r_levelserial) {drops.clear();slides.clear();clearGiblets();return;}
    if(!sprayOn()&&!goreOnly&&!acidLiquidOn()) {drops.clear();slides.clear();clearGiblets();return;}
    checkSprayLevel();
    if(activeGiblets()) {
        const float now=gameSeconds();
        const int procedural=int(std::count_if(drops.begin(),drops.end(),[&](const Drop &d){
            return d.visual>=DropVisual::Flesh&&d.end>now;
        }));
        tickGiblets(std::count_if(drops.begin(),drops.end(),[](const Drop &d){return d.material==LiquidMaterial::Blood;}),procedural);
    }
    if(!sprayOn()){slides.clear();return;}
    for(auto &entry:slides)entry.second.seen=false;
    float now=gameSeconds();
    for(int i=0;i<numsectors;++i)for(mobj_t *thing=sectors[i].thinglist;thing;thing=thing->snext) {
        if(!bleeds(*thing))continue;
        BloodSourceScope bloodSource(thing);
        if(thing->health>0) {if(!P_WeakpointBleeding(thing))drip(*thing);slides.erase(thing);continue;}
        float x=units(thing->x),y=units(thing->y),mx=units(thing->momx),my=units(thing->momy),speed=std::hypot(mx,my);
        auto found=slides.find(thing);
        if(found==slides.end()) {slides[thing]={x,y,speed,speed>0?mx/speed:0,speed>0?my/speed:0,0,false,true};continue;}
        Slide &s=found->second;s.seen=true;
        // A ragdoll's body is elsewhere: its own contacts splash and smear (ragdoll.cpp).
        bool doll=hasRagdoll(thing);
        // Thrown hard and stopped short (more than friction does): it met
        // a wall. The wall takes a splash at the body's middle, once.
        if(!doll&&!s.slammed&&s.speed>5&&speed<s.speed*0.5f) {
            float tall=std::max(16.0f,units(thing->info->height)),z=units(thing->z)+tall*0.5f;
            if(wallSlam(s.x,s.y,z,s.dx,s.dy,units(thing->radius)+12,randomRange(24,34),6))s.slammed=true;
        }
        // Down and sliding: a smear every so far, thick end at the body.
        float moved=std::hypot(x-s.x,y-s.y);
        if(!doll&&!(thing->flags&MF_SOLID)&&thing->z<=thing->floorz&&speed>1.2f) {
            s.since+=moved;
            float size=std::clamp(units(thing->radius)*0.6f,9.0f,15.0f);
            if(s.since>=size*1.4f) {
                s.since=0;
                size_t index=thing->subsector->sector-sectors;
                if(index>=sectorMist.size()||!sectorMist[index].liquid)
                    bloodFloorSplat(x-mx/speed*size*0.6f,y-my/speed*size*0.6f,(int)index,size,std::atan2(my,mx)+doomPi,1,false,now,BLOOD_SMEAR);
            }
        } else s.since=0;
        s.x=x;s.y=y;s.speed=speed;
        if(speed>0.01f) {s.dx=mx/speed;s.dy=my/speed;}
    }
    for(auto it=slides.begin();it!=slides.end();) it=it->second.seen?std::next(it):slides.erase(it);
}
void buildSpray(const Uniforms &camera) {
    checkSprayLevel();
    drawGiblets(camera);
    float now=gameSeconds();
    const bool showDecor=horrorDecorEnabled()&&settings.blood&&settings.bloodSpray;
    const bool showGore=settings.accelerated&&settings.goreLevel==1&&settings.blood&&totalkills<200&&
                        !netgame&&!demoplayback&&!demorecording;
    drops.erase(std::remove_if(drops.begin(),drops.end(),[&](const Drop &d){
        if(d.material==LiquidMaterial::Acid)return now>=d.end||!acidLiquidOn();
        return now>=d.end||now<d.born-0.1f||(d.visual==DropVisual::Blood&&!sprayOn()&&!showGore)||(d.decoration&&!showDecor)||
               (d.visual!=DropVisual::Blood&&(!showGore||
                  (d.visual==DropVisual::GoreBlood?!settings.goreWallBlood:d.visual==DropVisual::GoreDrip?
                      (!settings.goreBodyDamage||!settings.bloodSpray):!settings.goreFragments)));
    }),drops.end());
    for(const Drop &d:drops) {
        const bool smooth=d.visual==DropVisual::BlueGut||d.visual==DropVisual::Bone;
        const float elapsed=now-d.born;
        const float t=std::clamp(smooth?elapsed:std::floor(elapsed*TICRATE)/TICRATE,0.f,d.flight);
        const auto height=[&](float at){return d.z+d.vz*at-.5f*d.gravity*at*at;};
        const float x=d.x+d.vx*t,y=d.y+d.vy*t,z=height(t);
        const float light=lighting(sectorAt(x,y)),tall=std::abs(d.vz-d.gravity*t)>250?2.0f:1.0f;
        std::array<float,3> lit;for(int c=0;c<3;++c)lit[c]=std::min(1.0f,d.color[c]*light);
        if(d.visual==DropVisual::BlueGut) {
            const float speed=std::hypot(d.vx,d.vy);
            const float sideX=-d.vy/speed*d.size*.42f,sideY=d.vx/speed*d.size*.42f;
            pixelBlock(particleVertices,camera,x,y,z,d.size,1.03f,lit,false,&d.color,false);
            pixelBlock(particleVertices,camera,x+sideX,y+sideY,z+d.size*.21f,d.size*.67f,.86f,lit,false,&d.color,false);
            const float shoulder=std::max(0.f,t-.035f);
            pixelBlock(particleVertices,camera,d.x+d.vx*shoulder-sideX*.5f,d.y+d.vy*shoulder-sideY*.5f,
                       height(shoulder)-d.size*.15f,d.size*.61f,.86f,lit,false,&d.color,false);
            for(int i=1;i<=2;++i) {
                const float lag=.06f+i*.035f;
                if(t<lag)break;
                const float at=t-lag;
                pixelBlock(particleVertices,camera,d.x+d.vx*at,d.y+d.vy*at,height(at),
                           d.size*(.42f-.07f*i),.9f,lit,false,&d.color,false);
            }
        } else if(d.visual==DropVisual::Bone) {
            const bool resting=elapsed>=d.flight;
            pixelBlock(particleVertices,camera,x,y,z,d.size*.72f,resting?.75f:2.3f,lit,resting,&d.color);
            pixelBlock(particleVertices,camera,x+d.size*.3f,y,z+(resting?0:-d.size*.4f),d.size*.5f,.85f,lit,resting,&d.color);
        } else if(d.visual==DropVisual::Organ) {
            pixelBlock(particleVertices,camera,x,y,z,d.size,1.25f,lit,false,&d.color);
            pixelBlock(particleVertices,camera,x+d.size*.42f,y,z+d.size*.28f,d.size*.8f,.9f,lit,false,&d.color);
            pixelBlock(particleVertices,camera,x-d.size*.27f,y+d.size*.25f,z-d.size*.3f,d.size*.58f,1.1f,lit,false,&d.color);
        } else if(d.visual>=DropVisual::Flesh) {
            // A tiny uneven two-lobe cluster reads as tissue rather than a
            // perfectly spherical drop, without art or additional textures.
            pixelBlock(particleVertices,camera,x,y,z,d.size*1.15f,d.visual==DropVisual::Tissue?1.3f:tall,lit,false,&d.color);
            pixelBlock(particleVertices,camera,x+d.size*.4f,y-d.size*.25f,z+d.size*.25f,
                       d.size*.63f,tall,lit,false,&d.color);
        } else pixelBlock(particleVertices,camera,x,y,z,d.size,tall,lit,false,&d.color);
    }
}
void sprayShutdown() {clearAcidVisuals();drops.clear();slides.clear();clearGiblets();resetBlastCapture();}
} // namespace doom3d

#ifdef DOOM_TEST_DRIVER
extern "C" int I_TestAcidDrops() {return int(std::count_if(doom3d::drops.begin(),doom3d::drops.end(),[](const doom3d::Drop &d){return d.material==doom3d::LiquidMaterial::Acid;}));}
extern "C" int I_TestAcidCoatings() {return int(doom3d::acidCoatings.size());}
#endif

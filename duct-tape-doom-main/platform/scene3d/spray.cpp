/* Blood spray: what hits and kills throw. Drops fly as Doom-pixel specks
   and land as splats on floors, walls and ceilings; wounds drip; bodies
   sliding across the floor smear it, bodies thrown into walls splash them.
   Drawing only: own random numbers, nothing the game sees. */
#include "internal.h"
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
struct Drop {
    float x,y,z,vx,vy,vz,born,end,size;
    std::array<float,3> color;
    bool decoration=false,flesh=false;
};
static std::vector<Drop> drops;
constexpr size_t maxDrops=768;
// Dying and dead bodies watched for sliding (smears) and stopping short (a wall).
struct Slide { float x,y,speed,dx,dy,since;bool slammed,seen; };
static std::unordered_map<const mobj_t*,Slide> slides;
static unsigned sprayLevelSerial;

static void checkSprayLevel() {
    if(sprayLevelSerial==r_levelserial)return;
    drops.clear();slides.clear();sprayLevelSerial=r_levelserial;
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
                      const uint32_t *decorationSeed=nullptr,bool flesh=false) {
    float now=decorationSeed?leveltime/float(TICRATE):gameSeconds();
    const sector_t *start=sectorAt(x,y);
    z=std::clamp(z,floorZ(start)+0.5f,ceilZ(start)-0.5f);
    auto fallTime=[&](float floor) {
        float d=z-floor;
        return d<0?-1.0f:(vz+std::sqrt(vz*vz+2*sprayGravity*d))/sprayGravity;
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
        if(rise>0&&vz*vz>2*sprayGravity*rise) {
            float up=(vz-std::sqrt(vz*vz-2*sprayGravity*rise))/sprayGravity;
            const sector_t *above=sectorAt(x+vx*up,y+vy*up);
            if(up<t&&std::abs(ceilZ(above)-ceilZ(start))<1) {t=up;where=CEILING;landing=above;}
        }
    }
    // A wall in the way, along the chord of its arc.
    float h=std::hypot(vx,vy);WallHit hit;
    if(h>1) {
        float reach=h*t,end=z+vz*t-0.5f*sprayGravity*t*t;
        if(firstSolidWall(x,y,z,vx/h,vy/h,(end-z)/reach,reach,hit)) {t=hit.t/h;where=WALL;}
    }
    if(t>2.5f)splat=0; // Down a pit.
    float lx=x+vx*t,ly=y+vy*t,lz=z+vz*t-0.5f*sprayGravity*t*t,fall=vz-sprayGravity*t;
    bool floorSplat=false;
    if(splat>0) {
        if(where==WALL) {
            // Glancing drops streak along the wall.
            const line_t &line=lines[hit.line];
            float ex=units(line.v2->x-line.v1->x),ey=units(line.v2->y-line.v1->y),el=std::max(1e-3f,std::hypot(ex,ey));
            float facing=std::abs((vx*ey-vy*ex)/(h*el));
            WallSpan span;bool valid=!wallBudget||(*wallBudget>0&&wallSpan(line,hit.side,hit.section,span)&&span.texture>0&&span.texture!=skytexture&&lz>=span.bottom&&lz<=span.top);
            if(valid&&wallBudget&&SV_Active()&&SV_Trace(x,y,z,lx-x,ly-y,lz-z,nullptr,nullptr)<.98f)valid=false;
            if(valid){bloodWallSplat(hit,lx,ly,lz,splat,std::clamp(1/std::max(facing,0.35f),1.0f,2.4f),now+t,kind,kind==BLOOD_SPLAT&&splat>=18);
                if(wallBudget){--*wallBudget;++combatFeedbackStats.wallSplats;}}
        } else {
            const sector_t *s=where==CEILING?landing:sectorAt(lx,ly);
            size_t index=s-sectors;
            bool liquid=index<sectorMist.size()&&sectorMist[index].liquid;
            if(where==CEILING||!liquid) {
                // Drops coming in low stretch along their way.
                float stretch=where==CEILING?1:1+std::clamp(h/std::max(60.0f,std::abs(fall))*0.6f,0.0f,1.2f);
                const float angle=decorationSeed?doom_decor::unit(*decorationSeed)*float(2*doomPi):std::atan2(vy,vx);
                bloodFloorSplat(lx,ly,(int)index,splat,angle,stretch,where==CEILING,now+t,kind,decorationSeed);
                floorSplat=where==FLOOR&&s->floorpic!=skyflatnum;
            }
        }
    }
    if(decorationSeed&&!settings.bloodSpray)return floorSplat;
    if(drops.size()>=maxDrops)drops.erase(drops.begin(),drops.begin()+(drops.size()-maxDrops+1));
    const float shade=decorationSeed?.25f+.2f*doom_decor::unit(*decorationSeed^0xc63u):randomRange(.35f,.7f);
    drops.push_back({x,y,z,vx,vy,vz,now,now+t,size,bloodColor(shade),decorationSeed!=nullptr,flesh});
    return floorSplat;
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

// Milestone 1 heavy close-range impact. These are short-lived drawn pixel
// clusters, never new actors/physics bodies. The existing ballistic drop
// integrator finds walls/floors/ceilings and uses the ordinary decal caps.
void goreBurst(const mobj_t &target,float x,float y,float z,float dx,float dy,bool head) {
    if(!settings.accelerated||settings.goreLevel!=1||!settings.blood||
       totalkills>=200||(!settings.goreWallBlood&&!settings.goreFragments))return;
    checkSprayLevel();
    BloodSourceScope tint(&target);
    const float heading=std::atan2(dy,dx);
    int wallBudget=settings.goreWallBlood?3:0;
    // A directional cloud behind the victim makes the shot feel forceful
    // without globally increasing normal bullet spray.
    if(settings.goreWallBlood)for(int n=0;n<(head?4:3);++n) {
        const float a=heading+randomRange(-.43f,.43f),speed=randomRange(68,126);
        throwDrop(x,y,z,std::cos(a)*speed,std::sin(a)*speed,
            randomRange(-18,45),randomRange(.9f,1.4f),randomRange(14,23),BLOOD_SPLAT,&wallBudget);
    }
    if(!settings.goreFragments)return;
    // Max 64 live flesh clusters across the whole scene. Keep the 768-drop
    // budget shared with normal blood instead of allocating a new particle pool.
    const float now=gameSeconds();
    int active=0;
    for(const auto &d:drops)if(d.flesh&&d.end>now)++active;
    const int count=std::min(head?6:3,std::max(0,64-active));
    for(int n=0;n<count;++n) {
        const float a=heading+randomRange(-.60f,.60f),speed=randomRange(54,132);
        float vZ=randomRange(-25,105);
        // Only the existing decal system persists on the ground or wall.
        const float splat=settings.goreWallBlood?randomRange(9,17):0;
        throwDrop(x,y,z,std::cos(a)*speed,std::sin(a)*speed,vZ,
                  randomRange(1.45f,2.6f),splat,BLOOD_SPLAT,&wallBudget,nullptr,true);
    }
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
    int count=woundSpots(&thing,spots,10);
    if(!count)return;
    for(int n=0;n<count;++n) {
        const WoundSpot &w=spots[n];
        float age=(leveltime-w.tic)/(float)TICRATE,fresh=std::clamp(1-age/20,0.0f,1.0f);
        if(randomRange(0,1)>=0.02f*w.radius*fresh)continue;
        float x,y,z,yaw;
        woundOrigin(thing,w,x,y,z,yaw);
        throwDrop(x,y,z,units(thing.momx)*TICRATE+randomRange(-6,6),units(thing.momy)*TICRATE+randomRange(-6,6),0,
                  randomRange(0.7f,1.0f),randomRange(0,1)<0.5f?randomRange(4,6):0,BLOOD_SPECK);
    }
}
void watchSpray() {
    const bool goreOnly=settings.accelerated&&settings.goreLevel&&settings.blood&&totalkills<200;
    if((!sprayOn()&&!goreOnly)||levelSerial!=r_levelserial) {drops.clear();slides.clear();return;}
    checkSprayLevel();
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
    float now=gameSeconds();
    const bool showDecor=horrorDecorEnabled()&&settings.blood&&settings.bloodSpray;
    drops.erase(std::remove_if(drops.begin(),drops.end(),[&](const Drop &d){
        return now>=d.end||now<d.born-0.1f||(d.decoration&&!showDecor);
    }),drops.end());
    for(const Drop &d:drops) {
        float t=std::clamp(std::floor((now-d.born)*TICRATE)/TICRATE,0.0f,d.end-d.born);
        float x=d.x+d.vx*t,y=d.y+d.vy*t,z=d.z+d.vz*t-0.5f*sprayGravity*t*t;
        float light=lighting(sectorAt(x,y)),tall=std::abs(d.vz-sprayGravity*t)>250?2.0f:1.0f;
        std::array<float,3> lit;for(int c=0;c<3;++c)lit[c]=std::min(1.0f,d.color[c]*light);
        if(d.flesh) {
            // A tiny uneven two-lobe cluster reads as tissue rather than a
            // perfectly spherical drop, without art or additional textures.
            pixelBlock(particleVertices,camera,x,y,z,d.size*1.15f,tall,lit,false,&d.color);
            pixelBlock(particleVertices,camera,x+d.size*.4f,y-d.size*.25f,z+d.size*.25f,
                       d.size*.63f,tall,lit,false,&d.color);
        } else pixelBlock(particleVertices,camera,x,y,z,d.size,tall,lit,false,&d.color);
    }
}
void sprayShutdown() {drops.clear();slides.clear();}
} // namespace doom3d

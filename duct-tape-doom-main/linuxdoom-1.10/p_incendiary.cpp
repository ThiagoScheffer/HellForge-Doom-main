#include "p_incendiary.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <deque>
#include <unordered_map>
#include <unordered_set>
#include <vector>

extern "C" {
#include "doomstat.h"
#include "p_local.h"
#include "p_weakpoints.h"
#include "r_state.h"
#include "s_sound.h"
#include "sounds.h"
int G_DemoLoading(void);
}

namespace {
constexpr int health=30, blast=96, fireDamage=3, interval=20, linger=35;
constexpr int maxPools=32, maxExposures=1024, maxBlastsPerTic=16;
constexpr int leakLife=100, centralLife=320, spillLife=290;
constexpr int leakRadius=21, maxLeakRadius=29, explosionTics=24;
constexpr unsigned featureFlags=MF_SHOOTABLE|MF_NOBLOOD;
constexpr double tau=6.283185307179586;
struct Pool {
    p_oil_pool_view_t view{};
    int born=0,expires=0;
    mobj_t *source=nullptr;
};
struct Exposure { int until,next; mobj_t *source; };
std::vector<Pool> pools;
std::unordered_map<mobj_t*,Exposure> exposures;
std::deque<mobj_t*> pending;
bool enabled=false, applied=false;
int lastSweep=-1;

bool eligibleBarrel(const mobj_t *actor) {
    if(actor->type!=MT_MISC77||!actor->info)return false;
    const auto &info=*actor->info;
    if(info.doomednum!=70||info.flags!=MF_SOLID||info.spawnstate!=S_BBAR1||
       info.spawnhealth!=1000||info.deathstate!=S_NULL||info.xdeathstate!=S_NULL||
       info.seestate!=S_NULL||info.painstate!=S_NULL||info.raisestate!=S_NULL||
       info.meleestate!=S_NULL||info.missilestate!=S_NULL||
       info.radius!=16*FRACUNIT||info.height!=16*FRACUNIT)return false;
    for(int i=0;i<3;++i) {
        const auto &state=states[S_BBAR1+i];
        if(state.sprite!=SPR_FCAN||state.frame!=(32768+i)||state.tics!=4||
           state.action.acv||state.nextstate!=S_BBAR1+(i+1)%3)return false;
    }
    return true;
}

template<class Function> void forActors(Function visit) {
    if(!thinkercap.next)return;
    for(auto *t=thinkercap.next;t!=&thinkercap;t=t->next)
        if(t->function.acp1==(actionf_p1)P_MobjThinker)visit((mobj_t*)t);
}

void updateBarrel(mobj_t *actor,bool on) {
    if(!eligibleBarrel(actor))return;
    // The stock definition has neither flag. Native saves retain the marker
    // without changing the size of a serialized actor.
    if(actor->flags&MF_NOBLOOD) {
        if(actor->health<=0) {
            if(on&&actor->state!=&states[S_BEXP5])pending.push_back(actor);
            else if(!on)P_RemoveMobj(actor);
        } else if(!on) {
            actor->flags&=~featureFlags;
            actor->health=actor->info->spawnhealth;
        }
    } else if(on&&actor->health>0) {
        actor->flags|=featureFlags;
        actor->health=health;
    }
}

bool validVictim(const mobj_t *actor) {
    if(actor->health<=0||!(actor->flags&MF_SHOOTABLE)||
       (actor->flags&(MF_CORPSE|MF_MISSILE)))return false;
    return actor->player||(actor->flags&MF_COUNTKILL)||actor->type==MT_SKULL;
}

const sector_t *floorAt(fixed_t x,fixed_t y) {
    const auto *sub=R_PointInSubsector(x,y);
    if(!sub||!sub->sector)return nullptr;
    const auto *sector=sub->sector;
    if(sector->floorpic==skyflatnum||
       int64_t(sector->ceilingheight)-sector->floorheight<4LL*FRACUNIT)return nullptr;
    return sector;
}

boolean oilCanCross(intercept_t *hit) {
    const auto *line=hit->d.line;
    return line->backsector&&!(line->flags&ML_BLOCKING);
}

bool groundSight(fixed_t x,fixed_t y,fixed_t tx,fixed_t ty,fixed_t floor) {
    if(x==tx&&y==ty)return true;
    const auto z=fixed_t(std::min(int64_t(floor)+FRACUNIT,int64_t(INT32_MAX)-FRACUNIT));
    mobj_t start{},end{};
    start.x=x;start.y=y;start.z=z;start.height=FRACUNIT;
    start.subsector=R_PointInSubsector(x,y);
    end=start;end.x=tx;end.y=ty;end.subsector=R_PointInSubsector(tx,ty);
    return P_CheckSight(&start,&end)&&P_PathTraverse(x,y,tx,ty,PT_ADDLINES,oilCanCross);
}

// Clip each spoke to the owning floor's boundary, including self-referencing
// blocking walls. The renderer also clips the resulting decal to BSP floors.
void shape(p_oil_pool_view_t &view) {
    const auto &sector=sectors[view.sector];
    for(int n=0;n<OIL_SPOKES;++n) {
        const double a=tau*n/OIL_SPOKES;
        const double dx=std::cos(a),dy=std::sin(a);
        const double edge=.89+.052*std::sin(3*a+.8)+.045*std::sin(7*a+1.8);
        double limit=view.radius*edge;
        for(int i=0;i<sector.linecount;++i) {
            const auto &line=*sector.lines[i];
            if(line.frontsector==&sector&&line.backsector==&sector&&!(line.flags&ML_BLOCKING))continue;
            const double ex=double(line.v2->x)-line.v1->x,ey=double(line.v2->y)-line.v1->y;
            const double denom=dx*ey-dy*ex;
            if(std::abs(denom)<1e-6)continue;
            const double x=double(line.v1->x)-view.x,y=double(line.v1->y)-view.y;
            const double distance=(x*ey-y*ex)/denom;
            const double u=(x*dy-y*dx)/denom;
            if(distance>=0&&distance<limit&&u>=0&&u<=1)limit=distance;
        }
        view.reach[n]=(unsigned char)std::clamp(int(limit*255/view.radius),0,255);
    }
}

void sweep() {
    if(lastSweep==leveltime)return;
    lastSweep=leveltime;
    pools.erase(std::remove_if(pools.begin(),pools.end(),[](const Pool &p){return leveltime>=p.expires;}),pools.end());
}

Pool *addPool(fixed_t x,fixed_t y,int radius,const sector_t *sector,int kind,mobj_t *source,int lifetime) {
    if(!sector)return nullptr;
    sweep();
    if(pools.size()>=maxPools) {
        auto oldest=std::min_element(pools.begin(),pools.end(),[](const Pool &a,const Pool &b){
            if((a.view.kind==OIL_LEAK)!=(b.view.kind==OIL_LEAK))return a.view.kind==OIL_LEAK;
            return a.born<b.born;
        });
        if(kind==OIL_LEAK&&oldest->view.kind!=OIL_LEAK)return nullptr;
        pools.erase(oldest);
    }
    Pool pool;
    pool.view.x=x;pool.view.y=y;pool.view.radius=radius*FRACUNIT;
    pool.view.sector=int(sector-sectors);pool.view.kind=kind;
    pool.born=leveltime;pool.expires=leveltime+lifetime;pool.source=source;
    shape(pool.view);
    pools.push_back(pool);
    return &pools.back();
}

void leakOil(mobj_t *actor,const mobj_t *impact) {
    const auto *sector=floorAt(actor->x,actor->y);
    if(!sector||std::abs(int64_t(actor->z)-sector->floorheight)>4LL*FRACUNIT)return;
    sweep();
    auto found=std::find_if(pools.begin(),pools.end(),[actor](const Pool &p){
        return p.view.kind==OIL_LEAK&&p.view.x==actor->x&&p.view.y==actor->y;
    });
    Pool *leak=nullptr;
    if(found==pools.end())leak=addPool(actor->x,actor->y,leakRadius,sector,OIL_LEAK,nullptr,leakLife);
    else if(leveltime-found->born>=4) {
        leak=&*found;
        leak->born=leveltime;
        leak->expires=leveltime+leakLife;
        const auto radius=std::clamp(leak->view.radius+3*FRACUNIT,leakRadius*FRACUNIT,maxLeakRadius*FRACUNIT);
        if(radius!=leak->view.radius) {
            leak->view.radius=radius;
            shape(leak->view);
        }
    }
    if(leak)leak->view.leakAngle=impact?R_PointToAngle2(actor->x,actor->y,impact->x,impact->y):actor->angle;
}

bool stockExplosionFrames() {
    for(int i=0;i<5;++i)
        if(states[S_BEXP+i].sprite!=SPR_BEXP||states[S_BEXP+i].frame!=32768+i)return false;
    return true;
}

bool touches(const mobj_t *actor,const Pool &pool) {
    const auto &v=pool.view;
    const auto *sector=&sectors[v.sector];
    if(actor->subsector->sector!=sector||
       int64_t(sector->ceilingheight)-sector->floorheight<4LL*FRACUNIT||
       std::abs(int64_t(actor->z)-sector->floorheight)>4LL*FRACUNIT)return false;
    const double dx=double(actor->x)-v.x,dy=double(actor->y)-v.y;
    const double broad=double(v.radius)+actor->radius*.35;
    if(dx*dx+dy*dy>broad*broad)return false;
    const double a=std::atan2(dy,dx)*OIL_SPOKES/tau+OIL_SPOKES;
    const int s=int(a);
    const int i=s%OIL_SPOKES;
    const double edge=(v.reach[i]+(v.reach[(i+1)%OIL_SPOKES]-v.reach[i])*(a-s))*v.radius/255.0;
    const double reach=edge+actor->radius*.35;
    return dx*dx+dy*dy<=reach*reach&&groundSight(v.x,v.y,actor->x,actor->y,sector->floorheight);
}

void rupture(mobj_t *barrel) {
    auto *source=barrel->target;
    const auto *origin=floorAt(barrel->x,barrel->y);
    if(origin) {
        pools.erase(std::remove_if(pools.begin(),pools.end(),[barrel](const Pool &p){
            return p.view.kind==OIL_LEAK&&p.view.x==barrel->x&&p.view.y==barrel->y;
        }),pools.end());
        addPool(barrel->x,barrel->y,40,origin,OIL_RUPTURE,source,centralLife);
        const double direction=double(barrel->angle)*tau/4294967296.0;
        for(int n=0;n<3;++n) {
            const double a=direction+tau*n/3;
            const auto x=(fixed_t)std::clamp(int64_t(barrel->x)+std::llround(35*FRACUNIT*std::cos(a)),
                                             (long long)INT32_MIN,(long long)INT32_MAX);
            const auto y=(fixed_t)std::clamp(int64_t(barrel->y)+std::llround(35*FRACUNIT*std::sin(a)),
                                             (long long)INT32_MIN,(long long)INT32_MAX);
            const auto *floor=floorAt(x,y);
            if(floor&&floor->floorheight==origin->floorheight&&
               groundSight(barrel->x,barrel->y,x,y,floor->floorheight))
                addPool(x,y,18,floor,OIL_SPILL,source,spillLife+n*12);
        }
    }
    P_RadiusAttackFire(barrel,source,blast);
    barrel->target=nullptr;
    // This stock terminal frame has no action and leads directly to removal.
    // Keep its safe terminal state and display the stock BEXP artwork in
    // sequence without running the classic barrel's scream/acid/blast actions.
    const auto &end=states[S_BEXP5];
    if(end.sprite==SPR_BEXP&&end.tics==10&&!end.action.acv&&end.nextstate==S_NULL) {
        P_SetMobjState(barrel,S_BEXP5);
        if(stockExplosionFrames()) {
            barrel->flags|=2u<<MF_TRANSSHIFT; // The barrel's green contents become the brown oil ramp.
            barrel->tics=explosionTics;
            barrel->frame=states[S_BEXP].frame;
        }
        S_StartSound(barrel,sfx_barexp);
    } else {
        S_StartSound(nullptr,sfx_barexp);
        P_RemoveMobj(barrel);
    }
}

uint32_t read32(const unsigned char *p) {
    return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);
}
void write32(unsigned char *&p,uint32_t v) {
    for(int i=0;i<4;++i)*p++=(unsigned char)(v>>(8*i));
}
constexpr size_t headerAndTrailer=24,poolStride=32,exposureStride=16;
}

extern "C" int P_IncendiaryEnabled(void) {
    return enabled&&!netgame&&!demoplayback&&!demorecording&&!G_DemoLoading();
}
extern "C" void P_IncendiaryConfigure(int value) {
    enabled=value!=0;
    P_IncendiarySync();
}
extern "C" void P_IncendiarySync(void) {
    const bool on=P_IncendiaryEnabled()!=0;
    if(on==applied)return;
    applied=on;
    P_IncendiaryRestoreActors();
    if(!on)P_IncendiaryClear();
}
extern "C" void P_IncendiaryRestoreActors(void) {
    auto savedOrder=std::move(pending);
    pending.clear();
    const bool on=P_IncendiaryEnabled()!=0;
    forActors([on](mobj_t *actor){updateBarrel(actor,on);});
    if(on&&!savedOrder.empty()) {
        auto nativeOrder=std::move(pending);
        pending.clear();
        std::unordered_set<mobj_t*> valid(nativeOrder.begin(),nativeOrder.end());
        for(auto *actor:savedOrder)if(valid.erase(actor))pending.push_back(actor);
        for(auto *actor:nativeOrder)if(valid.erase(actor))pending.push_back(actor);
    }
}
extern "C" void P_IncendiarySpawn(mobj_t *actor) {
    if(P_IncendiaryEnabled()&&actor->type==MT_MISC77)updateBarrel(actor,true);
}
extern "C" int P_IncendiaryDamage(mobj_t *actor,mobj_t *inflictor,mobj_t *source,int damage) {
    if(actor->type!=MT_MISC77||!(actor->flags&MF_NOBLOOD)||!eligibleBarrel(actor))return 0;
    if(!P_IncendiaryEnabled()||actor->health<=0||damage<=0)return 1;
    if(damage>=actor->health) {
        actor->health=0;
        actor->flags&=~(MF_SHOOTABLE|MF_SOLID);
        actor->target=source==actor?nullptr:source;
        pending.push_back(actor);
    } else {
        actor->health-=damage;
        const bool atCenter=inflictor&&inflictor->x==actor->x&&inflictor->y==actor->y;
        leakOil(actor,inflictor&&!atCenter?inflictor:source);
    }
    return 1;
}
extern "C" void P_IncendiaryTick(mobj_t *actor) {
    if(!P_IncendiaryEnabled())return;
    if(actor->type==MT_MISC77&&actor->state==&states[S_BEXP5]&&actor->health==0&&
       (actor->flags&MF_NOBLOOD)&&eligibleBarrel(actor)&&stockExplosionFrames()) {
        const int age=explosionTics-actor->tics;
        const int frame=age<12?std::max(0,age/4):age<18?3:4;
        actor->frame=states[S_BEXP+frame].frame;
        return;
    }
    if(pools.empty()&&exposures.empty())return;
    sweep();
    if(!validVictim(actor)) {
        exposures.erase(actor);
        return;
    }
    const Pool *contact=nullptr;
    for(const auto &pool:pools)
        if(pool.view.kind!=OIL_LEAK&&touches(actor,pool)){
            contact=&pool;
            break;
        }
    auto it=exposures.find(actor);
    if(contact) {
        if(it==exposures.end()) {
            if(exposures.size()>=maxExposures)return;
            it=exposures.emplace(actor,Exposure{leveltime+linger,leveltime+interval,contact->source}).first;
        }
        it->second.until=leveltime+linger;
        it->second.source=contact->source;
    }
    if(it==exposures.end())return;
    if(leveltime>it->second.until) {
        exposures.erase(it);
        return;
    }
    if(leveltime<it->second.next)return;
    it->second.next=leveltime+interval;
    if(P_CombatDotKind(actor)!=1)P_DamageMobjFire(actor,nullptr,it->second.source,fireDamage);
}
extern "C" void P_IncendiaryEndTic(void) {
    if(!P_IncendiaryEnabled())return;
    // Take a snapshot of the wave size. Barrels hit by these blasts enter the
    // next wave, after the native radius traversal has returned.
    const auto count=std::min(pending.size(),size_t(maxBlastsPerTic));
    for(size_t i=0;i<count&&!pending.empty();++i) {
        auto *actor=pending.front();
        pending.pop_front();
        rupture(actor);
    }
}
extern "C" void P_IncendiaryForget(mobj_t *actor) {
    exposures.erase(actor);
    if(!pending.empty()) {
        pending.erase(std::remove(pending.begin(),pending.end(),actor),pending.end());
        for(auto *barrel:pending)if(barrel->target==actor)barrel->target=nullptr;
    }
    for(auto &pool:pools)if(pool.source==actor)pool.source=nullptr;
    for(auto &entry:exposures)if(entry.second.source==actor)entry.second.source=nullptr;
}
extern "C" void P_IncendiaryClear(void) {
    pending.clear();pools.clear();exposures.clear();lastSweep=-1;
}
extern "C" int P_IncendiaryPoolCount(void) {
    if(!P_IncendiaryEnabled())return 0;
    sweep();
    return int(pools.size());
}
extern "C" int P_IncendiaryPoolGet(int index,p_oil_pool_view_t *out) {
    if(!out||index<0||index>=P_IncendiaryPoolCount())return 0;
    const auto &pool=pools[size_t(index)];
    *out=pool.view;
    out->age=leveltime-pool.born;
    out->remaining=pool.expires-leveltime;
    return 1;
}

extern "C" size_t P_IncendiarySave(unsigned char *destination) {
    if(!P_IncendiaryEnabled())return 0;
    sweep();
    for(auto it=exposures.begin();it!=exposures.end();) {
        if(it->second.until<leveltime||!validVictim(it->first))it=exposures.erase(it);
        else ++it;
    }
    if(pools.empty()&&exposures.empty()&&pending.empty())return 0;
    const size_t bytes=headerAndTrailer+pools.size()*poolStride+exposures.size()*exposureStride+pending.size()*4;
    if(!destination)return bytes;
    std::unordered_map<mobj_t*,uint32_t> actors;
    uint32_t next=0;
    forActors([&](mobj_t *actor){actors.emplace(actor,++next);});
    auto ref=[&](mobj_t *actor) {
        auto it=actors.find(actor);return it==actors.end()?0u:it->second;
    };
    auto *p=destination;
    write32(p,1);write32(p,uint32_t(pools.size()));write32(p,uint32_t(exposures.size()));
    write32(p,uint32_t(pending.size()));
    for(const auto &pool:pools) {
        const auto &v=pool.view;
        for(uint32_t value:{uint32_t(v.x),uint32_t(v.y),uint32_t(v.radius),
              uint32_t(v.sector),uint32_t(pool.expires-leveltime),uint32_t(leveltime-pool.born),
              uint32_t(v.kind),ref(pool.source)})write32(p,value);
    }
    std::vector<std::array<uint32_t,4>> records;
    for(const auto &entry:exposures) {
        const auto &e=entry.second;
        records.push_back({ref(entry.first),uint32_t(e.until-leveltime),
                           uint32_t(std::max(0,e.next-leveltime)),ref(e.source)});
    }
    std::sort(records.begin(),records.end());
    for(const auto &r:records)for(uint32_t value:r)write32(p,value);
    for(auto *actor:pending)write32(p,ref(actor));
    write32(p,uint32_t(bytes));std::memcpy(p,"OI01",4);
    return bytes;
}

extern "C" size_t P_IncendiaryLoad(const unsigned char *data,size_t length) {
    P_IncendiaryClear();
    if(!data||length<headerAndTrailer||std::memcmp(data+length-4,"OI01",4))return 0;
    const auto bytes=read32(data+length-8);
    if(bytes<headerAndTrailer||bytes>length)return 0;
    auto *p=data+length-bytes;
    const auto count=read32(p+4),victims=read32(p+8),queued=read32(p+12);
    if(read32(p)!=1||count>maxPools||victims>maxExposures||queued>1000000||
       bytes!=headerAndTrailer+count*poolStride+victims*exposureStride+size_t(queued)*4||
       !P_IncendiaryEnabled())return bytes;
    std::vector<mobj_t*> actors;
    forActors([&](mobj_t *actor){actors.push_back(actor);});
    auto ref=[&](uint32_t id)->mobj_t*{return id&&id<=actors.size()?actors[id-1]:nullptr;};
    p+=16;
    for(uint32_t i=0;i<count;++i,p+=poolStride) {
        Pool pool;
        auto &v=pool.view;
        v.x=(fixed_t)read32(p);v.y=(fixed_t)read32(p+4);v.radius=(fixed_t)read32(p+8);
        const auto sector=read32(p+12),remaining=read32(p+16),age=read32(p+20),kind=read32(p+24);
        const bool validRadius=kind==OIL_LEAK?
            v.radius==11*FRACUNIT||(v.radius>=leakRadius*FRACUNIT&&v.radius<=maxLeakRadius*FRACUNIT):
            v.radius==(kind==OIL_RUPTURE?40:18)*FRACUNIT;
        if(sector>=unsigned(numsectors)||!remaining||remaining>centralLife||age>=centralLife||
           kind>OIL_SPILL||!validRadius||
           R_PointInSubsector(v.x,v.y)->sector!=&sectors[sector]||sectors[sector].floorpic==skyflatnum)continue;
        v.sector=int(sector);v.kind=int(kind);pool.born=leveltime-int(age);
        pool.expires=leveltime+int(remaining);pool.source=ref(read32(p+28));
        shape(v);pools.push_back(pool);
    }
    for(uint32_t i=0;i<victims;++i,p+=exposureStride) {
        auto *actor=ref(read32(p));
        const auto until=read32(p+4),next=read32(p+8);
        if(actor&&validVictim(actor)&&until<=linger&&next<=interval)
            exposures.emplace(actor,Exposure{leveltime+int(until),leveltime+int(next),ref(read32(p+12))});
    }
    std::unordered_set<mobj_t*> loaded;
    for(uint32_t i=0;i<queued;++i,p+=4) {
        auto *actor=ref(read32(p));
        if(actor&&actor->health==0&&(actor->flags&MF_NOBLOOD)&&!(actor->flags&MF_SHOOTABLE)&&
           actor->state!=&states[S_BEXP5]&&eligibleBarrel(actor)&&loaded.insert(actor).second)
            pending.push_back(actor);
    }
    return bytes;
}

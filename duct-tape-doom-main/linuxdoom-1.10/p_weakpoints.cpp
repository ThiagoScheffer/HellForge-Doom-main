#include <algorithm>
#include <cmath>
#include <climits>
#include <cstring>
#include <unordered_map>
#include <vector>
extern "C" {
#include "p_local.h"
#include "p_weakpoints.h"
#include "doomstat.h"
#include "m_random.h"
}
wp_balance_t p_wp_balance={200,175,150,150,150,200,175,150,200,300,150,150,125,
    30,25,8,105,105,30,35,175,96,128,3,7,10,50,30,210,2,20,40,350,128,512};
int p_wp_enhanced=0,p_wp_vulnerabilities=0,p_wp_injuries=0;
int p_wp_bleeding=0,p_wp_frenzy=0,p_wp_bleed_damage=0;
void (*P_WeakpointImpact)(const wp_damage_event_t*)=nullptr;
void (*P_WeakpointBleedVisual)(mobj_t*)=nullptr;
int (*P_WeakpointBulletLocate)(mobj_t*,mobj_t*,fixed_t,fixed_t,fixed_t,fixed_t,fixed_t,fixed_t,wp_impact_t*)=nullptr;
int (*P_WeakpointTargetAllowed)(const mobj_t*)=nullptr;
int (*P_WeakpointLocate)(mobj_t*,mobj_t*,wp_impact_t*)=nullptr;
namespace {
struct Runtime {
    wp_damage_kind_t kind=WP_OTHER;
    int aimUntil=0,aimCooldown=0,legUntil=0,legCooldown=0,offset=0;
    bool directProcessed=false,friendlyMissile=false;
    int bleedUntil=0,bleedNext=0,frenzyUntil=0;
    mobj_t *bleedSource=nullptr,*frenzyTarget=nullptr;
    float start[3]={},end[3]={};
    uint64_t shot=0;
};
std::unordered_map<mobj_t*,Runtime> runtime;
struct Pellets {int total=0,face=0;bool frenzyHead=false;};
std::unordered_map<mobj_t*,Pellets> pellets;
mobj_t *shotSource=nullptr;
weapontype_t shotWeapon=wp_pistol;
uint64_t serial=0,shotId=0;
bool allowed(const mobj_t *target) {
    return P_WeakpointActive()&&target&&!target->player&&target->health>0&&
        (target->flags&MF_SHOOTABLE)&&(!P_WeakpointTargetAllowed||P_WeakpointTargetAllowed(target));
}
bool roll(int chance) {return chance>=100||(chance>0&&P_Random()*100/256<chance);}
bool heavy(int type){return type==MT_FATSO||type==MT_CYBORG;}
bool mechanical(int type){return type==MT_BABY||type==MT_SPIDER;}
bool floating(int type){return type==MT_HEAD||type==MT_PAIN;}
}
int P_WeakpointActive(void){return !netgame&&!demoplayback&&!demorecording;}
int P_WeakpointHuman(int type){return type==MT_POSSESSED||type==MT_SHOTGUY||type==MT_CHAINGUY||type==MT_WOLFSS;}
int P_WeakpointProfile(int type) {
    switch(type) {
    case MT_TROOP:case MT_UNDEAD:case MT_VILE:case MT_KNIGHT:case MT_BRUISER:
    case MT_FATSO:case MT_CYBORG:case MT_SERGEANT:case MT_SHADOWS:case MT_HEAD:case MT_PAIN:case MT_BABY:case MT_SPIDER:return 1;
    default:return 0;
    }
}
int P_WeakpointRegionPercent(int type,wp_damage_kind_t kind,wp_region_t region,int confidence) {
    if(!p_wp_enhanced||confidence<100)return 100;
    bool shotgun=kind==WP_SHOTGUN_PELLET;
    if(kind!=WP_BULLET&&!shotgun)return 100;
    if(region==WP_HEAD) {
        if(type==MT_FATSO)return p_wp_balance.heavyHead;
        if(type==MT_CYBORG)return p_wp_balance.cyberHead;
        if(type==MT_TROOP||type==MT_UNDEAD||type==MT_VILE||type==MT_KNIGHT||type==MT_BRUISER)return p_wp_balance.humanoidHead;
    }
    if(shotgun&&region==WP_FACE&&(type==MT_SERGEANT||type==MT_SHADOWS))return p_wp_balance.facePellet;
    if(region==WP_EYE&&floating(type))return p_wp_balance.eyeBullet;
    if(shotgun&&region==WP_BRAIN&&mechanical(type))return p_wp_balance.brainPellet;
    return 100;
}
int P_WeakpointScale(int damage,int percent) {
    if(damage<=0)return damage;
    return (int)std::clamp<int64_t>(((int64_t)damage*std::clamp(percent,100,800)+50)/100,1,INT_MAX);
}
void P_WeakpointConfigure(int enhanced,int vulnerabilities,int injuries) {
    if(enhanced!=p_wp_enhanced||vulnerabilities!=p_wp_vulnerabilities||injuries!=p_wp_injuries) {
        for(auto it=runtime.begin();it!=runtime.end();) {
            auto &r=it->second;r.aimUntil=r.aimCooldown=r.legUntil=r.legCooldown=r.offset=0;
            if(r.kind==WP_OTHER&&!r.bleedUntil&&!r.frenzyUntil&&!r.friendlyMissile)it=runtime.erase(it);else ++it;
        }
    }
    p_wp_enhanced=enhanced;p_wp_vulnerabilities=vulnerabilities;p_wp_injuries=injuries;
    if(!enhanced&&p_wp_frenzy)P_WeakpointConfigureExtras(p_wp_bleeding,p_wp_frenzy);
}
wp_damage_kind_t P_WeakpointBulletKind(void){return shotSource&&(shotWeapon==wp_shotgun||shotWeapon==wp_supershotgun)?WP_SHOTGUN_PELLET:WP_BULLET;}
void P_WeakpointResolve(wp_damage_event_t *e) {
    if(e->resolved)return;e->resolved=1;e->eventId=++serial;if(!e->dischargeId)e->dischargeId=shotId;e->finalDamage=e->baseDamage;
    e->weapon=wp_nochange;e->projectileType=-1;
    if((e->kind==WP_BULLET||e->kind==WP_SHOTGUN_PELLET)&&e->source==shotSource)e->weapon=shotWeapon;
    else if(e->kind==WP_PLASMA_DIRECT){e->weapon=wp_plasma;e->projectileType=MT_PLASMA;}
    else if(e->kind==WP_ROCKET_DIRECT||e->kind==WP_ROCKET_SPLASH){e->weapon=wp_missile;e->projectileType=MT_ROCKET;}
    else if(e->kind==WP_BFG_DIRECT||e->kind==WP_BFG_TRACER){e->weapon=wp_bfg;e->projectileType=MT_BFG;}
    e->eligible=e->source&&e->source->player&&allowed(e->target);
    // The original human multiplier remains independent of the new switches.
    int region=std::max(100,e->regionPercent),weapon=100;bool fatal=false;
    if(e->eligible) {
        region=std::max(region,P_WeakpointRegionPercent(e->target->type,e->kind,e->impact.region,e->impact.confidence));
        if(p_wp_vulnerabilities) {
            int type=e->target->type;
            if(e->kind==WP_PLASMA_DIRECT) {
                if(type==MT_FATSO)weapon=p_wp_balance.heavyPlasma;
                else if(type==MT_CYBORG)weapon=p_wp_balance.cyberPlasma;
                else if(floating(type))weapon=p_wp_balance.floatingPlasma;
                else if(mechanical(type))weapon=p_wp_balance.mechanicalPlasma;
                else if(type==MT_SKULL)fatal=true;
            } else if(type==MT_SKULL&&(e->kind==WP_BFG_DIRECT||e->kind==WP_BFG_TRACER))fatal=true;
            else if(e->kind==WP_ROCKET_DIRECT) {
                if(heavy(type))weapon=p_wp_balance.heavyRocket;
                if(e->impact.confidence==100) {
                    if(mechanical(type)&&e->impact.region==WP_BRAIN)weapon=p_wp_balance.brainRocket;
                    else if(mechanical(type)&&e->impact.region==WP_LEGS)weapon=p_wp_balance.legRocket;
                    else if(floating(type)&&e->impact.region==WP_EYE)fatal=roll(p_wp_balance.eyeExecutionChance);
                }
            }
        }
        if(p_wp_enhanced&&p_wp_frenzy&&e->kind==WP_SHOTGUN_PELLET&&shotWeapon==wp_supershotgun&&shotSource==e->source&&
            (e->target->type==MT_FATSO||e->target->type==MT_KNIGHT||e->target->type==MT_BRUISER)&&
            e->impact.region==WP_HEAD&&e->impact.confidence==100&&e->impact.distance<=p_wp_balance.frenzyRange)
            pellets[e->target].frenzyHead=true;
        if(p_wp_enhanced&&e->kind==WP_SHOTGUN_PELLET&&shotSource==e->source&&
            (e->target->type==MT_SERGEANT||e->target->type==MT_SHADOWS)) {
            auto &p=pellets[e->target];++p.total;
            int range=shotWeapon==wp_supershotgun?p_wp_balance.ssgRange:p_wp_balance.shotgunRange;
            if(e->impact.region==WP_FACE&&e->impact.confidence==100&&e->impact.distance<=range)++p.face;
        }
    } else region=100;
    e->finalDamage=fatal?e->target->health:P_WeakpointScale(e->baseDamage,std::max(region,weapon));
}
void P_WeakpointAfterDamage(const wp_damage_event_t *e) {
    if(!e->target||e->target->health<=0){if(e->target)P_WeakpointForget(e->target);return;}
    if(!e->eligible||e->baseDamage<=0||e->finalDamage<=0)return;
    if(p_wp_bleeding&&e->weapon!=wp_nochange&&(e->kind==WP_BULLET||e->kind==WP_SHOTGUN_PELLET)&&
        (e->target->type==MT_TROOP||e->target->type==MT_KNIGHT||e->target->type==MT_FATSO||e->target->type==MT_SERGEANT)) {
        bool critical=p_wp_enhanced&&e->impact.confidence==100&&
            P_WeakpointRegionPercent(e->target->type,e->kind,e->impact.region,100)>100;
        if(critical||roll(p_wp_balance.bleedChance)) {
            auto &r=runtime[e->target];
            if(r.bleedUntil<=leveltime)r.bleedNext=leveltime+TICRATE;
            r.bleedUntil=leveltime+p_wp_balance.bleedDuration;r.bleedSource=e->source;
        }
    }
    if(!p_wp_injuries||e->impact.confidence<100)return;
    auto found=runtime.find(e->target);
    bool injuryRule=(p_wp_enhanced&&(e->kind==WP_BULLET||e->kind==WP_SHOTGUN_PELLET))||
        (p_wp_vulnerabilities&&(e->kind==WP_PLASMA_DIRECT||e->kind==WP_ROCKET_DIRECT));
    if(injuryRule&&heavy(e->target->type)&&e->impact.region==WP_HEAD&&e->finalDamage>e->baseDamage&&
        (int64_t)e->target->health*100<(int64_t)e->target->info->spawnhealth*p_wp_balance.lowHealthPercent&&
        (found==runtime.end()||leveltime>=found->second.aimCooldown)&&roll(p_wp_balance.aimChance)) {
        auto &r=runtime[e->target];r.aimUntil=leveltime+p_wp_balance.aimDuration;r.aimCooldown=leveltime+p_wp_balance.aimCooldown;
    }
    if(p_wp_vulnerabilities&&mechanical(e->target->type)&&e->kind==WP_ROCKET_DIRECT&&e->impact.region==WP_LEGS&&
        (found==runtime.end()||leveltime>=found->second.legCooldown)&&roll(p_wp_balance.legChance)) {
        auto &r=runtime[e->target];r.legUntil=leveltime+p_wp_balance.legDuration;
        r.legCooldown=r.legUntil+p_wp_balance.legCooldown;e->target->momx=e->target->momy=0;
    }
}
void P_WeakpointApply(wp_damage_event_t *e) {
    if(e->resolved>1)return;
    P_WeakpointResolve(e);P_WeakpointNotify(e);P_DamageMobj(e->target,e->inflictor,e->source,e->finalDamage);P_WeakpointAfterDamage(e);
    e->resolved=2;
}
void P_WeakpointBeginShot(mobj_t *source,weapontype_t weapon) {
    pellets.clear();shotSource=source;shotWeapon=weapon;shotId=++serial;
}
void P_WeakpointEndShot(void) {
    if(p_wp_enhanced&&P_WeakpointActive()) {
        // Move aside: death cleanup can erase the live bookkeeping map.
        auto completed=std::move(pellets);pellets.clear();
        int quota=shotWeapon==wp_supershotgun?p_wp_balance.ssgQuota:p_wp_balance.shotgunQuota;
        for(auto *t=thinkercap.next;t!=&thinkercap;t=t->next) {
            auto entryIt=completed.find((mobj_t*)t);if(entryIt==completed.end())continue;
            const auto &entry=*entryIt;
            if(allowed(entry.first)&&p_wp_frenzy&&entry.second.frenzyHead&&roll(p_wp_balance.frenzyChance)) {
                runtime[entry.first].frenzyUntil=leveltime+p_wp_balance.frenzyDuration;
                P_WeakpointFrenzyTarget(entry.first);
            }
            if(allowed(entry.first)&&entry.second.face>=quota&&
            entry.second.face*100>=entry.second.total*p_wp_balance.faceMajorityPercent) {
            wp_damage_event_t e{};e.target=entry.first;e.source=e.inflictor=shotSource;e.kind=WP_OTHER;
            e.baseDamage=e.finalDamage=e.target->health;e.resolved=1;
            e.weapon=shotWeapon;e.projectileType=-1;e.eventId=++serial;e.dischargeId=shotId;
            P_WeakpointApply(&e);
            }
        }
    }
    pellets.clear();shotSource=nullptr;shotId=0;
}
void P_WeakpointProjectileSpawn(mobj_t *projectile,mobj_t *source,wp_damage_kind_t kind) {
    if(!P_WeakpointActive()||!source||!source->player||kind==WP_OTHER)return;
    auto &r=runtime[projectile];r.kind=kind;r.shot=++serial;
    for(int i=0;i<3;++i)r.start[i]=r.end[i]=0;
    r.start[0]=r.end[0]=projectile->x/(float)FRACUNIT;
    r.start[1]=r.end[1]=projectile->y/(float)FRACUNIT;
    r.start[2]=r.end[2]=projectile->z/(float)FRACUNIT;
}
void P_WeakpointProjectileStep(mobj_t *projectile,fixed_t x,fixed_t y,fixed_t z,fixed_t nx,fixed_t ny,fixed_t nz) {
    if(!(projectile->flags&MF_MISSILE))return;
    auto it=runtime.find(projectile);if(it==runtime.end()||it->second.kind==WP_OTHER)return;
    auto &r=it->second;r.start[0]=x/(float)FRACUNIT;r.start[1]=y/(float)FRACUNIT;r.start[2]=z/(float)FRACUNIT;
    r.end[0]=nx/(float)FRACUNIT;r.end[1]=ny/(float)FRACUNIT;r.end[2]=nz/(float)FRACUNIT;
}
int P_WeakpointTrajectory(mobj_t *projectile,float start[3],float end[3]) {
    auto it=runtime.find(projectile);if(it==runtime.end())return 0;
    std::copy(it->second.start,it->second.start+3,start);std::copy(it->second.end,it->second.end+3,end);return 1;
}
void P_WeakpointProjectileHit(mobj_t *projectile,mobj_t *target,int damage) {
    wp_damage_event_t e{};e.target=target;e.inflictor=projectile;e.source=projectile->target;e.baseDamage=damage;
    auto found=runtime.find(projectile);
    if(found!=runtime.end()&&found->second.directProcessed)return;
    if(found!=runtime.end()&&!found->second.directProcessed) {
        found->second.directProcessed=true;e.kind=found->second.kind;e.dischargeId=found->second.shot;
        if(allowed(target)&&P_WeakpointLocate)P_WeakpointLocate(projectile,target,&e.impact);
    }
    P_WeakpointApply(&e);
}
void P_WeakpointBFGTracer(mobj_t *bfg,mobj_t *target,int damage) {
    wp_damage_event_t e{};e.target=target;e.source=e.inflictor=bfg->target;e.baseDamage=damage;
    auto it=runtime.find(bfg);e.kind=it!=runtime.end()&&it->second.kind==WP_BFG_DIRECT?WP_BFG_TRACER:WP_OTHER;
    if(it!=runtime.end())e.dischargeId=it->second.shot;
    P_WeakpointApply(&e);
}
int P_WeakpointImmobilized(mobj_t *actor) {
    if(!p_wp_injuries||!p_wp_vulnerabilities)return 0;
    auto it=runtime.find(actor);return it!=runtime.end()&&leveltime<it->second.legUntil&&allowed(actor);
}
void P_WeakpointAttackBegin(mobj_t *actor) {
    auto it=runtime.find(actor);if(it==runtime.end())return;auto &r=it->second;r.offset=0;
    if(allowed(actor)&&p_wp_injuries&&r.aimUntil>leveltime) {
        r.offset=(int)((int64_t)(P_Random()*2-255)*p_wp_balance.aimDegrees*(int64_t)(4294967296LL/360)/255);r.aimUntil=0;
    }
}
angle_t P_WeakpointAttackOffset(mobj_t *actor){if(!p_wp_injuries||!P_WeakpointActive())return 0;auto it=runtime.find(actor);return it==runtime.end()?0:(angle_t)it->second.offset;}
void P_WeakpointAttackEnd(mobj_t *actor){auto it=runtime.find(actor);if(it!=runtime.end())it->second.offset=0;}
void P_WeakpointForget(mobj_t *actor){
    runtime.erase(actor);pellets.erase(actor);
    for(auto &entry:runtime){if(entry.second.bleedSource==actor)entry.second.bleedSource=nullptr;
        if(entry.second.frenzyTarget==actor)entry.second.frenzyTarget=nullptr;}
}
void P_WeakpointClear(void){runtime.clear();pellets.clear();shotSource=nullptr;shotId=0;}
size_t P_WeakpointRuntimeCount(void){return runtime.size();}

void P_WeakpointNotify(wp_damage_event_t *e) {
    if(e->notified)return;e->notified=1;
    if(e->eligible&&e->baseDamage>0&&P_WeakpointImpact)P_WeakpointImpact(e);
}
void P_WeakpointConfigureExtras(int bleeding,int frenzy) {
    if(bleeding==p_wp_bleeding&&frenzy==p_wp_frenzy&&(p_wp_enhanced||!frenzy))return;
    p_wp_bleeding=bleeding;p_wp_frenzy=frenzy;
    for(auto &entry:runtime) {
        auto &r=entry.second;
        if(!bleeding){r.bleedUntil=r.bleedNext=0;r.bleedSource=nullptr;}
        if(!frenzy||!p_wp_enhanced) {
            if(r.frenzyUntil&&entry.first->target==r.frenzyTarget){entry.first->target=nullptr;entry.first->threshold=0;}
            r.frenzyUntil=0;r.frenzyTarget=nullptr;
        }
        if(!frenzy)r.friendlyMissile=false;
    }
}
int P_WeakpointBleeding(mobj_t *actor) {
    auto it=runtime.find(actor);return p_wp_bleeding&&allowed(actor)&&it!=runtime.end()&&leveltime<=it->second.bleedUntil&&it->second.bleedUntil>0;
}
fixed_t P_WeakpointWalkSpeed(mobj_t *actor,fixed_t speed) {
    return P_WeakpointBleeding(actor)?(fixed_t)((int64_t)speed*(100-p_wp_balance.bleedSlow)/100):speed;
}
int P_WeakpointFrenzied(mobj_t *actor) {
    auto it=runtime.find(actor);return p_wp_frenzy&&p_wp_enhanced&&allowed(actor)&&it!=runtime.end()&&leveltime<it->second.frenzyUntil;
}
static bool frenzyVictim(mobj_t *actor,mobj_t *other) {
    if(!other||other==actor||other->player||other->health<=0||!(other->flags&MF_SHOOTABLE)||
        !(other->flags&MF_COUNTKILL)&&other->type!=MT_SKULL)return false;
    double dx=(double)other->x-actor->x,dy=(double)other->y-actor->y;
    double range=(double)p_wp_balance.frenzySearch*FRACUNIT;
    return dx*dx+dy*dy<=range*range&&P_CheckSight(actor,other);
}
void P_WeakpointFrenzyTarget(mobj_t *actor) {
    auto it=runtime.find(actor);if(it==runtime.end())return;auto &r=it->second;
    if(!P_WeakpointFrenzied(actor)) {
        if(r.frenzyUntil){if(actor->target==r.frenzyTarget)actor->target=nullptr;actor->threshold=0;}
        r.frenzyUntil=0;r.frenzyTarget=nullptr;return;
    }
    if(!frenzyVictim(actor,r.frenzyTarget)) {
        r.frenzyTarget=nullptr;double nearest=INFINITY;
        for(auto *t=thinkercap.next;t!=&thinkercap;t=t->next) {
            if(t->function.acp1!=(actionf_p1)P_MobjThinker)continue;
            auto *other=(mobj_t*)t;
            double dx=(double)other->x-actor->x,dy=(double)other->y-actor->y,d=dx*dx+dy*dy;
            if(d<nearest&&frenzyVictim(actor,other)){nearest=d;r.frenzyTarget=other;}
        }
    }
    if(!r.frenzyTarget){r.frenzyUntil=0;actor->target=nullptr;actor->threshold=0;return;}
    actor->target=r.frenzyTarget;actor->threshold=100;
}
void P_WeakpointMonsterMissile(mobj_t *missile,mobj_t *actor) {
    if(P_WeakpointFrenzied(actor))runtime[missile].friendlyMissile=true;
}
int P_WeakpointFriendlyMissile(mobj_t *missile) {
    auto it=runtime.find(missile);return P_WeakpointActive()&&p_wp_frenzy&&it!=runtime.end()&&it->second.friendlyMissile;
}
void P_WeakpointTick(mobj_t *actor) {
    auto it=runtime.find(actor);if(it==runtime.end())return;
    if(!allowed(actor)) {
        if(it->second.kind==WP_OTHER&&!it->second.friendlyMissile)P_WeakpointForget(actor);
        return;
    }
    P_WeakpointFrenzyTarget(actor);
    it=runtime.find(actor);if(it==runtime.end())return;
    if(!p_wp_bleeding||it->second.bleedUntil<leveltime){it->second.bleedUntil=it->second.bleedNext=0;it->second.bleedSource=nullptr;}
    if(P_WeakpointBleeding(actor)) {
        if(leveltime>=it->second.bleedNext) {
            auto *source=it->second.bleedSource;it->second.bleedNext+=TICRATE;
            p_wp_bleed_damage=1;P_DamageMobj(actor,nullptr,source,p_wp_balance.bleedDamage);p_wp_bleed_damage=0;
            if(actor->health<=0){P_WeakpointForget(actor);return;}
        }
        if(P_WeakpointBleedVisual)P_WeakpointBleedVisual(actor);
    }
    it=runtime.find(actor);
    if(it!=runtime.end()) {
        const auto &r=it->second;
        if(r.kind==WP_OTHER&&!r.friendlyMissile&&!r.bleedUntil&&!r.frenzyUntil&&r.aimUntil<=leveltime&&
            r.aimCooldown<=leveltime&&r.legUntil<=leveltime&&r.legCooldown<=leveltime)runtime.erase(it);
    }
}

// Numeric tuning is deliberately config-only: the F4 panel exposes three switches.
namespace {
struct BalanceKey {const char *name;int *value;int low,high;};
BalanceKey keys[]={
#define WP_KEY(field,name,lo,hi) {"wp_" name,&p_wp_balance.field,lo,hi}
    WP_KEY(humanoidHead,"humanoid_head",100,800),WP_KEY(heavyHead,"heavy_head",100,800),WP_KEY(cyberHead,"cyber_head",100,800),WP_KEY(facePellet,"face_pellet",100,800),
    WP_KEY(eyeBullet,"eye_bullet",100,800),WP_KEY(brainPellet,"brain_pellet",100,800),WP_KEY(heavyPlasma,"heavy_plasma",100,800),WP_KEY(cyberPlasma,"cyber_plasma",100,800),
    WP_KEY(floatingPlasma,"floating_plasma",100,800),WP_KEY(mechanicalPlasma,"mechanical_plasma",100,800),WP_KEY(heavyRocket,"heavy_rocket",100,800),WP_KEY(brainRocket,"brain_rocket",100,800),WP_KEY(legRocket,"leg_rocket",100,800),
    WP_KEY(eyeExecutionChance,"eye_execution_chance",0,100),WP_KEY(aimChance,"aim_chance",0,100),WP_KEY(aimDegrees,"aim_degrees",0,30),WP_KEY(aimDuration,"aim_duration",1,350),WP_KEY(aimCooldown,"aim_cooldown",1,3500),
    WP_KEY(legChance,"leg_chance",0,100),WP_KEY(legDuration,"leg_duration",1,350),WP_KEY(legCooldown,"leg_cooldown",1,3500),WP_KEY(shotgunRange,"shotgun_range",1,1024),WP_KEY(ssgRange,"ssg_range",1,1024),WP_KEY(shotgunQuota,"shotgun_quota",1,7),WP_KEY(ssgQuota,"ssg_quota",1,20),WP_KEY(bleedChance,"bleed_chance",0,100),WP_KEY(bleedDuration,"bleed_duration",35,3500),WP_KEY(bleedDamage,"bleed_damage",1,100),WP_KEY(bleedSlow,"bleed_slow",0,90),
    WP_KEY(frenzyChance,"frenzy_chance",0,100),WP_KEY(frenzyDuration,"frenzy_duration",1,3500),WP_KEY(frenzyRange,"frenzy_range",1,1024),WP_KEY(frenzySearch,"frenzy_search",1,2048),
    WP_KEY(lowHealthPercent,"low_health_percent",0,100),WP_KEY(faceMajorityPercent,"face_majority_percent",1,100)
#undef WP_KEY
};
}
int P_WeakpointReadBalance(const char *key,double value) {
    for(auto &k:keys)if(!std::strcmp(key,k.name)){if(std::isfinite(value)&&value==std::floor(value)&&value>=k.low&&value<=k.high)*k.value=(int)value;return 1;}return 0;
}
void P_WeakpointWriteBalance(FILE *file){for(auto &k:keys)std::fprintf(file,"%s %d\n",k.name,*k.value);}

// Versioned trailer; actor IDs are thinker save order, never native pointers.
namespace {
void put(std::vector<unsigned char> &out,uint32_t n){for(int i=0;i<4;++i)out.push_back((unsigned char)(n>>(i*8)));}
uint32_t get(const unsigned char *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
std::vector<mobj_t*> actors(){std::vector<mobj_t*> out;for(auto *t=thinkercap.next;t!=&thinkercap;t=t->next)if(t->function.acp1==(actionf_p1)P_MobjThinker)out.push_back((mobj_t*)t);return out;}
}
size_t P_WeakpointSave(unsigned char *destination) {
    auto list=actors();std::vector<unsigned char> out;put(out,2);put(out,0);uint32_t count=0;
    for(size_t i=0;i<list.size();++i) {
        auto it=runtime.find(list[i]);if(it==runtime.end())continue;const auto &r=it->second;
        if(r.kind==WP_OTHER&&!r.friendlyMissile&&r.bleedUntil<=leveltime&&r.frenzyUntil<=leveltime&&r.aimUntil<=leveltime&&r.aimCooldown<=leveltime&&r.legUntil<=leveltime&&r.legCooldown<=leveltime)continue;
        ++count;put(out,(uint32_t)i+1);put(out,r.kind);put(out,r.directProcessed);
        put(out,(uint32_t)r.shot);put(out,(uint32_t)(r.shot>>32));
        for(int n:{r.aimUntil,r.aimCooldown,r.legUntil,r.legCooldown})put(out,(uint32_t)std::max(0,n-leveltime));
        for(float v:r.start){int32_t n=(int32_t)std::clamp<int64_t>(std::llround((double)v*FRACUNIT),INT_MIN,INT_MAX);put(out,(uint32_t)n);}
        for(float v:r.end){int32_t n=(int32_t)std::clamp<int64_t>(std::llround((double)v*FRACUNIT),INT_MIN,INT_MAX);put(out,(uint32_t)n);}
        for(int n:{r.bleedUntil,r.bleedNext,r.frenzyUntil})put(out,(uint32_t)std::max(0,n-leveltime));
        auto actorId=[&](mobj_t *actor)->uint32_t{auto found=std::find(list.begin(),list.end(),actor);return found==list.end()?0:(uint32_t)(found-list.begin()+1);};
        put(out,actorId(r.bleedSource));put(out,actorId(r.frenzyTarget));put(out,r.friendlyMissile);
    }
    for(int i=0;i<4;++i)out[4+i]=(unsigned char)(count>>(i*8));
    put(out,(uint32_t)(out.size()+8));out.insert(out.end(),{'W','P','0','1'});
    if(destination)std::memcpy(destination,out.data(),out.size());return out.size();
}
size_t P_WeakpointLoad(const unsigned char *data,size_t length) {
    P_WeakpointClear();if(length<16||std::memcmp(data+length-4,"WP01",4))return 0;
    size_t bytes=get(data+length-8);if(bytes<16||bytes>length)return 0;
    const auto *p=data+length-bytes;uint32_t count=get(p+4);
    uint32_t version=get(p);size_t stride=version==1?60:84;
    if((version!=1&&version!=2)||count>(bytes-16)/stride||bytes!=16+(size_t)count*stride)return bytes;
    auto list=actors();p+=8;
    for(uint32_t i=0;i<count;++i,p+=stride) {
        uint32_t id=get(p),kind=get(p+4);if(!id||id>list.size()||kind>WP_OTHER+7)continue;
        Runtime r;r.kind=(wp_damage_kind_t)kind;r.directProcessed=get(p+8)!=0;
        r.shot=(uint64_t)get(p+12)|((uint64_t)get(p+16)<<32);serial=std::max(serial,r.shot);
        int *timers[]={&r.aimUntil,&r.aimCooldown,&r.legUntil,&r.legCooldown};
        for(int k=0;k<4;++k)*timers[k]=leveltime+(int)std::min(get(p+20+k*4),3500u);
        for(int k=0;k<3;++k){r.start[k]=(int32_t)get(p+36+k*4)/(float)FRACUNIT;r.end[k]=(int32_t)get(p+48+k*4)/(float)FRACUNIT;}
        if(version==2) {
            int *extra[]={&r.bleedUntil,&r.bleedNext,&r.frenzyUntil};
            for(int k=0;k<3;++k){auto ticks=std::min(get(p+60+k*4),3500u);*extra[k]=ticks?leveltime+(int)ticks:0;}
            auto ref=[&](uint32_t n)->mobj_t*{return n&&n<=list.size()?list[n-1]:nullptr;};
            r.bleedSource=ref(get(p+72));r.frenzyTarget=ref(get(p+76));r.friendlyMissile=get(p+80)!=0;
        }
        runtime[list[id-1]]=r;
    }
    return bytes;
}

#include "../platform/monster_profile.h"
#include "../platform/monster_bench_state.h"
#include <algorithm>
#include <cmath>
#include <climits>
#include <cstring>
#include <unordered_map>
#include <vector>
extern "C" {
#include "p_local.h"
#include "p_arena.h"
#include "p_weakpoints.h"
#include "doomstat.h"
#include "m_random.h"
}
wp_balance_t p_wp_balance={200,175,150,150,150,200,175,150,200,300,150,150,125,
    30,25,8,105,105,30,35,175,96,128,3,7,10,50,30,210,2,20,40,350,128,512,3,4,280,350,15,25,105,70,30,60,384,16,5,10,35,105,175,128,14,8,32,175,175,64,3,15,350,70,15,200,200,512,350,120,350,50,24,96};
int p_wp_eyes=0;
int p_wp_enhanced=0,p_wp_vulnerabilities=0,p_wp_injuries=0;
int p_wp_bleeding=0,p_wp_frenzy=0,p_wp_bleed_damage=0,p_wp_feeding=0;
int (*P_WeakpointFoodPath)(mobj_t*,mobj_t*)=nullptr;
int (*P_WeakpointFoodMove)(mobj_t*)=nullptr;
int (*P_WeakpointFoodRetreat)(mobj_t*,mobj_t*)=nullptr;
void (*P_WeakpointFoodAnimate)(mobj_t*)=nullptr;
void (*P_WeakpointFoodWake)(mobj_t*)=nullptr;
void (*P_WeakpointFoodConsume)(mobj_t*)=nullptr;
void (*P_WeakpointPainVoice)(mobj_t*)=nullptr;
void (*P_WeakpointImpact)(const wp_damage_event_t*)=nullptr;
void (*P_WeakpointBleedVisual)(mobj_t*)=nullptr;
int (*P_WeakpointBulletLocate)(mobj_t*,mobj_t*,fixed_t,fixed_t,fixed_t,fixed_t,fixed_t,fixed_t,wp_impact_t*)=nullptr;
int (*P_WeakpointTargetAllowed)(const mobj_t*)=nullptr;
int (*P_WeakpointLocate)(mobj_t*,mobj_t*,wp_impact_t*)=nullptr;
namespace {
struct Runtime {
    int dotUntil=0,dotNext=0,dotKind=0,dotDamage=0,dotPlayer=-1;
    mobj_t *dotSource=nullptr;
    int burnUntil=0,burnKind=0,charred=0;
    int staggerUntil=0,staggerCooldown=0,baronUntil=0;
    int siphonUntil=0,siphonNext=0,siphonCooldown=0;
    bool blind=false,eyeMissile=false;
    int blindFire=0,eyeRageUntil=0,eyeSearch=0;
    mobj_t *eyeTarget=nullptr;
    wp_damage_kind_t kind=WP_OTHER;
    int aimUntil=0,aimCooldown=0,legUntil=0,legCooldown=0,offset=0;
    bool directProcessed=false,friendlyMissile=false;
    int bleedUntil=0,bleedNext=0,frenzyUntil=0;
    mobj_t *bleedSource=nullptr,*frenzyTarget=nullptr;
    int bleedTier=0,bleedVoiceNext=0,attackCredit=0,bleedPlayer=-1;
    uint64_t bleedDischarge=0;
    int feedPhase=0,feedUntil=0,feedCooldown=0,feedSearch=0;
    bool noEat=false;
    mobj_t *feedTarget=nullptr,*feedPrevious=nullptr,*feedDanger=nullptr;
    int feedAttemptUntil=0;
    mobj_t *retaliationTarget=nullptr,*retaliationVictim=nullptr;
    float start[3]={},end[3]={};
    uint64_t shot=0;
};
bool keepSurvival(const Runtime &r){return r.retaliationTarget||r.retaliationVictim;}
bool keepExpansion(const Runtime &r){return r.dotUntil>=leveltime&&r.dotKind||r.charred||r.burnUntil>leveltime||r.staggerCooldown>leveltime||r.baronUntil>leveltime||r.siphonUntil>leveltime||r.siphonCooldown>leveltime;}
std::unordered_map<mobj_t*,Runtime> runtime;
mobj_t *deathCleanup=nullptr;
struct Pellets {int total=0,face=0;bool frenzyHead=false,eye=false;};
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
struct DamageContext {const wp_damage_event_t *event;bool consumed;DamageContext *previous;};
DamageContext *damageContext=nullptr;
}
static void combatExpansionTick(mobj_t*);
static void combatExpansionReset();
int P_WeakpointBlind(mobj_t *actor) {
    if(!p_wp_eyes||!p_wp_enhanced||!allowed(actor)||!floating(actor->type))return 0;
    auto it=runtime.find(actor);return it!=runtime.end()&&it->second.blind;
}
int P_WeakpointEyeRage(mobj_t *actor) {
    if(!p_wp_eyes||!p_wp_enhanced||!allowed(actor)||actor->type!=MT_SPIDER)return 0;
    auto it=runtime.find(actor);return it!=runtime.end()&&leveltime<it->second.eyeRageUntil;
}
void P_WeakpointConfigureEyes(int enabled) {
    p_wp_eyes=enabled!=0;
    if(p_wp_eyes&&p_wp_enhanced)return;
    for(auto &entry:runtime){auto &r=entry.second;
        if(r.blind){P_WeakpointStopFeeding(entry.first);r.feedCooldown=r.feedSearch=0;entry.first->target=nullptr;entry.first->threshold=0;}
        if(r.eyeRageUntil&&entry.first->target==r.eyeTarget){entry.first->target=nullptr;entry.first->threshold=0;}
        r.blind=false;r.blindFire=r.eyeRageUntil=r.eyeSearch=0;r.eyeTarget=nullptr;r.eyeMissile=false;
    }
}
int P_WeakpointBlindFireDue(mobj_t *actor) {
    if(!P_WeakpointBlind(actor))return 0;auto &r=runtime[actor];
    if(leveltime<r.blindFire)return 0;r.blindFire=leveltime+p_wp_balance.blindFireInterval;return 1;
}
void P_WeakpointBlindAim(mobj_t *actor,angle_t *yaw,fixed_t *slope) {
    *yaw=actor->angle;*slope=0;if(!P_WeakpointBlind(actor))return;
    *yaw=((uint32_t)P_Random()<<24)|((uint32_t)P_Random()<<16);
    double pitch=(P_Random()*2.0/255.0-1.0)*p_wp_balance.blindPitch;
    *slope=(fixed_t)std::lround(std::tan(pitch*3.141592653589793/180)*FRACUNIT);actor->angle=*yaw;
}
int P_WeakpointAdjustDamage(mobj_t *target,mobj_t *inflictor,mobj_t *source,int damage) {
    if(p_wp_dot_damage||damage<=0||!p_wp_eyes||!p_wp_enhanced||!P_WeakpointActive())return damage;
    bool resolved=damageContext&&!damageContext->consumed&&damageContext->event->target==target&&
        damageContext->event->source==source&&damageContext->event->inflictor==inflictor&&damageContext->event->finalDamage==damage;
    if(resolved)damageContext->consumed=true;
    if(!resolved&&P_WeakpointEyeRage(target))damage=P_WeakpointScale(damage,p_wp_balance.eyeIncoming);
    if(target->player&&P_WeakpointEyeRage(source))damage=P_WeakpointScale(damage,p_wp_balance.eyePlayerDamage);
    return damage;
}
void P_WeakpointDealDamage(wp_damage_event_t *e) {
    DamageContext context{e,false,damageContext};damageContext=&context;
    P_DamageMobj(e->target,e->inflictor,e->source,e->finalDamage);damageContext=context.previous;
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
            if(!keepExpansion(r)&&!keepSurvival(r)&&r.kind==WP_OTHER&&!r.blind&&!r.eyeRageUntil&&!r.eyeMissile&&!r.bleedUntil&&!r.frenzyUntil&&!r.friendlyMissile&&!r.feedPhase&&!r.feedCooldown&&!r.feedSearch&&!r.noEat)it=runtime.erase(it);else ++it;
        }
    }
    p_wp_enhanced=enhanced;p_wp_vulnerabilities=vulnerabilities;p_wp_injuries=injuries;
    if(!enhanced&&p_wp_frenzy)P_WeakpointConfigureExtras(p_wp_bleeding,p_wp_frenzy);
    if(!enhanced)P_WeakpointConfigureEyes(p_wp_eyes);
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
    int incoming=P_WeakpointEyeRage(e->target)?p_wp_balance.eyeIncoming:100;
    e->finalDamage=fatal?e->target->health:P_WeakpointScale(e->baseDamage,std::max({region,weapon,incoming}));
}
void P_WeakpointAfterDamage(const wp_damage_event_t *e) {
    if(!e->target||e->target->health<=0){if(e->target&&!P_CombatCharred(e->target)&&!P_CombatVisualKind(e->target))P_WeakpointDied(e->target);return;}
    if(!e->eligible||e->baseDamage<=0||e->finalDamage<=0)return;
    if(p_wp_eyes&&p_wp_enhanced&&e->source==shotSource&&
        (e->weapon==wp_pistol||e->weapon==wp_chaingun||e->weapon==wp_shotgun||e->weapon==wp_supershotgun)&&
        (e->kind==WP_BULLET||e->kind==WP_SHOTGUN_PELLET)&&e->impact.confidence==100&&
        (floating(e->target->type)||e->target->type==MT_SPIDER)&&
        e->impact.eyeContact)pellets[e->target].eye=true;
    if(p_wp_bleeding&&e->weapon!=wp_nochange&&(e->kind==WP_BULLET||e->kind==WP_SHOTGUN_PELLET)&&
        (e->target->type==MT_TROOP||e->target->type==MT_KNIGHT||e->target->type==MT_FATSO||e->target->type==MT_SERGEANT||e->target->type==MT_BRUISER||e->target->type==MT_VILE)) {
        bool critical=p_wp_enhanced&&e->impact.confidence==100&&
            P_WeakpointRegionPercent(e->target->type,e->kind,e->impact.region,100)>100;
        if(critical||roll(p_wp_balance.bleedChance)) {
            auto &r=runtime[e->target];int oldTier=r.bleedUntil>=leveltime?r.bleedTier:0;
            uint64_t discharge=e->dischargeId?e->dischargeId:e->eventId;
            if(r.bleedUntil<leveltime||!r.bleedTier) {
                r.bleedTier=1;r.bleedNext=leveltime+TICRATE;r.attackCredit=0;
                r.bleedVoiceNext=leveltime+p_wp_balance.bleedVoice2;
            } else if(r.bleedDischarge!=discharge)r.bleedTier=std::min(3,r.bleedTier+1);
            r.bleedDischarge=discharge;
            if(critical&&r.bleedTier>oldTier)P_CombatMessage(HUD_CRITICAL_BLEED,e->target,discharge);
            int duration=r.bleedTier==3?p_wp_balance.bleedDuration3:r.bleedTier==2?p_wp_balance.bleedDuration2:p_wp_balance.bleedDuration;
            r.bleedUntil=leveltime+duration;r.bleedSource=e->source;r.bleedPlayer=-1;
            for(int i=0;i<MAXPLAYERS;++i)if(e->source->player==&players[i]){r.bleedPlayer=i;break;}
        }
    }
    P_CombatConfirmed(e->target,e->source,0,e->finalDamage,0);
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
    P_WeakpointResolve(e);P_WeakpointNotify(e);P_WeakpointDealDamage(e);P_WeakpointAfterDamage(e);
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
            if(allowed(entry.first)&&p_wp_eyes&&entry.second.eye&&!P_WeakpointBlind(entry.first)&&roll(p_wp_balance.eyeChance)){
                auto *actor=entry.first;bool priorEye=P_WeakpointEyeRage(actor)!=0;auto &r=runtime[actor];
                if(floating(actor->type)){P_WeakpointStopFeeding(actor);r.blind=true;r.blindFire=leveltime+p_wp_balance.blindFireInterval;}
                else {r.eyeRageUntil=leveltime+p_wp_balance.eyeRageDuration;r.eyeSearch=0;r.eyeTarget=nullptr;}
                if(!priorEye)P_CombatMessage(floating(actor->type)?HUD_EYE:HUD_EYE_RAGE,actor,shotId);
                actor->target=nullptr;actor->threshold=0;
                if(P_WeakpointFoodWake)P_WeakpointFoodWake(actor);
            }
            if(allowed(entry.first)&&p_wp_frenzy&&entry.second.frenzyHead&&roll(p_wp_balance.frenzyChance)) {
                bool wasFrenzy=P_WeakpointFrenzied(entry.first)!=0;
                runtime[entry.first].frenzyUntil=leveltime+p_wp_balance.frenzyDuration;
                P_WeakpointFrenzyTarget(entry.first);
                if(!wasFrenzy&&P_WeakpointFrenzied(entry.first))P_CombatMessage(HUD_FRENZY,entry.first,shotId);
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
    for(auto &entry:runtime){if(entry.second.dotSource==actor&&deathCleanup!=actor)entry.second.dotSource=nullptr;if(entry.second.bleedSource==actor)entry.second.bleedSource=nullptr;
        if(entry.second.retaliationTarget==actor){entry.second.retaliationTarget=nullptr;if(entry.first->target==actor){entry.first->target=nullptr;entry.first->threshold=0;}}
        if(entry.second.retaliationVictim==actor)entry.second.retaliationVictim=nullptr;
        if(entry.second.feedDanger==actor)entry.second.feedDanger=nullptr;
        if(entry.second.frenzyTarget==actor)entry.second.frenzyTarget=nullptr;
        if(entry.second.eyeTarget==actor){entry.second.eyeTarget=nullptr;if(entry.first->target==actor){entry.first->target=nullptr;entry.first->threshold=0;}}
        if(entry.second.feedPrevious==actor)entry.second.feedPrevious=nullptr;
        if(entry.second.feedTarget==actor){
            if(entry.first->target==actor){
                auto *previous=entry.second.feedPrevious;
                entry.first->target=!P_WeakpointFrenzied(entry.first)&&previous&&previous->health>0&&(previous->flags&MF_SHOOTABLE)?previous:nullptr;
            }
            entry.second.feedTarget=nullptr;
        }}
}
void P_WeakpointDied(mobj_t *actor){auto *previous=deathCleanup;deathCleanup=actor;P_WeakpointForget(actor);deathCleanup=previous;}
void P_WeakpointClear(void){combatExpansionReset();P_WeakpointFeedingReset();runtime.clear();pellets.clear();shotSource=nullptr;shotId=0;}
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
        if(!bleeding){r.staggerUntil=r.staggerCooldown=r.baronUntil=r.siphonUntil=r.siphonNext=r.siphonCooldown=0;r.bleedUntil=r.bleedNext=r.bleedTier=r.bleedVoiceNext=r.attackCredit=0;r.bleedDischarge=0;r.bleedSource=nullptr;r.bleedPlayer=-1;}
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
    if(P_WeakpointBleeding(actor))speed=(fixed_t)((int64_t)speed*(100-p_wp_balance.bleedSlow)/100);
    if((actor->type==MT_HEAD||actor->type==MT_SERGEANT)&&P_WeakpointFeeding(actor))speed=(fixed_t)std::min<int64_t>(INT_MAX,(int64_t)speed*p_wp_balance.feedSpeed/100);
    return P_CombatRage(actor)?(fixed_t)std::min<int64_t>(INT_MAX,(int64_t)speed*110/100):speed;
}
int P_WeakpointFrenzied(mobj_t *actor) {
    if(P_WeakpointEyeRage(actor))return 1;
    auto it=runtime.find(actor);return p_wp_frenzy&&p_wp_enhanced&&allowed(actor)&&it!=runtime.end()&&leveltime<it->second.frenzyUntil;
}
static bool retaliationPair(mobj_t*,mobj_t*);
static bool frenzyVictim(mobj_t *actor,mobj_t *other) {
    if(!other||other==actor||other->player||other->health<=0||!(other->flags&MF_SHOOTABLE)||
        !(other->flags&MF_COUNTKILL)&&other->type!=MT_SKULL)return false;
    double dx=(double)other->x-actor->x,dy=(double)other->y-actor->y;
    double range=(double)(P_WeakpointEyeRage(actor)?p_wp_balance.eyeSearchRange:p_wp_balance.frenzySearch)*FRACUNIT;
    return dx*dx+dy*dy<=range*range&&P_CheckSight(actor,other);
}
void P_WeakpointFrenzyTarget(mobj_t *actor) {
    MP_AUTO(MP_FRENZY);
    auto it=runtime.find(actor);if(it==runtime.end())return;auto &r=it->second;
    if(P_WeakpointEyeRage(actor)) {
        if(!frenzyVictim(actor,r.eyeTarget)) {
            r.eyeTarget=nullptr;
            if(leveltime>=r.eyeSearch){
                r.eyeSearch=leveltime+TICRATE;double nearest=INFINITY;
                for(auto *t=thinkercap.next;t!=&thinkercap;t=t->next){
                    if(t->function.acp1!=(actionf_p1)P_MobjThinker)continue;
                    auto *other=(mobj_t*)t;double dx=(double)other->x-actor->x,dy=(double)other->y-actor->y,d=dx*dx+dy*dy;
                    if(d<nearest&&frenzyVictim(actor,other)){nearest=d;r.eyeTarget=other;}
                }
            }
        }
        actor->target=r.eyeTarget;actor->threshold=r.eyeTarget?100:0;return;
    }
    if(r.eyeRageUntil){if(actor->target==r.eyeTarget)actor->target=nullptr;actor->threshold=0;r.eyeRageUntil=r.eyeSearch=0;r.eyeTarget=nullptr;}
    if(!P_WeakpointFrenzied(actor)) {
        if(r.frenzyUntil){if(actor->target==r.frenzyTarget&&!retaliationPair(actor,actor->target))actor->target=nullptr;actor->threshold=actor->target?100:0;}
        r.frenzyUntil=0;r.frenzyTarget=nullptr;return;
    }
    if(!frenzyVictim(actor,r.frenzyTarget)) {
        MP_COLLECT;
        r.frenzyTarget=nullptr;double nearest=INFINITY;
        for(auto *t=thinkercap.next;t!=&thinkercap;t=t->next) {
            if(t->function.acp1!=(actionf_p1)P_MobjThinker)continue;
            MP_LOCAL_COUNT(MP_FRENZY_CANDIDATE,1);auto *other=(mobj_t*)t;
            double dx=(double)other->x-actor->x,dy=(double)other->y-actor->y,d=dx*dx+dy*dy;
            if(d<nearest&&frenzyVictim(actor,other)){nearest=d;r.frenzyTarget=other;}
        }
    }
    if(!r.frenzyTarget){r.frenzyUntil=0;actor->target=nullptr;actor->threshold=0;return;}
    actor->target=r.frenzyTarget;actor->threshold=100;
}
static bool speciesPair(const mobj_t* a,const mobj_t* b){return a&&b&&(a->type==b->type||(a->type==MT_KNIGHT&&b->type==MT_BRUISER)||(a->type==MT_BRUISER&&b->type==MT_KNIGHT));}
static bool retaliationPair(mobj_t* a,mobj_t* b){
    if(!allowed(a)||!allowed(b)||a==b||!speciesPair(a,b))return false;
    auto x=runtime.find(a),y=runtime.find(b);return (x!=runtime.end()&&x->second.retaliationTarget==b)||(y!=runtime.end()&&y->second.retaliationTarget==a);
}
void P_WeakpointMonsterMissile(mobj_t *missile,mobj_t *actor) {
    if(P_WeakpointEyeRage(actor))runtime[missile].eyeMissile=true;
    else if(P_WeakpointFrenzied(actor))runtime[missile].friendlyMissile=true;
    if(retaliationPair(actor,actor->target))runtime[missile].retaliationVictim=actor->target;
}
int P_WeakpointFriendlyMissile(mobj_t *missile) {
    auto it=runtime.find(missile);return P_WeakpointActive()&&it!=runtime.end()&&((p_wp_frenzy&&it->second.friendlyMissile)||(p_wp_eyes&&p_wp_enhanced&&it->second.eyeMissile));
}
int P_WeakpointCanHitSpecies(mobj_t* missile,mobj_t* victim){
    if(!missile||!victim||missile->target==victim||!P_WeakpointActive())return 0;
    if(P_WeakpointFriendlyMissile(missile)||retaliationPair(missile->target,victim))return 1;
    auto it=runtime.find(missile);return allowed(victim)&&it!=runtime.end()&&it->second.retaliationVictim==victim;
}
void P_WeakpointRetaliationDamage(mobj_t* victim,mobj_t* missile,mobj_t* source){
    if(!allowed(victim)||!allowed(source)||source==victim||!speciesPair(source,victim))return;
    if(P_WeakpointFrenzied(source)||P_WeakpointCanHitSpecies(missile,victim)||retaliationPair(source,victim))runtime[victim].retaliationTarget=source;
}
void P_WeakpointTick(mobj_t *actor) {
    combatExpansionTick(actor);
    if(p_wp_feeding||P_WeakpointBlind(actor))P_WeakpointFeedingTick(actor);
    auto it=runtime.find(actor);if(it==runtime.end())return;
    if(!allowed(actor)) {
        if(!keepExpansion(it->second)&&!keepSurvival(it->second)&&it->second.kind==WP_OTHER&&!it->second.eyeMissile&&!it->second.friendlyMissile&&!it->second.noEat)P_WeakpointForget(actor);
        return;
    }
    P_WeakpointFrenzyTarget(actor);
    it=runtime.find(actor);if(it==runtime.end())return;
    if(!p_wp_bleeding||it->second.bleedUntil<leveltime){it->second.bleedUntil=it->second.bleedNext=it->second.bleedTier=it->second.attackCredit=0;it->second.bleedSource=nullptr;it->second.bleedPlayer=-1;}
    if(P_WeakpointBleeding(actor)) {
        if(leveltime>=it->second.bleedNext) {
            auto *source=it->second.bleedSource;
            if(!source&&it->second.bleedPlayer>=0)source=players[it->second.bleedPlayer].mo;
            it->second.bleedNext+=TICRATE;
            p_wp_bleed_damage=1;P_DamageMobj(actor,nullptr,source,P_WeakpointBleedDamage(actor));p_wp_bleed_damage=0;
            if(actor->health<=0){if(!P_CombatCharred(actor)&&!P_CombatVisualKind(actor))P_WeakpointDied(actor);return;}
        }
        it=runtime.find(actor);
        if(it!=runtime.end()&&it->second.bleedTier>=2&&leveltime>=it->second.bleedVoiceNext) {
            it->second.bleedVoiceNext=leveltime+(it->second.bleedTier==3?p_wp_balance.bleedVoice3:p_wp_balance.bleedVoice2);
            if(P_WeakpointPainVoice)P_WeakpointPainVoice(actor);
        }
        if(P_WeakpointBleedVisual)P_WeakpointBleedVisual(actor);
    }
    it=runtime.find(actor);
    if(it!=runtime.end()) {
        const auto &r=it->second;
        if(!keepExpansion(r)&&!keepSurvival(r)&&r.kind==WP_OTHER&&!r.blind&&!r.eyeRageUntil&&!r.eyeMissile&&!r.friendlyMissile&&!r.noEat&&!r.bleedUntil&&!r.frenzyUntil&&!r.feedPhase&&r.feedCooldown<=leveltime&&r.feedSearch<=leveltime&&r.aimUntil<=leveltime&&
            r.aimCooldown<=leveltime&&r.legUntil<=leveltime&&r.legCooldown<=leveltime)runtime.erase(it);
    }
}

int P_WeakpointBleedTier(mobj_t *actor) {
    auto it=runtime.find(actor);return P_WeakpointBleeding(actor)&&it!=runtime.end()?it->second.bleedTier:0;
}
int P_WeakpointBleedDamage(mobj_t *actor) {
    int tier=P_WeakpointBleedTier(actor);return tier==3?p_wp_balance.bleedDamage3:tier==2?p_wp_balance.bleedDamage2:p_wp_balance.bleedDamage;
}
int P_WeakpointBleedOpportunity(mobj_t *actor) {
    int tier=P_WeakpointBleedTier(actor);bool rage=P_CombatRage(actor)!=0;
    if((tier<2&&!rage)||!actor->target||P_WeakpointFeeding(actor))return 0;
    int bonus=tier==3?p_wp_balance.bleedAttack3:tier==2?p_wp_balance.bleedAttack2:0;
    auto &r=runtime[actor];r.attackCredit+=std::max(bonus,rage?10:0);
    if(r.attackCredit<100)return 0;r.attackCredit-=100;return 1;
}
#include "p_feeding.inc"
#include "p_combat_hud.inc"
#include "p_elemental.inc"

// Numeric tuning is config-only; F4 exposes feature controls.
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
    WP_KEY(bleedDamage2,"bleed_damage_2",1,100),WP_KEY(bleedDamage3,"bleed_damage_3",1,100),WP_KEY(bleedDuration2,"bleed_duration_2",35,3500),WP_KEY(bleedDuration3,"bleed_duration_3",35,3500),
    WP_KEY(bleedAttack2,"bleed_attack_2",0,50),WP_KEY(bleedAttack3,"bleed_attack_3",0,50),WP_KEY(bleedVoice2,"bleed_voice_2",35,3500),WP_KEY(bleedVoice3,"bleed_voice_3",35,3500),
    WP_KEY(feedStart,"feed_start",1,50),WP_KEY(feedStop,"feed_stop",51,100),WP_KEY(feedRadius,"feed_radius",64,1024),WP_KEY(feedBite,"feed_bite",1,100),
    WP_KEY(feedWindow,"feed_window",35,1050),WP_KEY(feedSpeed,"feed_speed",100,150),WP_KEY(feedFailureCooldown,"feed_failure_cooldown",35,3500),WP_KEY(feedVileStart,"feed_vile_start",1,50),WP_KEY(feedVileDamage,"feed_vile_damage",1,100),WP_KEY(feedVileReach,"feed_vile_reach",32,128),
    WP_KEY(feedLiveHeal,"feed_live_heal",1,20),WP_KEY(feedCorpseHeal,"feed_corpse_heal",1,25),WP_KEY(feedSearch,"feed_search",7,350),WP_KEY(feedPursuit,"feed_pursuit",35,350),WP_KEY(feedCooldown,"feed_cooldown",35,3500),WP_KEY(feedThreat,"feed_threat",0,256),
    WP_KEY(sparkBullet,"metal_sparks_bullet",1,32),WP_KEY(sparkPellet,"metal_sparks_pellet",1,16),WP_KEY(sparkCap,"metal_sparks_cap",1,64),WP_KEY(sparkScale,"metal_spark_scale",100,300),
    WP_KEY(headBloodScale,"head_blood_scale",100,300),WP_KEY(headWallReach,"head_wall_reach",1,96),WP_KEY(headWallMarks,"head_wall_marks",0,3),
    WP_KEY(eyeChance,"eye_injury_chance",0,100),WP_KEY(eyeRageDuration,"eye_rage_duration",35,3500),WP_KEY(blindFireInterval,"blind_fire_interval",35,350),
    WP_KEY(blindPitch,"blind_fire_pitch",0,30),WP_KEY(eyeIncoming,"eye_rage_incoming",100,800),WP_KEY(eyePlayerDamage,"eye_rage_player_damage",100,800),
    WP_KEY(eyeSearchRange,"eye_rage_search",64,2048),
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
    auto list=actors();std::vector<unsigned char> out;put(out,7);put(out,0);uint32_t count=0;
    for(size_t i=0;i<list.size();++i) {
        auto it=runtime.find(list[i]);if(it==runtime.end())continue;const auto &r=it->second;
        if(!keepExpansion(r)&&!keepSurvival(r)&&r.kind==WP_OTHER&&!r.blind&&!r.eyeRageUntil&&!r.eyeMissile&&!r.friendlyMissile&&!r.noEat&&!r.feedPhase&&r.feedCooldown<=leveltime&&r.feedSearch<=leveltime&&r.bleedUntil<=leveltime&&r.frenzyUntil<=leveltime&&r.aimUntil<=leveltime&&r.aimCooldown<=leveltime&&r.legUntil<=leveltime&&r.legCooldown<=leveltime)continue;
        ++count;put(out,(uint32_t)i+1);put(out,r.kind);put(out,r.directProcessed);
        put(out,(uint32_t)r.shot);put(out,(uint32_t)(r.shot>>32));
        for(int n:{r.aimUntil,r.aimCooldown,r.legUntil,r.legCooldown})put(out,(uint32_t)std::max(0,n-leveltime));
        for(float v:r.start){int32_t n=(int32_t)std::clamp<int64_t>(std::llround((double)v*FRACUNIT),INT_MIN,INT_MAX);put(out,(uint32_t)n);}
        for(float v:r.end){int32_t n=(int32_t)std::clamp<int64_t>(std::llround((double)v*FRACUNIT),INT_MIN,INT_MAX);put(out,(uint32_t)n);}
        for(int n:{r.bleedUntil,r.bleedNext,r.frenzyUntil})put(out,(uint32_t)std::max(0,n-leveltime));
        auto actorId=[&](mobj_t *actor)->uint32_t{auto found=std::find(list.begin(),list.end(),actor);return found==list.end()?0:(uint32_t)(found-list.begin()+1);};
        put(out,actorId(r.bleedSource));put(out,actorId(r.frenzyTarget));put(out,r.friendlyMissile);
        put(out,r.bleedTier);put(out,(uint32_t)r.bleedDischarge);put(out,(uint32_t)(r.bleedDischarge>>32));
        put(out,std::max(0,r.bleedVoiceNext-leveltime));put(out,r.attackCredit);
        put(out,r.feedPhase);put(out,actorId(r.feedTarget));put(out,actorId(r.feedPrevious));
        for(int timer:{r.feedUntil,r.feedCooldown,r.feedSearch})put(out,std::max(0,timer-leveltime));
        put(out,r.noEat);put(out,(uint32_t)(r.bleedPlayer+1));
        put(out,r.blind);put(out,std::max(0,r.blindFire-leveltime));put(out,std::max(0,r.eyeRageUntil-leveltime));
        put(out,std::max(0,r.eyeSearch-leveltime));put(out,actorId(r.eyeTarget));put(out,r.eyeMissile);
        put(out,std::max(0,r.burnUntil-leveltime));put(out,r.burnKind);put(out,r.charred);
        for(int n:{r.staggerUntil,r.staggerCooldown,r.baronUntil,r.siphonUntil,r.siphonNext,r.siphonCooldown})put(out,std::max(0,n-leveltime));
        put(out,std::max(0,r.dotUntil-leveltime));put(out,std::max(0,r.dotNext-leveltime));put(out,r.dotKind);put(out,r.dotDamage);
        put(out,actorId(r.dotSource));put(out,r.dotPlayer+1);
        put(out,std::max(0,r.feedAttemptUntil-leveltime));put(out,actorId(r.feedDanger));put(out,actorId(r.retaliationTarget));put(out,actorId(r.retaliationVictim));
    }
    for(int i=0;i<4;++i)out[4+i]=(unsigned char)(count>>(i*8));
    put(out,(uint32_t)(out.size()+8));out.insert(out.end(),{'W','P','0','1'});
    if(destination)std::memcpy(destination,out.data(),out.size());return out.size();
}
size_t P_WeakpointLoad(const unsigned char *data,size_t length) {
    P_WeakpointClear();if(length<16||std::memcmp(data+length-4,"WP01",4))return 0;
    size_t bytes=get(data+length-8);if(bytes<16||bytes>length)return 0;
    const auto *p=data+length-bytes;uint32_t count=get(p+4);
    uint32_t version=get(p);size_t stride=version==1?60:version==2?84:version==3?136:version==4?160:version==5?196:version==6?220:236;
    if((version!=1&&version!=2&&version!=3&&version!=4&&version!=5&&version!=6&&version!=7)||count>(bytes-16)/stride||bytes!=16+(size_t)count*stride)return bytes;
    auto list=actors();p+=8;
    for(uint32_t i=0;i<count;++i,p+=stride) {
        uint32_t id=get(p),kind=get(p+4);if(!id||id>list.size()||kind>WP_OTHER+7)continue;
        Runtime r;r.kind=(wp_damage_kind_t)kind;r.directProcessed=get(p+8)!=0;
        r.shot=(uint64_t)get(p+12)|((uint64_t)get(p+16)<<32);serial=std::max(serial,r.shot);
        int *timers[]={&r.aimUntil,&r.aimCooldown,&r.legUntil,&r.legCooldown};
        for(int k=0;k<4;++k)*timers[k]=leveltime+(int)std::min(get(p+20+k*4),3500u);
        for(int k=0;k<3;++k){r.start[k]=(int32_t)get(p+36+k*4)/(float)FRACUNIT;r.end[k]=(int32_t)get(p+48+k*4)/(float)FRACUNIT;}
        if(version>=2) {
            int *extra[]={&r.bleedUntil,&r.bleedNext,&r.frenzyUntil};
            for(int k=0;k<3;++k){auto ticks=std::min(get(p+60+k*4),3500u);*extra[k]=ticks?leveltime+(int)ticks:0;}
            auto ref=[&](uint32_t n)->mobj_t*{return n&&n<=list.size()?list[n-1]:nullptr;};
            r.bleedSource=ref(get(p+72));r.frenzyTarget=ref(get(p+76));r.friendlyMissile=get(p+80)!=0;
        }
        if(version>=3) {
            r.bleedTier=(int)std::min(get(p+84),3u);r.bleedDischarge=(uint64_t)get(p+88)|((uint64_t)get(p+92)<<32);
            serial=std::max(serial,r.bleedDischarge);r.bleedVoiceNext=leveltime+(int)std::min(get(p+96),3500u);r.attackCredit=(int)std::min(get(p+100),99u);
            r.feedPhase=(int)std::min(get(p+104),version>=7?4u:3u);
            auto ref=[&](uint32_t n)->mobj_t*{return n&&n<=list.size()?list[n-1]:nullptr;};
            r.feedTarget=ref(get(p+108));r.feedPrevious=ref(get(p+112));
            int *timers3[]={&r.feedUntil,&r.feedCooldown,&r.feedSearch};
            for(int k=0;k<3;++k){uint32_t remain=std::min(get(p+116+k*4),3500u);*timers3[k]=remain?leveltime+(int)remain:0;}
            r.noEat=get(p+128)!=0;r.bleedPlayer=get(p+132)>0&&get(p+132)<=MAXPLAYERS?(int)get(p+132)-1:-1;
            if(r.bleedTier&&r.bleedUntil==0)r.bleedUntil=leveltime;
            if(!r.feedTarget&&r.feedPhase!=4)r.feedPhase=0;
        } else if(r.bleedUntil>=leveltime&&r.bleedUntil>0)r.bleedTier=1;
        if(version>=4){
            r.blind=get(p+136)!=0;r.blindFire=leveltime+(int)std::min(get(p+140),3500u);
            auto remain=std::min(get(p+144),3500u);r.eyeRageUntil=remain?leveltime+(int)remain:0;
            r.eyeSearch=leveltime+(int)std::min(get(p+148),3500u);
            uint32_t target=get(p+152);r.eyeTarget=target&&target<=list.size()?list[target-1]:nullptr;r.eyeMissile=get(p+156)!=0;
        }
        if(version>=5){
            auto timer=[&](int offset){auto t=std::min(get(p+offset),3500u);return t?leveltime+(int)t:0;};
            r.burnUntil=timer(160);r.burnKind=(int)std::min(get(p+164),2u);r.charred=get(p+168)!=0;
            int *more[]={&r.staggerUntil,&r.staggerCooldown,&r.baronUntil,&r.siphonUntil,&r.siphonNext,&r.siphonCooldown};
            for(int j=0;j<6;++j)*more[j]=timer(172+j*4);
        }
        if(version>=6){
            auto t=std::min(get(p+196),210u);r.dotUntil=leveltime+(int)t;
            r.dotNext=leveltime+(int)std::min(get(p+200),35u);r.dotKind=(int)std::min(get(p+204),2u);r.dotDamage=(int)std::min(get(p+208),5u);
            uint32_t source=get(p+212);r.dotSource=source&&source<=list.size()?list[source-1]:nullptr;
            r.dotPlayer=get(p+216)>0&&get(p+216)<=MAXPLAYERS?(int)get(p+216)-1:-1;
        }
        if(version>=7){
            auto remain=std::min(get(p+220),3500u);r.feedAttemptUntil=remain?leveltime+(int)remain:0;
            auto ref=[&](uint32_t id)->mobj_t*{return id&&id<=list.size()?list[id-1]:nullptr;};
            r.feedDanger=ref(get(p+224));r.retaliationTarget=ref(get(p+228));r.retaliationVictim=ref(get(p+232));
        }else if(r.feedPhase)r.feedAttemptUntil=leveltime+p_wp_balance.feedWindow;
        auto *actor=list[id-1];
        if(!p_wp_dot||!dotEligible(actor)||r.dotUntil<leveltime||!r.dotKind||!r.dotDamage){r.dotUntil=r.dotNext=r.dotKind=r.dotDamage=0;r.dotSource=nullptr;r.dotPlayer=-1;}
        if(!p_wp_elemental||!elementalEligible(actor)){r.burnUntil=r.burnKind=r.charred=0;}
        if(!p_wp_reactions||!allowed(actor)){r.staggerUntil=r.staggerCooldown=r.baronUntil=r.siphonUntil=r.siphonNext=r.siphonCooldown=0;}
        if(!floating(actor->type)||!allowed(actor)){r.blind=false;r.blindFire=0;}
        if(actor->type!=MT_SPIDER||!allowed(actor)){r.eyeRageUntil=r.eyeSearch=0;r.eyeTarget=nullptr;}
        if(!(actor->flags&MF_MISSILE))r.eyeMissile=false;
        if(!p_wp_eyes||!p_wp_enhanced){
            if((r.blind&&actor->target==r.feedTarget)||(r.eyeRageUntil&&actor->target==r.eyeTarget)){actor->target=nullptr;actor->threshold=0;}
            if(r.blind){r.feedPhase=r.feedUntil=r.feedCooldown=r.feedSearch=r.feedAttemptUntil=0;r.feedTarget=r.feedPrevious=r.feedDanger=nullptr;}
            r.blind=false;r.blindFire=r.eyeRageUntil=r.eyeSearch=0;r.eyeTarget=nullptr;r.eyeMissile=false;
        }
        // A save cannot re-arm statuses whose current feature switch is off.
        if(!p_wp_bleeding){r.bleedUntil=r.bleedNext=r.bleedTier=r.bleedVoiceNext=r.attackCredit=0;r.bleedDischarge=0;r.bleedSource=nullptr;r.bleedPlayer=-1;}
        if(!p_wp_feeding&&!r.blind){
            if(r.feedPhase&&actor->target==r.feedTarget)actor->target=r.feedPrevious&&r.feedPrevious->health>0&&(r.feedPrevious->flags&MF_SHOOTABLE)?r.feedPrevious:nullptr;
            r.feedPhase=r.feedUntil=r.feedCooldown=r.feedSearch=r.feedAttemptUntil=0;r.feedTarget=r.feedPrevious=r.feedDanger=nullptr;
        }
        if(!p_wp_frenzy||!p_wp_enhanced){if(r.frenzyUntil&&actor->target==r.frenzyTarget){actor->target=nullptr;actor->threshold=0;}r.frenzyUntil=0;r.frenzyTarget=nullptr;}
        if(!p_wp_frenzy)r.friendlyMissile=false;
        if(!P_WeakpointActive()||!allowed(actor)){r.retaliationTarget=nullptr;if(!(actor->flags&MF_MISSILE))r.retaliationVictim=nullptr;}
        if(r.retaliationTarget&&(!allowed(r.retaliationTarget)||!speciesPair(actor,r.retaliationTarget)))r.retaliationTarget=nullptr;
        if(r.retaliationVictim&&(!(actor->flags&MF_MISSILE)||!allowed(r.retaliationVictim)))r.retaliationVictim=nullptr;
        runtime[actor]=r;
    }
    return bytes;
}
#ifdef DOOM_MONSTER_BENCHMARK
void MP_StateCombat(MP_StateHash *h){
    MP_StateField(h,serial);MP_StateField(h,shotId);MP_StateActor(h,shotSource);MP_StateField(h,shotWeapon);
    MP_StateField(h,feedBudgetTic);MP_StateField(h,feedBudget);
    for(size_t i=0;i<h->count;++i){
        auto *actor=(mobj_t*)h->actor_at(h->context,i);auto found=runtime.find(actor);
        MP_StateField(h,found!=runtime.end());
        if(found!=runtime.end()){
            const auto &r=found->second;
            for(int value:{(int)r.kind,r.aimUntil,r.aimCooldown,r.legUntil,r.legCooldown,r.offset,(int)r.directProcessed,(int)r.friendlyMissile,
                          r.bleedUntil,r.bleedNext,r.frenzyUntil,r.bleedTier,r.bleedVoiceNext,r.attackCredit,r.bleedPlayer,
                          r.feedPhase,r.feedUntil,r.feedCooldown,r.feedSearch,r.feedAttemptUntil,(int)r.noEat,(int)r.blind,r.blindFire,r.eyeRageUntil,r.eyeSearch,(int)r.eyeMissile,r.burnUntil,r.burnKind,r.charred,r.dotUntil,r.dotNext,r.dotKind,r.dotDamage,r.dotPlayer,r.staggerUntil,r.staggerCooldown,r.baronUntil,r.siphonUntil,r.siphonNext,r.siphonCooldown})MP_StateField(h,(uint64_t)(int64_t)value);
            MP_StateField(h,r.shot);MP_StateField(h,r.bleedDischarge);
            MP_StateActor(h,r.dotSource);MP_StateActor(h,r.bleedSource);MP_StateActor(h,r.frenzyTarget);MP_StateActor(h,r.feedTarget);MP_StateActor(h,r.feedPrevious);MP_StateActor(h,r.eyeTarget);MP_StateActor(h,r.feedDanger);MP_StateActor(h,r.retaliationTarget);MP_StateActor(h,r.retaliationVictim);
            for(float value:r.start)MP_StateFloat(h,value);for(float value:r.end)MP_StateFloat(h,value);
        }
        auto shot=pellets.find(actor);MP_StateField(h,shot!=pellets.end());
        if(shot!=pellets.end()){MP_StateField(h,shot->second.total);MP_StateField(h,shot->second.face);MP_StateField(h,shot->second.frenzyHead);MP_StateField(h,shot->second.eye);}
    }
}
#endif

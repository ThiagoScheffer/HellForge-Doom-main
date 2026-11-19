// Rule and persistence tests against the real combat module, with the
// vanilla damage/RNG boundaries replaced by observable deterministic mocks.
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
extern "C" {
#include "p_local.h"
#include "doomstat.h"
extern int wp_test_sight;
extern mobj_t *wp_test_last_source;
extern int wp_test_rng_value,wp_test_rng_calls,wp_test_damage_calls;
}
int leveltime=10;
static unsigned checks=0;
static void check(bool ok,const char *why){if(!ok){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}++checks;}
static mobjinfo_t info{};
static player_t player{};
static mobj_t source{},target{};
static void link(mobj_t *m) {
    m->thinker.function.acp1=(actionf_p1)P_MobjThinker;
    m->thinker.prev=thinkercap.prev;m->thinker.next=&thinkercap;thinkercap.prev->next=&m->thinker;thinkercap.prev=&m->thinker;
}
static void reset(int type=MT_TROOP) {
    P_WeakpointClear();P_CombatConfigureDot(0);P_HudConfigure(0);P_SessionReset();P_CombatConfigure(0,0);P_CombatCompatible=nullptr;P_CombatStagger=nullptr;P_CombatDonor=nullptr;P_WeakpointConfigureEyes(0);P_WeakpointConfigureFeeding(0);P_WeakpointConfigureExtras(0,0);
    P_WeakpointFoodPath=nullptr;P_WeakpointFoodMove=nullptr;P_WeakpointFoodRetreat=nullptr;P_WeakpointFoodAnimate=nullptr;P_WeakpointFoodWake=nullptr;P_WeakpointFoodConsume=nullptr;P_WeakpointPainVoice=nullptr;
    for(auto &p:players)p={};wp_test_sight=1;thinkercap.next=thinkercap.prev=&thinkercap;
    source={};target={};info={};player={};source.player=&player;
    target.type=(mobjtype_t)type;target.flags=MF_SHOOTABLE;target.health=1000;info.spawnhealth=1000;target.info=&info;
    leveltime=10;wp_test_rng_calls=wp_test_damage_calls=0;wp_test_rng_value=0;
    netgame=demoplayback=demorecording=false;
    P_WeakpointTargetAllowed=nullptr;P_WeakpointLocate=nullptr;P_WeakpointConfigure(1,1,1);
    link(&source);link(&target);
}
static wp_damage_event_t event(wp_damage_kind_t kind,wp_region_t region,int damage=20,int confidence=100) {
    wp_damage_event_t e{};e.target=&target;e.source=e.inflictor=&source;e.kind=kind;e.impact.region=region;
    e.impact.confidence=confidence;e.baseDamage=damage;e.impact.distance=50;return e;
}
static void matrix() {
    struct Row{int type;wp_damage_kind_t kind;wp_region_t region;int percent;};
    const Row rows[]={
        {MT_TROOP,WP_BULLET,WP_HEAD,200},{MT_UNDEAD,WP_SHOTGUN_PELLET,WP_HEAD,200},
        {MT_VILE,WP_BULLET,WP_HEAD,200},{MT_KNIGHT,WP_BULLET,WP_HEAD,200},{MT_BRUISER,WP_BULLET,WP_HEAD,200},
        {MT_FATSO,WP_BULLET,WP_HEAD,175},{MT_CYBORG,WP_BULLET,WP_HEAD,150},
        {MT_FATSO,WP_PLASMA_DIRECT,WP_BODY,175},{MT_CYBORG,WP_PLASMA_DIRECT,WP_BODY,150},
        {MT_FATSO,WP_ROCKET_DIRECT,WP_UNKNOWN,150},{MT_CYBORG,WP_ROCKET_DIRECT,WP_UNKNOWN,150},
        {MT_SERGEANT,WP_SHOTGUN_PELLET,WP_FACE,150},{MT_SHADOWS,WP_SHOTGUN_PELLET,WP_FACE,150},
        {MT_SERGEANT,WP_BULLET,WP_FACE,100},{MT_SERGEANT,WP_SHOTGUN_PELLET,WP_BODY,100},
        {MT_HEAD,WP_BULLET,WP_EYE,150},{MT_PAIN,WP_BULLET,WP_EYE,150},
        {MT_HEAD,WP_PLASMA_DIRECT,WP_EYE,200},{MT_PAIN,WP_PLASMA_DIRECT,WP_UNKNOWN,200},
        {MT_BABY,WP_PLASMA_DIRECT,WP_BRAIN,300},{MT_SPIDER,WP_PLASMA_DIRECT,WP_UNKNOWN,300},
        {MT_BABY,WP_SHOTGUN_PELLET,WP_BRAIN,200},{MT_SPIDER,WP_SHOTGUN_PELLET,WP_BRAIN,200},
        {MT_BABY,WP_BULLET,WP_BRAIN,100},{MT_BABY,WP_ROCKET_DIRECT,WP_BRAIN,150},
        {MT_SPIDER,WP_ROCKET_DIRECT,WP_LEGS,125},{MT_SPIDER,WP_ROCKET_DIRECT,WP_CHASSIS,100},
        {MT_SPIDER,WP_ROCKET_SPLASH,WP_BRAIN,100},{MT_CYBORG,WP_ROCKET_SPLASH,WP_HEAD,100},
        {MT_HEAD,WP_BFG_DIRECT,WP_EYE,100},{MT_BABY,WP_BFG_TRACER,WP_BRAIN,100}};
    for(const auto &row:rows) {
        reset(row.type);auto e=event(row.kind,row.region);P_WeakpointResolve(&e);
        check(e.finalDamage==row.percent/5,"monster/weapon damage matrix");
        check(wp_test_damage_calls==0,"resolving damage must not call vanilla damage");
    }
    reset(MT_BABY);auto e=event(WP_PLASMA_DIRECT,WP_BRAIN);e.regionPercent=200;
    P_WeakpointApply(&e);check(target.health==940,"bonuses take the maximum instead of multiplying");
    P_WeakpointApply(&e);check(wp_test_damage_calls==1,"one damage application per event");
    check(P_WeakpointScale(1,150)==2&&P_WeakpointScale(5,175)==9,"consistent half-up fractional rounding");
    check(P_WeakpointScale(INT_MAX,800)==INT_MAX&&P_WeakpointScale(0,300)==0,"saturating arithmetic");
    reset(MT_TROOP);e=event(WP_BULLET,WP_HEAD,5,0);P_WeakpointApply(&e);
    check(target.health==995,"uncertain head regions fail safely");
    reset(MT_HEAD);e=event(WP_PLASMA_DIRECT,WP_UNKNOWN,20,0);P_WeakpointApply(&e);
    check(target.health==960,"location-independent vulnerability survives unknown anatomy");
}
static void eligibility() {
    for(int mode=0;mode<4;++mode) {
        reset(MT_BABY);if(mode==0)netgame=true;else if(mode==1)demoplayback=true;else if(mode==2)demorecording=true;
        else P_WeakpointTargetAllowed=+[](const mobj_t*)->int{return 0;};
        auto e=event(WP_PLASMA_DIRECT,WP_BRAIN);P_WeakpointApply(&e);
        check(target.health==980&&wp_test_rng_calls==0,"network/demo/voxel exclusions preserve damage and RNG");
    }
    reset(MT_BABY);source.player=nullptr;auto e=event(WP_PLASMA_DIRECT,WP_BRAIN);P_WeakpointApply(&e);
    check(target.health==980,"monster infighting gets no player vulnerabilities");
    reset(MT_TROOP);target.player=&player;e=event(WP_BULLET,WP_HEAD);P_WeakpointApply(&e);
    check(target.health==980,"players never receive monster critical rules");
    reset(MT_FATSO);P_WeakpointConfigure(0,0,0);e=event(WP_BULLET,WP_HEAD);P_WeakpointApply(&e);
    check(target.health==980&&wp_test_rng_calls==0,"all new switches off preserve damage and RNG");
    reset(MT_POSSESSED);P_WeakpointConfigure(0,0,0);e=event(WP_BULLET,WP_HEAD);e.regionPercent=200;P_WeakpointApply(&e);
    check(target.health==960,"existing human headshots remain independent");
    reset(MT_BABY);P_WeakpointConfigure(0,1,0);e=event(WP_PLASMA_DIRECT,WP_UNKNOWN);P_WeakpointResolve(&e);
    check(e.finalDamage==60&&e.weapon==wp_plasma&&e.projectileType==MT_PLASMA,"weapon-only switch retains explicit original projectile identity");
    reset(MT_TROOP);P_WeakpointBeginShot(&source,wp_chaingun);e=event(WP_BULLET,WP_HEAD);P_WeakpointResolve(&e);
    check(e.weapon==wp_chaingun&&e.eventId&&e.dischargeId,"ballistic context retains explicit weapon, event and discharge identity");P_WeakpointEndShot();
}
static void energyAndRockets() {
    for(auto kind:{WP_PLASMA_DIRECT,WP_BFG_DIRECT,WP_BFG_TRACER}) {
        reset(MT_SKULL);auto e=event(kind,WP_UNKNOWN);P_WeakpointApply(&e);
        check(target.health==0&&wp_test_damage_calls==1,"valid Lost Soul energy hits are fatal once");
    }
    reset(MT_SKULL);auto normal=event(WP_BULLET,WP_HEAD);P_WeakpointApply(&normal);check(target.health==980,"Lost Soul bullets are normal");
    int oldChance=p_wp_balance.eyeExecutionChance;
    p_wp_balance.eyeExecutionChance=0;reset(MT_HEAD);
    auto disabled=event(WP_ROCKET_DIRECT,WP_EYE);P_WeakpointApply(&disabled);
    check(target.health==980&&wp_test_rng_calls==0,"zero eye-execution chance never executes or consumes RNG");
    p_wp_balance.eyeExecutionChance=100;
    for(auto type:{MT_HEAD,MT_PAIN}) {
        reset(type);auto e=event(WP_ROCKET_DIRECT,WP_EYE);P_WeakpointApply(&e);check(target.health==0,"confident rocket eye execution");
        reset(type);e=event(WP_ROCKET_DIRECT,WP_BODY);P_WeakpointApply(&e);check(target.health==980,"body rocket cannot execute");
        reset(type);e=event(WP_ROCKET_SPLASH,WP_EYE);P_WeakpointApply(&e);check(target.health==980,"splash cannot execute");
        reset(type);e=event(WP_ROCKET_DIRECT,WP_EYE,20,0);P_WeakpointApply(&e);check(target.health==980,"uncertain eye cannot execute");
    }
    p_wp_balance.eyeExecutionChance=30;reset(MT_HEAD);
    auto e=event(WP_ROCKET_DIRECT,WP_EYE);P_WeakpointResolve(&e);int calls=wp_test_rng_calls;
    P_WeakpointResolve(&e);check(calls==1&&wp_test_rng_calls==calls,"one random eye roll per resolved event");
    p_wp_balance.eyeExecutionChance=oldChance;
    reset(MT_HEAD);mobj_t rocket{};rocket.target=&source;
    P_WeakpointProjectileSpawn(&rocket,&source,WP_PLASMA_DIRECT);player.readyweapon=wp_bfg;
    P_WeakpointProjectileHit(&rocket,&target,20);check(target.health==960,"projectile provenance survives weapon switching");
    P_WeakpointProjectileHit(&rocket,&target,20);check(target.health==960,"duplicate projectile contact cannot repeat direct damage");
}
static void discharge() {
    for(auto weapon:{wp_shotgun,wp_supershotgun}) {
        int quota=weapon==wp_shotgun?3:7,range=weapon==wp_shotgun?96:128;
        for(int scenario=0;scenario<5;++scenario) {
            reset(MT_SERGEANT);P_WeakpointBeginShot(&source,weapon);
            int count=scenario==0?1:quota;
            for(int i=0;i<count;++i){auto e=event(WP_SHOTGUN_PELLET,WP_FACE,1);e.impact.distance=scenario==2?range+0.1f:(float)range;P_WeakpointApply(&e);}
            int bodies=scenario==3?quota+1:0;
            for(int i=0;i<bodies;++i){auto e=event(WP_SHOTGUN_PELLET,WP_BODY,1);P_WeakpointApply(&e);}
            check(target.health>0,"pellets remain individual ordinary damage until discharge completion");
            P_WeakpointEndShot();bool execute=scenario==1||scenario==4;
            check((target.health<=0)==execute,"shotgun quotas, distance boundary and face majority");
            int calls=wp_test_damage_calls;P_WeakpointEndShot();check(calls==wp_test_damage_calls,"discharge cannot execute twice");
        }
    }
    reset(MT_SHADOWS);P_WeakpointBeginShot(&source,wp_shotgun);
    for(int i=0;i<3;++i){auto e=event(WP_SHOTGUN_PELLET,WP_FACE,1,0);P_WeakpointApply(&e);}
    P_WeakpointEndShot();check(target.health==997,"uncertain Spectre face never executes");
}
static void injuriesAndSaves() {
    int oldAim=p_wp_balance.aimChance,oldLeg=p_wp_balance.legChance;
    p_wp_balance.aimChance=p_wp_balance.legChance=0;
    reset(MT_FATSO);target.health=95;auto noAim=event(WP_BULLET,WP_HEAD,5);P_WeakpointApply(&noAim);P_WeakpointAttackBegin(&target);
    check(P_WeakpointAttackOffset(&target)==0&&wp_test_rng_calls==0,"zero aim chance cannot disrupt or consume RNG");
    reset(MT_BABY);auto noLeg=event(WP_ROCKET_DIRECT,WP_LEGS,4);P_WeakpointApply(&noLeg);
    check(!P_WeakpointImmobilized(&target)&&wp_test_rng_calls==0,"zero leg chance cannot immobilize or consume RNG");
    p_wp_balance.aimChance=p_wp_balance.legChance=100;
    reset(MT_FATSO);target.health=95;
    auto e=event(WP_BULLET,WP_HEAD,5);P_WeakpointApply(&e);
    check(target.health==86,"heavy head damage uses fractional balance");
    P_WeakpointAttackBegin(&target);int32_t first=(int32_t)P_WeakpointAttackOffset(&target);
    check(first<0&&std::abs((int64_t)first)<=4294967296LL*8/360,"bounded aim deviation");
    check((int32_t)P_WeakpointAttackOffset(&target)==first,"one offset is shared by a volley");
    P_WeakpointAttackEnd(&target);P_WeakpointAttackBegin(&target);check(P_WeakpointAttackOffset(&target)==0,"aim disruption is consumed once");
    e=event(WP_BULLET,WP_HEAD,5);P_WeakpointApply(&e);P_WeakpointAttackBegin(&target);check(P_WeakpointAttackOffset(&target)==0,"heavy cooldown prevents repeated disruption");
    reset(MT_CYBORG);target.health=100;e=event(WP_PLASMA_DIRECT,WP_BODY,5);P_WeakpointApply(&e);
    P_WeakpointAttackBegin(&target);check(P_WeakpointAttackOffset(&target)==0,"body vulnerabilities do not trigger head injuries");
    reset(MT_BABY);e=event(WP_ROCKET_DIRECT,WP_LEGS,4);P_WeakpointApply(&e);
    check(P_WeakpointImmobilized(&target),"mechanical leg injury activates");
    std::vector<unsigned char> saved(P_WeakpointSave(nullptr));check(P_WeakpointSave(saved.data())==saved.size(),"save size matches serialized records");
    leveltime+=1000;check(P_WeakpointLoad(saved.data(),saved.size())==saved.size(),"versioned injury trailer loads");
    check(P_WeakpointImmobilized(&target),"remaining immobilization survives save/load");
    leveltime+=35;check(!P_WeakpointImmobilized(&target),"immobilization expires at 35 tics");
    e=event(WP_ROCKET_DIRECT,WP_LEGS,4);P_WeakpointApply(&e);check(!P_WeakpointImmobilized(&target),"cooldown starts after immobilization ends");
    leveltime+=175;e=event(WP_ROCKET_DIRECT,WP_LEGS,4);P_WeakpointApply(&e);check(P_WeakpointImmobilized(&target),"leg injury can recur after cooldown");
    P_WeakpointConfigure(1,1,0);check(!P_WeakpointImmobilized(&target),"disabling injuries cancels active effects");
    reset(MT_BABY);e=event(WP_ROCKET_DIRECT,WP_LEGS,4);P_WeakpointApply(&e);P_WeakpointForget(&target);
    check(P_WeakpointRuntimeCount()==0,"removal/death/resurrection cleanup releases actor state");
    check(P_WeakpointLoad(saved.data(),saved.size()-1)==0&&P_WeakpointRuntimeCount()==0,"truncated/legacy saves initialize empty state");
    reset(MT_HEAD);mobj_t plasma{};plasma.flags=MF_MISSILE;plasma.target=&source;plasma.type=MT_PLASMA;link(&plasma);
    P_WeakpointProjectileSpawn(&plasma,&source,WP_PLASMA_DIRECT);
    P_WeakpointProjectileStep(&plasma,0,FRACUNIT,2*FRACUNIT,3*FRACUNIT,4*FRACUNIT,5*FRACUNIT);
    saved.resize(P_WeakpointSave(nullptr));P_WeakpointSave(saved.data());P_WeakpointLoad(saved.data(),saved.size());
    float start[3],end[3];check(P_WeakpointTrajectory(&plasma,start,end)&&start[1]==1&&end[2]==5,"in-flight trajectory survives save/load");
    P_WeakpointProjectileHit(&plasma,&target,20);check(target.health==960,"restored projectile retains original weapon vulnerability");
    for(int i=0;i<1000;++i){mobj_t projectile{};P_WeakpointProjectileSpawn(&projectile,&source,WP_ROCKET_DIRECT);P_WeakpointForget(&projectile);}
    P_WeakpointForget(&plasma);check(P_WeakpointRuntimeCount()==0,"repeated projectile lifecycles do not leak records");
    p_wp_balance.aimChance=oldAim;p_wp_balance.legChance=oldLeg;
}
static int notifications=0;
static void bleedingAndFrenzy() {
    const auto balance=p_wp_balance;
    for(auto type:{MT_TROOP,MT_KNIGHT,MT_FATSO,MT_SERGEANT}) {
        reset(type);P_WeakpointConfigure(0,0,0);P_WeakpointConfigureExtras(1,0);p_wp_balance.bleedChance=100;
        P_WeakpointBeginShot(&source,wp_pistol);auto e=event(WP_BULLET,WP_BODY,1);P_WeakpointApply(&e);P_WeakpointEndShot();
        check(P_WeakpointBleeding(&target),"ordinary bullet bleeding works independently of enhanced rules");
        check(P_WeakpointWalkSpeed(&target,10*FRACUNIT)==8*FRACUNIT,"bleeding slows only walking by twenty percent");
        for(int i=1;i<=6;++i){leveltime=10+i*35;P_WeakpointTick(&target);check(target.health==999-i*2,"six exact bleeding damage ticks");}
        ++leveltime;P_WeakpointTick(&target);check(!P_WeakpointBleeding(&target),"bleeding expires after six seconds");
    }
    reset();P_WeakpointConfigureExtras(1,0);p_wp_balance.bleedChance=0;
    P_WeakpointBeginShot(&source,wp_shotgun);auto e=event(WP_SHOTGUN_PELLET,WP_HEAD,1);P_WeakpointApply(&e);P_WeakpointEndShot();
    check(P_WeakpointBleeding(&target),"confirmed critical guarantees bleed even with zero ordinary chance");
    check(wp_test_rng_calls==0,"guaranteed bleeding avoids probability draws");
    leveltime+=20;P_WeakpointBeginShot(&source,wp_pistol);e=event(WP_BULLET,WP_HEAD,1);P_WeakpointApply(&e);P_WeakpointEndShot();
    leveltime=45;int health=target.health;P_WeakpointTick(&target);check(target.health==health-3,"refresh preserves tick cadence while raising severity");
    P_WeakpointConfigureExtras(0,0);check(!P_WeakpointBleeding(&target),"bleeding disabled cancels immediately");
    for(auto kind:{WP_PLASMA_DIRECT,WP_ROCKET_DIRECT,WP_ROCKET_SPLASH,WP_BFG_DIRECT,WP_BFG_TRACER,WP_OTHER}) {
        reset();P_WeakpointConfigureExtras(1,0);p_wp_balance.bleedChance=100;e=event(kind,WP_HEAD,1);P_WeakpointApply(&e);
        check(!P_WeakpointBleeding(&target),"non-ballistic damage never causes bleeding");
    }
    reset();P_WeakpointConfigureExtras(1,0);p_wp_balance.bleedChance=100;e=event(WP_BULLET,WP_BODY,1);P_WeakpointApply(&e);
    check(!P_WeakpointBleeding(&target),"unmarked shared line attacks such as melee cannot bleed");
    for(int value:{76,77}) {
        reset();P_WeakpointConfigure(0,0,0);P_WeakpointConfigureExtras(1,0);p_wp_balance.bleedChance=30;wp_test_rng_value=value;
        P_WeakpointBeginShot(&source,wp_pistol);e=event(WP_BULLET,WP_BODY,1);P_WeakpointApply(&e);P_WeakpointEndShot();
        check((bool)P_WeakpointBleeding(&target)==(value==76),"thirty-percent bleed RNG boundary");
    }
    for(auto type:{MT_FATSO,MT_KNIGHT,MT_BRUISER})for(int scenario=0;scenario<7;++scenario) {
        reset(type);P_WeakpointConfigureExtras(0,1);p_wp_balance.frenzyChance=40;wp_test_rng_value=scenario==1?103:102;
        mobj_t other{};other.type=target.type;other.flags=MF_SHOOTABLE|MF_COUNTKILL;other.health=100;other.x=100*FRACUNIT;link(&other);
        if(scenario==4)wp_test_sight=0;if(scenario==5)other.x=513*FRACUNIT;
        auto weapon=scenario==2?wp_shotgun:wp_supershotgun;P_WeakpointBeginShot(&source,weapon);
        for(int i=0;i<4;++i){e=event(WP_SHOTGUN_PELLET,WP_HEAD,1);e.impact.distance=scenario==3?128.1f:128;
            if(scenario==6)e.impact.confidence=0;P_WeakpointApply(&e);}
        int calls=wp_test_rng_calls;P_WeakpointEndShot();
        check((bool)P_WeakpointFrenzied(&target)==(scenario==0),"frenzy type, chance, weapon, range, visibility and confidence gates");
        check(wp_test_rng_calls-calls==(scenario==0||scenario==1||scenario==4||scenario==5?1:0),"one forty-percent roll per qualifying discharge");
        if(scenario==0) {
            check(target.target==&other,"same-species victim selected");
            mobj_t missile{};link(&missile);P_WeakpointMonsterMissile(&missile,&target);
            check(P_WeakpointFriendlyMissile(&missile),"frenzy marks its monster projectile");
            std::vector<unsigned char> save(P_WeakpointSave(nullptr));P_WeakpointSave(save.data());P_WeakpointClear();P_WeakpointLoad(save.data(),save.size());
            check(P_WeakpointFrenzied(&target)&&P_WeakpointFriendlyMissile(&missile),"frenzy and marked projectiles survive save/load");
            target.target=&source;P_WeakpointFrenzyTarget(&target);check(target.target==&other,"frenzy overrides retaliation target");
            leveltime+=350;P_WeakpointFrenzyTarget(&target);
            check(!P_WeakpointFrenzied(&target)&&target.target==nullptr,"ten-second frenzy expiry resumes normal acquisition");
            check(P_WeakpointFriendlyMissile(&missile),"launched projectile remains friendly-fire capable after expiry");
            P_WeakpointConfigureExtras(0,0);check(!P_WeakpointFriendlyMissile(&missile),"disabling feature cancels projectile override");
        }
    }
    reset(MT_KNIGHT);P_WeakpointConfigureExtras(1,1);p_wp_balance.bleedChance=p_wp_balance.frenzyChance=100;
    mobj_t other{};other.type=MT_TROOP;other.flags=MF_SHOOTABLE|MF_COUNTKILL;other.health=100;link(&other);
    P_WeakpointBeginShot(&source,wp_supershotgun);e=event(WP_SHOTGUN_PELLET,WP_HEAD,1);P_WeakpointApply(&e);P_WeakpointEndShot();
    check(P_WeakpointBleeding(&target)&&P_WeakpointFrenzied(&target),"bleeding and frenzy coexist");
    std::vector<unsigned char> save(P_WeakpointSave(nullptr));P_WeakpointSave(save.data());leveltime+=1000;P_WeakpointLoad(save.data(),save.size());
    check(P_WeakpointBleeding(&target)&&P_WeakpointFrenzied(&target),"both statuses restore remaining time instead of absolute time");
    P_WeakpointForget(&other);other.health=0;P_WeakpointFrenzyTarget(&target);check(!P_WeakpointFrenzied(&target),"no surviving victims ends frenzy early");
    target.health=1;leveltime+=35;P_WeakpointTick(&target);check(target.health<=0&&P_WeakpointRuntimeCount()==0,"lethal bleeding releases status records");
    reset(MT_TROOP);P_WeakpointConfigureExtras(1,0);p_wp_balance.bleedChance=100;
    P_WeakpointBeginShot(&source,wp_pistol);e=event(WP_BULLET,WP_BODY,1);P_WeakpointApply(&e);P_WeakpointEndShot();
    P_WeakpointConfigure(0,0,0);check(P_WeakpointBleeding(&target),"unrelated injury/weak-point toggles preserve independent bleeding");
    notifications=0;P_WeakpointImpact=+[](const wp_damage_event_t*){++notifications;};
    target.health=1;e=event(WP_BULLET,WP_BODY,1);P_WeakpointApply(&e);P_WeakpointNotify(&e);P_WeakpointApply(&e);
    check(notifications==1,"one confirmed notification including lethal damage and duplicate callbacks");P_WeakpointImpact=nullptr;
    for(int mode=0;mode<4;++mode) {
        reset(MT_KNIGHT);P_WeakpointConfigureExtras(1,1);p_wp_balance.bleedChance=p_wp_balance.frenzyChance=100;
        if(mode==0)netgame=true;else if(mode==1)demoplayback=true;else if(mode==2)demorecording=true;
        else P_WeakpointTargetAllowed=+[](const mobj_t*)->int{return 0;};
        P_WeakpointBeginShot(&source,wp_supershotgun);e=event(WP_SHOTGUN_PELLET,WP_HEAD,1);P_WeakpointApply(&e);P_WeakpointEndShot();
        check(!P_WeakpointBleeding(&target)&&!P_WeakpointFrenzied(&target)&&wp_test_rng_calls==0,"new statuses excluded from demo/network/voxel paths without RNG");
    }
    reset(MT_KNIGHT);P_WeakpointConfigureExtras(0,1);p_wp_balance.frenzyChance=100;target.health=1;
    P_WeakpointBeginShot(&source,wp_supershotgun);e=event(WP_SHOTGUN_PELLET,WP_HEAD,1);P_WeakpointApply(&e);P_WeakpointEndShot();
    check(!P_WeakpointFrenzied(&target)&&wp_test_rng_calls==0,"lethal discharge never rolls frenzy");
    // Previous 60-byte v1 records must retain their injury/provenance prefix.
    reset(MT_BABY);p_wp_balance.legChance=100;e=event(WP_ROCKET_DIRECT,WP_LEGS,1);P_WeakpointApply(&e);
    save.resize(P_WeakpointSave(nullptr));P_WeakpointSave(save.data());
    std::vector<unsigned char> legacy(save.begin(),save.begin()+68);legacy[0]=1;
    uint32_t bytes=76;for(int i=0;i<4;++i)legacy.push_back((unsigned char)(bytes>>(8*i)));
    for(char c:{'W','P','0','1'})legacy.push_back(c);
    check(P_WeakpointLoad(legacy.data(),legacy.size())==76&&P_WeakpointImmobilized(&target),"v1 save records remain compatible after v2 status extension");
    p_wp_balance=balance;P_WeakpointConfigureExtras(0,0);
}
#include "survival_rules.inc"
#include "eye_injury_rules.inc"
#include "elemental_rules.inc"
#include "elemental2_rules.inc"
static void severeCriticalRules() {
    static int bursts;
    static wp_severe_burst_t last;
    auto observe=[&](int type) {
        reset(type);
        totalkills=199;
        bursts=0;
        last={};
        P_SevereBloodTint=+[](const mobj_t *m){return m->type==MT_HEAD?1:(m->type==MT_KNIGHT||m->type==MT_BRUISER)?2:0;};
        P_SevereBloodBurst=+[](const wp_severe_burst_t *burst){last=*burst;++bursts;};
        P_HudConfigure(1);
    };
    combat_notice_t notices[3]{};
    for(int type:{MT_TROOP,MT_SERGEANT,MT_VILE,MT_FATSO,MT_BRUISER,MT_KNIGHT,MT_HEAD,MT_SHADOWS}) {
        observe(type);
        target.health=251;
        auto hit=event(WP_BULLET,WP_BODY,1);
        P_WeakpointBeginShot(&source,wp_pistol);
        P_WeakpointApply(&hit);
        check(target.health==250&&bursts==0,"ordinary damage queues feedback until the shot ends");
        P_WeakpointEndShot();
        const bool supported=type!=MT_SHADOWS;
        check(bursts==int(supported),"each supported type qualifies on crossing 25 percent by 1 HP");
        if(supported) {
            check(P_HudNotices(notices)==1&&notices[0].kind==HUD_SEVERE_CRITICAL&&
                  notices[0].color==0xFFFF4444u,"low health triggers the red notice");
            check(last.tint==(type==MT_HEAD?1:(type==MT_BRUISER||type==MT_KNIGHT)?2:0),
                  "low-health event retains monster blood tint");
        }
        check(wp_test_rng_calls==0,"low-health feedback consumes no gameplay RNG");
    }
    observe(MT_TROOP);
    target.health=252;
    P_HudConfigure(0);
    P_DamageMobj(&target,&source,&source,1);
    check(bursts==0,"health above 25 percent gives no warning");
    P_DamageMobj(&target,&source,&source,1);
    check(bursts==1&&P_HudNotices(notices)==0,"ordinary damage outside a hitscan discharge triggers the effect independently");
    P_DamageMobj(&target,&source,&source,1);
    check(bursts==1,"further hits below the threshold do not repeat feedback");
    target.health=251;
    P_DamageMobj(&target,&source,&source,1);
    check(bursts==2,"crossing again after recovery can trigger feedback");
    observe(MT_TROOP);
    target.health=251;totalkills=200;
    P_DamageMobj(&target,&source,&source,1);
    check(bursts==0&&P_HudNotices(notices)==0,"map cutoff still applies");
    observe(MT_TROOP);
    target.health=251;
    P_DamageMobj(&target,&source,&source,251);
    check(bursts==0&&P_HudNotices(notices)==0,"a dead monster does not give a low-health warning");
    for(bool lethal:{false,true}) {
        observe(MT_TROOP);
        target.health=255;
        P_WeakpointBeginShot(&source,wp_shotgun);
        auto first=event(WP_SHOTGUN_PELLET,WP_BODY,5);
        auto next=event(WP_SHOTGUN_PELLET,WP_BODY,lethal?250:5);
        P_WeakpointApply(&first);P_WeakpointApply(&next);P_WeakpointEndShot();
        check(bursts==int(!lethal),"shotgun warns once only if target survives the whole discharge");
    }
    observe(MT_TROOP);
    target.health=251;
    P_SevereBloodBurst=nullptr;
    P_DamageMobj(&target,&source,&source,1);
    check(P_HudNotices(notices)==1,"HUD warning works with the effect disabled");
    P_SevereBloodTint=nullptr;totalkills=0;
}
int main() {
    matrix();eligibility();energyAndRockets();discharge();injuriesAndSaves();bleedingAndFrenzy();survivalRules();eyeInjuryRules();elementalRules();elemental2Rules();
    severeCriticalRules();
    int original=p_wp_balance.heavyHead;
    check(P_WeakpointReadBalance("wp_heavy_head",190)&&p_wp_balance.heavyHead==190,"documented balance key loads");
    P_WeakpointReadBalance("wp_heavy_head",900);check(p_wp_balance.heavyHead==190,"out-of-range tuning is ignored");
    P_WeakpointReadBalance("wp_heavy_head",NAN);check(p_wp_balance.heavyHead==190,"nonfinite tuning is ignored");p_wp_balance.heavyHead=original;
    std::printf("PASS %u weak-point rule, discharge, injury and persistence checks\n",checks);
}

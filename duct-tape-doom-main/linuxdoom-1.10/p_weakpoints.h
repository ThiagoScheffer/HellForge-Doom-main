/* Optional single-player combat rules. No mobj/save-record layout changes. */
#ifndef P_WEAKPOINTS_H
#define P_WEAKPOINTS_H
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "doomdef.h"
#include "p_mobj.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum { WP_OTHER,WP_BULLET,WP_SHOTGUN_PELLET,WP_ROCKET_DIRECT,WP_ROCKET_SPLASH,
    WP_PLASMA_DIRECT,WP_BFG_DIRECT,WP_BFG_TRACER,WP_CHAINSAW } wp_damage_kind_t;
typedef enum { WP_UNKNOWN,WP_BODY,WP_HEAD,WP_FACE,WP_EYE,WP_BRAIN,WP_LEGS,WP_CHASSIS } wp_region_t;
typedef struct {
    wp_region_t region;
    int confidence; /* 0 unknown, 100 verified pixel region. */
    float x,y,z,u,v,distance;
    int sprite,frame,rotation,flipped;
    int eyeContact; /* Verified visible eye/socket overlay; primary region is unchanged. */
} wp_impact_t;
typedef struct {
    mobj_t *target,*source,*inflictor;
    wp_damage_kind_t kind;
    weapontype_t weapon;
    int projectileType;
    int notified;
    wp_impact_t impact;
    int baseDamage,finalDamage,regionPercent,resolved,eligible;
    uint64_t eventId,dischargeId;
} wp_damage_event_t;
typedef struct {
    wp_impact_t impact;
    float directionX,directionY;
    int tint;
    int targetType;
    uint64_t seed;
} wp_severe_burst_t;
extern int (*P_SevereBloodTint)(const mobj_t*);
extern void (*P_SevereBloodBurst)(const wp_severe_burst_t*);
void P_WeakpointRecordHealthLoss(const mobj_t *target,const mobj_t *source,int before,int after);
typedef enum { GORE_BLAST_BEGIN,GORE_BLAST_DIRECT,GORE_BLAST_SPLASH,GORE_BLAST_END,
    GORE_BLAST_COMPLETE,GORE_BLAST_FORGET } gore_blast_op_t;
extern void (*P_GoreBlast)(gore_blast_op_t,const mobj_t *spot,const mobj_t *victim,int before);
typedef struct {
    int humanoidHead,heavyHead,cyberHead,facePellet,eyeBullet,brainPellet;
    int heavyPlasma,cyberPlasma,floatingPlasma,mechanicalPlasma;
    int heavyRocket,brainRocket,legRocket;
    int eyeExecutionChance,aimChance,aimDegrees,aimDuration,aimCooldown;
    int legChance,legDuration,legCooldown,shotgunRange,ssgRange,shotgunQuota,ssgQuota;
    int lowHealthPercent,faceMajorityPercent;
    int bleedChance,bleedDuration,bleedDamage,bleedSlow;
    int frenzyChance,frenzyDuration,frenzyRange,frenzySearch;
    int bleedDamage2,bleedDamage3,bleedDuration2,bleedDuration3,bleedAttack2,bleedAttack3,bleedVoice2,bleedVoice3;
    int feedStart,feedStop,feedRadius,feedBite,feedLiveHeal,feedCorpseHeal,feedSearch,feedPursuit,feedCooldown,feedThreat;
    int sparkBullet,sparkPellet,sparkCap,sparkScale,headBloodScale,headWallReach,headWallMarks;
    int eyeChance,eyeRageDuration,blindFireInterval,blindPitch,eyeIncoming,eyePlayerDamage,eyeSearchRange;
    int feedWindow,feedSpeed,feedFailureCooldown,feedVileStart,feedVileDamage,feedVileReach;
} wp_balance_t;
extern wp_balance_t p_wp_balance;
extern int p_wp_enhanced,p_wp_vulnerabilities,p_wp_injuries;
extern int p_wp_elemental,p_wp_reactions,p_wp_dot,p_wp_dot_damage;
void P_CombatConfigureDot(int enabled);
int P_CombatDotKind(mobj_t*);
typedef enum { HUD_CRITICAL_BLEED,HUD_FRENZY,HUD_EYE,HUD_EYE_RAGE,HUD_CUSTOM,HUD_SEVERE_CRITICAL } combat_notice_kind_t;
typedef struct {int kind,priority;unsigned color;uint64_t key,expires;char text[96];} combat_notice_t;
typedef struct {uint64_t attempts,deaths,streak,attemptTics,runtimeTics,dealt,taken;int episode,map;} combat_stats_t;
void P_HudConfigure(int messages);
void P_HudNotify(combat_notice_kind_t,const char*,unsigned,int,int,uint64_t);
int P_HudNotices(combat_notice_t out[3]);
void P_CombatMessage(combat_notice_kind_t,mobj_t*,uint64_t);
void P_SessionMap(int episode,int map);
void P_SessionNewVisit(void);
void P_SessionTick(int alive);
void P_SessionDamage(mobj_t*,mobj_t*,int before,int damage);
const combat_stats_t *P_SessionStats(void);
void P_SessionReset(void);
extern int p_wp_elemental,p_wp_reactions;
void P_CombatConfigure(int elemental,int reactions);
int P_CombatDamageKind(mobj_t *inflictor,mobj_t *source);
void P_CombatConfirmed(mobj_t*,mobj_t*,int kind,int actual,int previousBurn);
void P_CombatFireBegin(mobj_t*,mobj_t*);
void P_CombatFireEnd(void);
int P_CombatVisualKind(mobj_t*);
int P_CombatCharred(mobj_t*);
int P_CombatRage(mobj_t*);
extern void (*P_CombatVisual)(mobj_t*,int);
extern int (*P_CombatCompatible)(mobj_t*);
extern void (*P_CombatStagger)(mobj_t*);
extern mobj_t *(*P_CombatDonor)(mobj_t*,int corpse);
extern int p_wp_eyes;
void P_WeakpointConfigureEyes(int enabled);
int P_WeakpointBlind(mobj_t*);
int P_WeakpointEyeRage(mobj_t*);
int P_WeakpointBlindFireDue(mobj_t*);
void P_WeakpointBlindAim(mobj_t*,angle_t*,fixed_t*);
int P_WeakpointAdjustDamage(mobj_t*,mobj_t*,mobj_t*,int);
void P_WeakpointDealDamage(wp_damage_event_t*);
extern int (*P_WeakpointTargetAllowed)(const mobj_t*);
extern int (*P_WeakpointLocate)(mobj_t*,mobj_t*,wp_impact_t*);
extern int p_wp_bleeding,p_wp_frenzy,p_wp_bleed_damage;
extern void (*P_WeakpointImpact)(const wp_damage_event_t*);
extern void (*P_WeakpointBleedVisual)(mobj_t*,int justApplied);
uint64_t P_WeakpointDischarge(const mobj_t* source);
extern int (*P_WeakpointBulletLocate)(mobj_t*,mobj_t*,fixed_t,fixed_t,fixed_t,fixed_t,fixed_t,fixed_t,wp_impact_t*);
void P_WeakpointConfigureExtras(int bleeding,int frenzy);
void P_WeakpointNotify(wp_damage_event_t*);
void P_WeakpointTick(mobj_t*);
int P_WeakpointBleedTier(mobj_t*);
int P_WeakpointBleedDamage(mobj_t*);
int P_WeakpointBleedOpportunity(mobj_t*);
extern int p_wp_feeding;
extern int (*P_WeakpointFoodPath)(mobj_t*,mobj_t*);
extern int (*P_WeakpointFoodMove)(mobj_t*);
extern int (*P_WeakpointFoodRetreat)(mobj_t*,mobj_t*);
extern void (*P_WeakpointFoodAnimate)(mobj_t*);
extern void (*P_WeakpointFoodWake)(mobj_t*);
extern void (*P_WeakpointFoodConsume)(mobj_t*);
extern void (*P_WeakpointPainVoice)(mobj_t*);
void P_WeakpointInitFeedingHooks(void);
void P_WeakpointFeedingReset(void);
void P_WeakpointConfigureFeeding(int);
int P_WeakpointFeeding(mobj_t*);
int P_WeakpointFoodEligible(mobj_t*,int corpse);
void P_WeakpointFeedingTick(mobj_t*);
int P_WeakpointFeedingChase(mobj_t*);
int P_WeakpointFeedingAttack(mobj_t*);
void P_WeakpointFeedingHurtFrom(mobj_t*,mobj_t*);
void P_WeakpointFeedingHurt(mobj_t*);
void P_WeakpointStopFeeding(mobj_t*);
int P_WeakpointBleeding(mobj_t*);
fixed_t P_WeakpointWalkSpeed(mobj_t*,fixed_t);
int P_WeakpointFrenzied(mobj_t*);
void P_WeakpointFrenzyTarget(mobj_t*);
void P_WeakpointMonsterMissile(mobj_t*,mobj_t*);
int P_WeakpointFriendlyMissile(mobj_t*);
int P_WeakpointCanHitSpecies(mobj_t*,mobj_t*);
int P_WeakpointImpSpeciesRisk(mobj_t*,mobj_t*);
void P_WeakpointRetaliationDamage(mobj_t*,mobj_t*,mobj_t*);
int P_WeakpointActive(void);
int P_WeakpointHuman(int type);
int P_WeakpointProfile(int type);
int P_WeakpointRegionPercent(int type,wp_damage_kind_t kind,wp_region_t region,int confidence);
int P_WeakpointScale(int damage,int percent);
void P_WeakpointConfigure(int enhanced,int vulnerabilities,int injuries);
void P_WeakpointResolve(wp_damage_event_t*);
void P_WeakpointAfterDamage(const wp_damage_event_t*);
void P_WeakpointApply(wp_damage_event_t*);
void P_WeakpointBeginShot(mobj_t*,weapontype_t);
void P_WeakpointEndShot(void);
wp_damage_kind_t P_WeakpointBulletKind(void);
void P_WeakpointProjectileSpawn(mobj_t*,mobj_t*,wp_damage_kind_t);
void P_WeakpointProjectileStep(mobj_t*,fixed_t,fixed_t,fixed_t,fixed_t,fixed_t,fixed_t);
int P_WeakpointTrajectory(mobj_t*,float start[3],float end[3]);
void P_WeakpointProjectileHit(mobj_t*,mobj_t*,int);
void P_WeakpointBFGTracer(mobj_t*,mobj_t*,int);
int P_WeakpointImmobilized(mobj_t*);
void P_WeakpointAttackBegin(mobj_t*);
angle_t P_WeakpointAttackOffset(mobj_t*);
void P_WeakpointAttackEnd(mobj_t*);
void P_WeakpointForget(mobj_t*);
void P_WeakpointDied(mobj_t*);
void P_WeakpointClear(void);
size_t P_WeakpointSave(unsigned char*);
size_t P_WeakpointLoad(const unsigned char*,size_t);
int P_WeakpointReadBalance(const char*,double);
void P_WeakpointWriteBalance(FILE*);
size_t P_WeakpointRuntimeCount(void);
#ifdef __cplusplus
}
#endif
#endif

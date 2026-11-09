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
    WP_PLASMA_DIRECT,WP_BFG_DIRECT,WP_BFG_TRACER } wp_damage_kind_t;
typedef enum { WP_UNKNOWN,WP_BODY,WP_HEAD,WP_FACE,WP_EYE,WP_BRAIN,WP_LEGS,WP_CHASSIS } wp_region_t;
typedef struct {
    wp_region_t region;
    int confidence; /* 0 unknown, 100 verified pixel region. */
    float x,y,z,u,v,distance;
    int sprite,frame,rotation,flipped;
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
    int humanoidHead,heavyHead,cyberHead,facePellet,eyeBullet,brainPellet;
    int heavyPlasma,cyberPlasma,floatingPlasma,mechanicalPlasma;
    int heavyRocket,brainRocket,legRocket;
    int eyeExecutionChance,aimChance,aimDegrees,aimDuration,aimCooldown;
    int legChance,legDuration,legCooldown,shotgunRange,ssgRange,shotgunQuota,ssgQuota;
    int lowHealthPercent,faceMajorityPercent;
    int bleedChance,bleedDuration,bleedDamage,bleedSlow;
    int frenzyChance,frenzyDuration,frenzyRange,frenzySearch;
} wp_balance_t;
extern wp_balance_t p_wp_balance;
extern int p_wp_enhanced,p_wp_vulnerabilities,p_wp_injuries;
extern int (*P_WeakpointTargetAllowed)(const mobj_t*);
extern int (*P_WeakpointLocate)(mobj_t*,mobj_t*,wp_impact_t*);
extern int p_wp_bleeding,p_wp_frenzy,p_wp_bleed_damage;
extern void (*P_WeakpointImpact)(const wp_damage_event_t*);
extern void (*P_WeakpointBleedVisual)(mobj_t*);
extern int (*P_WeakpointBulletLocate)(mobj_t*,mobj_t*,fixed_t,fixed_t,fixed_t,fixed_t,fixed_t,fixed_t,wp_impact_t*);
void P_WeakpointConfigureExtras(int bleeding,int frenzy);
void P_WeakpointNotify(wp_damage_event_t*);
void P_WeakpointTick(mobj_t*);
int P_WeakpointBleeding(mobj_t*);
fixed_t P_WeakpointWalkSpeed(mobj_t*,fixed_t);
int P_WeakpointFrenzied(mobj_t*);
void P_WeakpointFrenzyTarget(mobj_t*);
void P_WeakpointMonsterMissile(mobj_t*,mobj_t*);
int P_WeakpointFriendlyMissile(mobj_t*);
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

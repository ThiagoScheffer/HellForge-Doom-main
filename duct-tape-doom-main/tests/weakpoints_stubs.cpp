// Simulation boundary for GPU-free sprite tests. Damage-rule integration
// is exercised separately against the actual engine by critical_smoke.
extern "C" {
#include "doomdef.h"
#include "doomstat.h"
#include "p_local.h"
boolean netgame=false,demoplayback=false,demorecording=false;
thinker_t thinkercap={};
int totalkills=0;
player_t players[MAXPLAYERS]={};
void P_MobjThinker(mobj_t*){}
int wp_test_sight=1;
boolean P_CheckSight(mobj_t*,mobj_t*){return wp_test_sight?true:false;}
mobj_t *wp_test_last_source=nullptr;
int wp_test_rng_value=127,wp_test_rng_calls=0,wp_test_damage_calls=0;
int P_Random(void){++wp_test_rng_calls;return wp_test_rng_value;}
void P_DamageMobj(mobj_t *target,mobj_t* inflictor,mobj_t *source,int damage){
    wp_test_last_source=source;++wp_test_damage_calls;int before=target->health;
    int kind=P_CombatDamageKind(inflictor,source),burn=P_CombatVisualKind(target);
    damage=P_WeakpointAdjustDamage(target,inflictor,source,damage);target->health-=damage;
    P_WeakpointRecordHealthLoss(target,source,before,target->health);
    P_SessionDamage(target,source,before,damage);
    if(target->health<=0)P_WeakpointDied(target);
    P_CombatConfirmed(target,source,kind,damage,target->health<=0?burn:-1);
}
}

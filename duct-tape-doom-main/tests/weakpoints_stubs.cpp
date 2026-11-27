// Simulation boundary for GPU-free sprite tests. Damage-rule integration
// is exercised separately against the actual engine by critical_smoke.
#include <cstdlib>
extern "C" {
#include "doomdef.h"
#include "doomstat.h"
#include "p_local.h"
boolean netgame=false,demoplayback=false,demorecording=false;
thinker_t thinkercap={};
int bmapwidth=0,bmapheight=0;
fixed_t bmaporgx=0,bmaporgy=0;
mobj_t **blocklinks=nullptr;
int wp_test_block_visits=0,wp_test_block_cells=0;
boolean P_BlockThingsIterator(int x,int y,boolean (*visit)(mobj_t*)) {
    if(!blocklinks||x<0||x>=bmapwidth||y<0||y>=bmapheight)std::abort();
    ++wp_test_block_cells;
    for(auto *m=blocklinks[y*bmapwidth+x];m;m=m->bnext) {
        ++wp_test_block_visits;
        if(!visit(m))return false;
    }
    return true;
}
int totalkills=0;
player_t players[MAXPLAYERS]={};
void P_MobjThinker(mobj_t*){}
int wp_test_sight=1;
mobj_t *wp_test_hidden=nullptr;
boolean P_CheckSight(mobj_t*,mobj_t *other){return wp_test_sight&&other!=wp_test_hidden?true:false;}
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

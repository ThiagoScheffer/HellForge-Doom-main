/* Conservative idle-search hibernation. Never skip a thinker or state action.
 * Only reuse a completed negative A_Look search with unchanged inputs. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "doomstat.h"
#include "p_local.h"
#include "p_swarm.h"
#include "m_argv.h"
#include "m_arcade.h"
void A_Look(mobj_t *actor);
#define SWARM_SLOTS 8192
typedef struct {
    mobj_t *actor,*player;
    uint64_t epoch;
    uint64_t key[16];
} idle_entry_t;
static idle_entry_t entries[SWARM_SLOTS];
static uint64_t epoch=1;
static int enabled,debugCounters,blockedMode,patchedDefinitions;
static swarm_stats_t stats;
static unsigned slot(mobj_t *actor){
    uint64_t hash=((uint64_t)(uintptr_t)actor>>4)*UINT64_C(11400714819323198485);
    return (unsigned)(hash>>32)&(SWARM_SLOTS-1);
}
void P_SwarmConfigure(int hibernation,int debug){
    hibernation=hibernation!=0;debug=debug!=0;
    if(enabled!=hibernation||debugCounters!=debug)++epoch;
    enabled=hibernation;debugCounters=debug;
}
void P_SwarmResetStats(void){memset(&stats,0,sizeof(stats));}
swarm_stats_t P_SwarmStats(void){return stats;}
void P_SwarmBeginLevel(void){
    ++epoch;P_SwarmResetStats();
    /* Mode flags are immutable after startup. Runtime destruction settings
     * cannot make a process without these modes use destructible geometry. */
    blockedMode=M_CheckParm("-digglobal")||M_CheckParm("-digging")||M_CheckParm("-solidmaps")||M_CheckParm("-solidlab");
}
void P_SwarmWorldChanged(void){if(enabled)++epoch;}
void P_SwarmPatchedDefinitions(void){patchedDefinitions=1;++epoch;}
void P_SwarmForget(mobj_t *actor){
    if(enabled&&entries[slot(actor)].actor==actor)entries[slot(actor)].actor=NULL;
}
static int eligible(mobj_t *actor){
    if(!enabled||blockedMode||patchedDefinitions||netgame||demoplayback||demorecording||arcade_active||respawnmonsters)return 0;
    if(!actor||actor->player||actor->health<=0||actor->target||!(actor->flags&MF_COUNTKILL)||!(actor->flags&MF_SHOOTABLE))return 0;
    if(actor->flags&(MF_MISSILE|MF_SKULLFLY|MF_CORPSE))return 0;
    if(!actor->subsector||!actor->subsector->sector||actor->subsector->sector->soundtarget)return 0;
    if(actor->momx||actor->momy||actor->momz||actor->z!=actor->floorz)return 0;
    if(!actor->state||actor->state->action.acp1!=(actionf_p1)A_Look||actor->lastlook!=0)return 0;
    if(!playeringame[0]||!players[0].mo||!players[0].mo->subsector||players[0].health<=0||players[0].playerstate!=PST_LIVE||P_Unseen(players[0].mo))return 0;
    for(int i=1;i<MAXPLAYERS;++i)if(playeringame[i])return 0;
    return 1;
}
static void key(mobj_t *a,uint64_t result[16]){
    mobj_t *p=players[0].mo;
    const uint64_t values[]={a->x,a->y,a->z,a->height,a->angle,(unsigned)a->flags,a->health,(uintptr_t)a->subsector->sector,
        p->x,p->y,p->z,p->height,(unsigned)p->flags,players[0].health,(uintptr_t)p->subsector->sector,a->lastlook};
    memcpy(result,values,sizeof(values));
}
int P_SwarmIdleSkip(mobj_t *actor){
    if(enabled||debugCounters)++stats.idle_checks;
    if(!eligible(actor))return 0;
    idle_entry_t *entry=&entries[slot(actor)];uint64_t current[16];
    if(entry->actor!=actor||entry->player!=players[0].mo||entry->epoch!=epoch)return 0;
    key(actor,current);
    if(memcmp(entry->key,current,sizeof(current)))return 0;
    ++stats.reused_searches;return 1;
}
void P_SwarmIdleNegative(mobj_t *actor,int previousLook){
    if(enabled||debugCounters)++stats.negative_searches;
    /* An initial nonzero lastlook may return before examining player zero.
     * Never memoize that cursor-only failure as a visibility result. */
    if(previousLook!=actor->lastlook||!eligible(actor))return;
    idle_entry_t *entry=&entries[slot(actor)];entry->actor=actor;entry->player=players[0].mo;entry->epoch=epoch;key(actor,entry->key);
}
void P_SwarmSoundWake(void){if(enabled||debugCounters)++stats.sound_wakes;}
void P_SwarmSpawned(mobj_t *actor){
    P_SwarmForget(actor);
    if((enabled||debugCounters)&&((actor->flags&MF_COUNTKILL)||actor->type==MT_SKULL))++stats.spawned;
}
void P_SwarmEndTic(void){
    if(!debugCounters||leveltime%TICRATE)return;
    stats.active=stats.dormant=stats.awake=0;
    for(thinker_t *t=thinkercap.next;t!=&thinkercap;t=t->next){
        if(t->function.acp1!=(actionf_p1)P_MobjThinker)continue;
        mobj_t *m=(mobj_t*)t;++stats.active;
        if(m->player||m->health<=0||(!(m->flags&MF_COUNTKILL)&&m->type!=MT_SKULL))continue;
        if(!m->target&&m->state&&m->state->action.acp1==(actionf_p1)A_Look)++stats.dormant;else ++stats.awake;
    }
    printf("SWARM tic %d: mobjs %u dormant %u awake %u pending 0 idle %llu reused %llu sound-wakes %llu spawned %llu\n",
        leveltime,stats.active,stats.dormant,stats.awake,(unsigned long long)stats.idle_checks,(unsigned long long)stats.reused_searches,
        (unsigned long long)stats.sound_wakes,(unsigned long long)stats.spawned);
}

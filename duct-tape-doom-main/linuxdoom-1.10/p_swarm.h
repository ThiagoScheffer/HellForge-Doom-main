#ifndef P_SWARM_H
#define P_SWARM_H
#include <stdint.h>
#include "p_mobj.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    uint64_t idle_checks, reused_searches, negative_searches, sound_wakes, spawned;
    unsigned active, dormant, awake;
} swarm_stats_t;
void P_SwarmConfigure(int hibernation,int debug);
void P_SwarmBeginLevel(void);
void P_SwarmWorldChanged(void);
void P_SwarmPatchedDefinitions(void);
int P_SwarmDefinitionsPatched(void);
void P_SwarmForget(mobj_t *actor);
int P_SwarmIdleSkip(mobj_t *actor);
void P_SwarmIdleNegative(mobj_t *actor,int previousLook);
void P_SwarmSoundWake(void);
void P_SwarmSpawned(mobj_t *actor);
void P_SwarmEndTic(void);
void P_SwarmResetStats(void);
swarm_stats_t P_SwarmStats(void);
#ifdef __cplusplus
}
#endif
#endif

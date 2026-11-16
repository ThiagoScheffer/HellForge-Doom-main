#ifndef HELLFORGE_P_ENCOUNTER_H
#define HELLFORGE_P_ENCOUNTER_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
struct mobj_s;
typedef struct {
 int enabled,monsters,reinforcements,items,bossProtection,weighted;
 int noBehind,adaptive,pauseBoss,ammo,armor,health,powerups,weapons,customInventory,excludeCounted,debug;
 float strength,minDistance,maxDistance,minGroup,maxGroup,minInterval,maxInterval;
 float maxActive,maxTotal,maxThreat,maxLocalThreat,maxAttempts,coverage,seed;
 int stream,streamPolicy,safeStasis,initialBudgetMode,populationPolicy;
 float initialMonsterCount,initialMonsterPercent;
 float activateDistance,prefetchDistance,stasisDistance,spawnBudget,packSize,packInterval,softLimit;
 float threatCosts[17]; // Order matches the documented stock profile table.
} encounter_config_t;
static inline encounter_config_t ED_DefaultConfig(void){encounter_config_t c={0};c.bossProtection=2;c.weighted=c.noBehind=c.adaptive=c.pauseBoss=c.excludeCounted=1;c.initialMonsterCount=500;c.initialMonsterPercent=25;c.activateDistance=1024;c.prefetchDistance=1536;c.stasisDistance=2048;c.spawnBudget=c.packSize=8;c.packInterval=2;c.softLimit=128;c.strength=50;c.minDistance=1024;c.maxDistance=2048;c.minGroup=1;c.maxGroup=3;c.minInterval=45;c.maxInterval=90;c.maxActive=8;c.maxTotal=24;c.maxThreat=16;c.maxLocalThreat=24;c.maxAttempts=16;c.coverage=100;{const float weights[17]={1,2,3,1,1,1,2,3,4,6,6,7,4,8,5,20,18};for(int i=0;i<17;++i)c.threatCosts[i]=weights[i];}return c;}
void ED_ValidateConfig(encounter_config_t*);
void ED_Configure(const encounter_config_t*);
void ED_Clear(void);
void ED_BeginMap(int capacity);
void ED_Capture(struct mobj_s*);
void ED_MapReady(void);
extern void (*ED_StartupSettings)(void);
void ED_MapThingIndex(int);
void ED_OriginalRevived(struct mobj_s*);
int ED_DeferOriginal(const void* mapthing,int type);
void ED_OriginalView(const double* eye,const double* right,const double* up,const double* forward,double horizontal,double vertical,int all);
typedef struct {unsigned deferred,queued,materialized,active,eager,stasis;unsigned urgent,blocked;double activationMs;
 unsigned filtered,requested,requiredEager,initialLive,initialDeferred,overrun,liveOriginals,liveAdditional,routine,effectiveSoftTarget,populationPolicy;
 unsigned reasons[10];double setupMs,ticMs;unsigned sampledTics;} encounter_population_t;
encounter_population_t ED_Population(void);
const char* ED_PopulationReasonName(int);
void ED_Tick(void);
void ED_Forget(struct mobj_s*);
void ED_Died(struct mobj_s*);
void ED_Respawn(struct mobj_s*,struct mobj_s*);
void ED_SetLoading(int);
int ED_SpawnRandom(void);
size_t ED_Save(unsigned char*);
size_t ED_Load(const unsigned char*,size_t);
#ifdef __cplusplus
}
#endif
#endif

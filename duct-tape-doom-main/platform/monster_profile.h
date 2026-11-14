#ifndef HELLFORGE_MONSTER_PROFILE_H
#define HELLFORGE_MONSTER_PROFILE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct { uint64_t start,child; unsigned category,depth; int active,actorType; } MP_Scope;
enum MP_Category {MP_TIC,MP_THINKERS,MP_MOBJ,MP_LOOK,MP_CHASE,MP_PLAYERS,MP_POSITION,MP_TRYMOVE,MP_THINGS,MP_LINES,MP_SIGHT,MP_SOLIDSIGHT,MP_FLUIDCHASE,MP_FLUIDTICK,MP_SUPPORT,MP_TACTICS,MP_FEED,MP_FRENZY,MP_GEOMETRY,MP_RENDER,MP_SPRITE_LIGHT,MP_SHADOW,MP_SUBSECTOR,MP_DIG_POSITION,MP_DIG_HEIGHT,MP_GPU_UPLOAD,MP_GPU_ENCODE,MP_GPU_SUBMIT,MP_TACTIC_MOVE,
#ifdef DOOM_MONSTER_PROFILE_ARGUMENTS
MP_ARGUMENTS,
#endif
MP_CATEGORIES};
enum MP_Counter {MP_THING_VISITS,MP_LINE_VISITS,MP_LINE_DUPLICATES,MP_THING_CALLBACKS,MP_LINE_CALLBACKS,MP_ITERATOR_STOPS,MP_SIGHT_REJECT,MP_SIGHT_BSP,MP_CACHE_HIT,MP_CACHE_MISS,MP_CACHE_CLEAR,MP_CACHE_INVALIDATE,MP_OVERLAP,MP_FLUID_STEP,MP_FLUID_FAILED,MP_FEED_CANDIDATE,MP_FRENZY_CANDIDATE,MP_POSITION_FAILED,MP_MOVE_FAILED,MP_CACHE_PEAK,MP_ACQUIRED,MP_ACTIVATED,MP_SIGHT_VISIBLE,MP_SIGHT_BLOCKED,MP_TACTIC_CANDIDATE,MP_COUNTERS};
#ifdef DOOM_MONSTER_BENCHMARK
void MP_BenchmarkStage(const char *stage);
#define MP_SETUP_STAGE(stage) do {MP_STAGE(stage);MP_BenchmarkStage(stage);} while(0)
#else
#define MP_SETUP_STAGE(stage) MP_STAGE(stage)
#endif
#ifdef DOOM_MONSTER_PROFILING
int MP_Enabled(void);
void MP_SetEnabled(int enabled);
void MP_PatchedType(int type);
void MP_ForgetActor(const void *actor);
#define mp_enabled MP_Enabled()
MP_Scope MP_Begin(unsigned category);
void MP_End(MP_Scope scope);
void MP_Count(unsigned counter,uint64_t amount);
void MP_Max(unsigned counter,uint64_t amount);
void MP_Init(const char *folder);
void MP_Startup(void);
void MP_Stage(const char *stage);
#define MP_STAGE(stage) MP_Stage(stage)
void MP_Window(int enabled);
void MP_Tic(int tic);
void MP_AutoTic(int tic);
void MP_ExternalTics(void);
void MP_Flush(void);
uint64_t MP_Now(void);
#define MP_BODY(name) name##_mp_body
#define MP_COUNT(c,n) do {if(mp_enabled)MP_Count(c,n);} while(0)
#define MP_MAX(c,n) do {if(mp_enabled)MP_Max(c,n);} while(0)
#define MP_COLLECT const int mp_collect=mp_enabled
#define MP_LOCAL_COUNT(c,n) do {if(mp_collect)MP_Count(c,n);} while(0)
#else
#define MP_BODY(name) name
#define MP_COUNT(c,n) ((void)0)
#define MP_MAX(c,n) ((void)0)
#define MP_COLLECT ((void)0)
#define MP_LOCAL_COUNT(c,n) ((void)0)
#define MP_STAGE(stage) ((void)0)
#endif
#ifdef __cplusplus
}
#ifdef DOOM_MONSTER_PROFILING
struct MP_AutoScope {MP_Scope scope; explicit MP_AutoScope(unsigned c):scope{} {if(mp_enabled)scope=MP_Begin(c);} ~MP_AutoScope(){if(scope.active)MP_End(scope);}};
#define MP_AUTO(c) MP_AutoScope mp_scope(c)
#else
#define MP_AUTO(c) ((void)0)
#endif
#endif
#endif

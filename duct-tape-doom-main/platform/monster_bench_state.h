#ifndef HELLFORGE_MONSTER_BENCH_STATE_H
#define HELLFORGE_MONSTER_BENCH_STATE_H
/* Read-only, benchmark-only canonical sidecar observation. Never serialized. */
#ifdef DOOM_MONSTER_BENCHMARK
#include <stdint.h>
#include <stddef.h>
#include <string.h>
typedef struct {
    uint64_t value;
    size_t count;
    void *context;
    const void *(*actor_at)(void*,size_t);
    uint32_t (*actor_id)(void*,const void*);
} MP_StateHash;
static inline void MP_StateField(MP_StateHash *h,uint64_t value){
    for(int i=0;i<8;++i){h->value=(h->value^(value&255))*UINT64_C(1099511628211);value>>=8;}
}
static inline void MP_StateActor(MP_StateHash *h,const void *actor){MP_StateField(h,h->actor_id(h->context,actor));}
static inline void MP_StateFloat(MP_StateHash *h,float value){uint32_t bits;memcpy(&bits,&value,sizeof(bits));MP_StateField(h,bits);}
#ifdef __cplusplus
extern "C" {
#endif
void MP_StateTactics(MP_StateHash*);
void MP_StateRecovery(MP_StateHash*);
void MP_StateFluid(MP_StateHash*);
void MP_StateCombat(MP_StateHash*);
#ifdef __cplusplus
}
#endif
#endif
#endif

#ifndef P_INCENDIARY_H
#define P_INCENDIARY_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif
#include "p_mobj.h"

enum { OIL_LEAK, OIL_RUPTURE, OIL_SPILL, OIL_SPOKES = 32 };
typedef struct {
    fixed_t x,y,radius;
    int sector,age,remaining,kind;
    angle_t leakAngle; /* Direction of the latest impact spray. */
    /* Fraction of radius available in each equally spaced direction. */
    unsigned char reach[OIL_SPOKES];
} p_oil_pool_view_t;

void P_IncendiaryConfigure(int enabled);
int P_IncendiaryEnabled(void);
void P_IncendiarySync(void);
void P_IncendiaryRestoreActors(void);
void P_IncendiarySpawn(mobj_t *actor);
int P_IncendiaryDamage(mobj_t *actor,mobj_t *inflictor,mobj_t *source,int damage);
void P_IncendiaryTick(mobj_t *actor);
void P_IncendiaryEndTic(void);
void P_IncendiaryForget(mobj_t *actor);
void P_IncendiaryClear(void);
int P_IncendiaryPoolCount(void);
int P_IncendiaryPoolGet(int index,p_oil_pool_view_t *out);
size_t P_IncendiarySave(unsigned char *destination);
size_t P_IncendiaryLoad(const unsigned char *data,size_t length);
void P_DamageMobjFire(mobj_t *target,mobj_t *inflictor,mobj_t *source,int damage);
void P_RadiusAttackFire(mobj_t *spot,mobj_t *source,int damage);

#ifdef __cplusplus
}
#endif
#endif

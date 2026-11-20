/* Corrosive barrel gameplay: persistent floor pools and acid exposure.
   This is independent of the optional blood and elemental-fire systems. */
#ifndef P_ACID_H
#define P_ACID_H

#include <stddef.h>
#include "p_mobj.h"

#ifdef __cplusplus
extern "C" {
#endif

enum { P_ACID_POOL_LEAK = 0, P_ACID_POOL_SPILL = 1 };

typedef struct {
    fixed_t x, y, radius;
    int sector;
    int age;       /* Tics since the pool was created. */
    int remaining; /* Tics until the pool expires. */
    int kind;      /* P_ACID_POOL_LEAK or P_ACID_POOL_SPILL. */
    fixed_t ownerX, ownerY, ownerZ, ownerHeight; /* Set for a live barrel leak. */
    int hasOwner;
} p_acid_pool_view_t;

void P_AcidConfigure(int enabled);
int P_AcidEnabled(void);
int P_AcidDamageActive(void);
void P_AcidBarrelDamaged(mobj_t *barrel, mobj_t *source, int healthBefore, int healthAfter);
void P_AcidBarrelExplode(mobj_t *barrel, mobj_t *source);
void P_AcidTick(mobj_t *actor);
void P_AcidForget(mobj_t *actor);
void P_AcidClear(void);

int P_AcidPoolCount(void);
int P_AcidPoolGet(int index, p_acid_pool_view_t *out);

size_t P_AcidSave(unsigned char *destination);
size_t P_AcidLoad(const unsigned char *data, size_t length);

#ifdef __cplusplus
}
#endif
#endif

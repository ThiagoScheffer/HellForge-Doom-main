#ifndef HELLFORGE_P_HAZARD_H
#define HELLFORGE_P_HAZARD_H

#include "p_mobj.h"
#include "r_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { HAZARD_NONE, HAZARD_ACID, HAZARD_LAVA } hazard_kind_t;
typedef struct { hazard_kind_t kind; int damage; } hazard_info_t;
#define ACID_DISSOLVE_TICS (3 * TICRATE)

void P_HazardConfigure(int damage, int avoidance, int dissolution);
int P_HazardAvoidanceEnabled(void);
int P_HazardDissolutionEnabled(void);
int P_HazardFloorTickDue(const mobj_t *actor);
void P_HazardTick(mobj_t *actor);
void P_DamageMobjLava(mobj_t *actor,int damage);
void P_HazardRecordDeath(mobj_t *actor,hazard_kind_t kind);
int P_HazardAcidCorpse(const mobj_t *actor);
int P_HazardLavaCorpse(const mobj_t *actor);
int P_HazardDeathAge(const mobj_t *actor);
int P_HazardDissolved(const mobj_t *actor);

static inline int P_HazardSupportedType(mobjtype_t type)
{
    switch(type) {
    case MT_POSSESSED: case MT_SERGEANT: case MT_TROOP:
    case MT_BRUISER: case MT_KNIGHT: return 1;
    default: return 0;
    }
}
int P_HazardAvoidanceChance(const mobj_t *actor);
int P_HazardBoundaryDue(mobj_t *actor);
int P_HazardSectorDamage(const sector_t *sector);
hazard_kind_t P_HazardFloorKind(int flat);
hazard_info_t P_HazardSectorInfo(const sector_t *sector);
int P_HazardFloorContact(const mobj_t *actor, const sector_t *sector);

#ifdef __cplusplus
}
#endif
#endif

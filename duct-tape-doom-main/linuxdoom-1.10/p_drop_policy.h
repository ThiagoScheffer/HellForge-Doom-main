#ifndef HELLFORGE_P_DROP_POLICY_H
#define HELLFORGE_P_DROP_POLICY_H

typedef enum {
    MONSTER_DROPS_CLASSIC,
    MONSTER_DROPS_AMMO,
    MONSTER_DROPS_SCARCE
} monster_drop_mode_t;

#ifdef __cplusplus
extern "C" {
#endif
void P_SetMonsterWeaponDrops(int mode);
#ifdef __cplusplus
}
#endif

#endif

#ifndef P_BOOM_H
#define P_BOOM_H
#include "p_spec.h"
/* Returns -1 for a vanilla special, otherwise handles/rejects this trigger.
 * trigger: 0 walk, 1 use, 2 gun. */
int P_BoomActivate(line_t *line,mobj_t *actor,int side,int trigger);
void P_BoomSpawn(void);
void P_BoomTick(void);
extern int p_boom_compat;
void P_BoomStairFinished(int group);
void T_BoomElevator(floormove_t *floor);
sector_t *P_BoomRenderSector(sector_t *sector,sector_t *copy,int back);
size_t P_BoomSave(byte *destination);
size_t P_BoomLoad(const byte *data,size_t length);

#endif

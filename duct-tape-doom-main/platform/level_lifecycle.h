// Optional renderer barrier before the game releases level-owned data.
#ifndef DOOM_LEVEL_LIFECYCLE_H
#define DOOM_LEVEL_LIFECYCLE_H
#ifdef __cplusplus
extern "C" {
#endif
extern void (*R_BeforeLevelReset)(void);
#ifdef __cplusplus
}
#endif
#endif

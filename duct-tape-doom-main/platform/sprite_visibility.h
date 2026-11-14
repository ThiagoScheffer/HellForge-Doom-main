#ifndef HELLFORGE_SPRITE_VISIBILITY_H
#define HELLFORGE_SPRITE_VISIBILITY_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
enum {SVF_DISABLED,SVF_SECONDARY,SVF_OTHER,SVF_VOXEL,SVF_POSE,SVF_WOUND,SVF_LIQUID,SVF_ARTWORK,SVF_INVALID,SVF_REASONS};
typedef struct {uint64_t examined,rejected,retained,eligible,filter_ns,fallback[SVF_REASONS];} SpriteVisibilityStats;
// Last completed render-thread selection pass; diagnostic consumers only.
SpriteVisibilityStats I_SpriteVisibilityStats(void);
#ifdef __cplusplus
}
#endif
#endif

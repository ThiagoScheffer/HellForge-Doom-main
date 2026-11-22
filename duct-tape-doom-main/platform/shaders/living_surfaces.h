// Shared by image classification, camera packing and the world shader.
#ifndef DOOM_LIVING_SURFACES_H
#define DOOM_LIVING_SURFACES_H

#define LS_NONE           0u
#define LS_HELL_FLESH     1u
#define LS_SPINE_WALL     2u
#define LS_BODY_MASS      3u
#define LS_TENTACLE_MEAT  4u
#define LS_DRY_SKIN       5u
#define LS_TORTURED_FACES 6u
#define LS_MARBLE_FACES   7u
#define LS_PINNED_BODY    8u
#define LS_TELEPORT     9u
#define LS_BLOOD_FLOW   10u
#define LS_EYE_SWITCH   11u
#define LS_FIRE         12u
#define LS_SKY          13u
#define LS_SKULL_SWITCH 14u
#define LS_INTESTINES   15u

// Camera map.w: flags in bits 0-4, intensity (1-3) in bits 6-7.
#define WORLD_SOFT_LIGHT   1u
#define LS_ENABLED        2u
#define LS_MORPH          4u
#define LS_PULSE          8u
#define LS_WETNESS       16u
#define LS_INTENSITY_SHIFT 6u
#define LS_INTENSITY_MASK  3u

#endif

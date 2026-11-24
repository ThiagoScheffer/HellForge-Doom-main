#ifndef DOOM_SCENE3D_GORE_STOCK_H
#define DOOM_SCENE3D_GORE_STOCK_H

#include "gore_policy.h"

namespace doom3d {

struct GoreFingerprint { uint32_t hash; int width,height,left,top; };
struct GoreBox { int left,top,right,bottom; };
struct GoreCavities { int chestX,chestY,bellyX,bellyY; };
struct GoreFrame {
    int type;
    const char *name;
    GoreFingerprint pixels;
    GoreBox head;
};

// Doom II reference IWAD. Each entry identifies exact palette/coverage data
// plus patch origin. Coordinates are reviewed in the original, unmirrored
// patch space. A zero head box on SKEL M?Q means the stock skull is gone.
inline constexpr GoreFrame goreFrames[]={
    {MT_POSSESSED,"POSSH0",{0x2c7b8171u,39,55,19,52},{0,0,0,0}},
    {MT_POSSESSED,"POSSI0",{0x62154af2u,35,46,16,47},{0,0,0,0}},
    {MT_POSSESSED,"POSSJ0",{0x626a47e4u,43,34,22,42},{0,0,0,0}},
    {MT_POSSESSED,"POSSK0",{0xbcd66239u,48,27,23,29},{0,0,0,0}},
    {MT_POSSESSED,"POSSL0",{0x2cc65c9fu,47,17,22,12},{0,0,0,0}},
    {MT_SHOTGUY,"SPOSH0",{0x60c0a2f4u,33,60,14,57},{0,0,0,0}},
    {MT_SHOTGUY,"SPOSI0",{0xed3d3c98u,35,50,16,51},{0,0,0,0}},
    {MT_SHOTGUY,"SPOSJ0",{0x48281e02u,42,35,22,43},{0,0,0,0}},
    {MT_SHOTGUY,"SPOSK0",{0xe4c525e6u,48,27,23,29},{0,0,0,0}},
    {MT_SHOTGUY,"SPOSL0",{0x3a8a1e5fu,52,17,24,12},{0,0,0,0}},
    {MT_TROOP,"TROOI0",{0x6e8b2345u,42,62,22,57},{18,0,32,20}},
    {MT_TROOP,"TROOJ0",{0x7d231cceu,41,59,21,54},{19,0,33,17}},
    {MT_TROOP,"TROOK0",{0xed566381u,40,54,18,54},{21,0,36,15}},
    {MT_TROOP,"TROOL0",{0x0d48c282u,48,46,23,45},{22,0,37,15}},
    {MT_TROOP,"TROOM0",{0x6cdd7d63u,58,22,29,19},{33,0,49,10}},
    {MT_UNDEAD,"SKELL1",{0xd8529c21u,66,72,31,68},{19,0,36,9}},
    {MT_UNDEAD,"SKELL2",{0x609aae44u,44,71,18,67},{26,0,44,10}},
    {MT_UNDEAD,"SKELL3",{0x783bfb0au,59,71,24,67},{44,0,59,12}},
    {MT_UNDEAD,"SKELL4",{0x4595679eu,73,73,34,68},{54,0,69,12}},
    {MT_UNDEAD,"SKELL5",{0x1018be93u,60,74,32,69},{28,0,47,12}},
    {MT_UNDEAD,"SKELL6",{0xdc3c1f40u,46,73,21,68},{0,0,19,12}},
    {MT_UNDEAD,"SKELL7",{0x43f1e625u,57,72,25,68},{0,0,17,10}},
    {MT_UNDEAD,"SKELL8",{0x8ad751b1u,69,73,33,69},{3,0,21,11}},
    {MT_UNDEAD,"SKELM0",{0x7c909e61u,56,78,27,74},{0,0,0,0}},
    {MT_UNDEAD,"SKELN0",{0xc903b6ccu,74,70,38,65},{0,0,0,0}},
    {MT_UNDEAD,"SKELO0",{0xb2bc2c91u,65,55,28,51},{0,0,0,0}},
    {MT_UNDEAD,"SKELP0",{0x9a1111b3u,62,38,28,33},{0,0,0,0}},
    {MT_UNDEAD,"SKELQ0",{0xb39f8610u,65,22,40,19},{0,0,0,0}},
};

struct GoreLivingFrame {
    int type;
    GoreFingerprint pixels;
    GoreCavities cavities;
};

// Torso landmarks from the stock standing, attack and pain patches.
// Forward and quarter views (including the engine-mirrored art) have
// reviewed landmarks. Other views retain the ordinary sprite.
inline constexpr GoreLivingFrame goreLivingFrames[]={
    {MT_POSSESSED,{0xcbde3c78u,41,55,18,50},{16,14,16,24}}, // POSSA1
    {MT_POSSESSED,{0xca3e75c0u,36,54,17,50},{21,14,22,26}}, // POSSA2A8
    {MT_POSSESSED,{0x395f063eu,37,55,19,52},{12,14,14,24}}, // POSSB1
    {MT_POSSESSED,{0x27ec5c53u,35,54,18,51},{21,14,23,25}}, // POSSB2B8
    {MT_POSSESSED,{0xff101ef1u,38,55,17,50},{15,15,15,25}}, // POSSC1
    {MT_POSSESSED,{0xbbcc075eu,37,54,17,50},{21,13,23,24}}, // POSSC2C8
    {MT_POSSESSED,{0x01299a90u,40,55,16,52},{17,15,18,25}}, // POSSD1
    {MT_POSSESSED,{0x9021416fu,35,54,18,51},{21,14,22,25}}, // POSSD2D8
    {MT_POSSESSED,{0x568e6f0eu,26,55,12,50},{13,15,13,25}}, // POSSE1
    {MT_POSSESSED,{0x29c12b78u,43,54,21,50},{31,14,30,26}}, // POSSE2E8
    {MT_POSSESSED,{0xe00e505du,27,55,13,50},{15,15,15,26}}, // POSSF1
    {MT_POSSESSED,{0x57fdc531u,45,54,23,50},{33,15,33,27}}, // POSSF2F8
    {MT_POSSESSED,{0xcc173d70u,39,55,20,52},{24,15,23,28}}, // POSSG1
    {MT_POSSESSED,{0x5d5b0074u,36,55,16,53},{26,15,25,27}}, // POSSG2G8
    {MT_SHOTGUY,{0xcdfcc2a3u,36,55,18,50},{16,14,16,25}}, // SPOSA1
    {MT_SHOTGUY,{0x7c80d36du,32,54,13,50},{20,14,20,25}}, // SPOSA2A8
    {MT_SHOTGUY,{0x9b9bd3beu,35,55,19,52},{12,14,14,25}}, // SPOSB1
    {MT_SHOTGUY,{0x1322203fu,27,54,12,51},{14,14,14,25}}, // SPOSB2B8
    {MT_SHOTGUY,{0x437d59fcu,32,55,17,50},{17,15,16,26}}, // SPOSC1
    {MT_SHOTGUY,{0xc437c3f3u,29,54,10,50},{14,14,13,25}}, // SPOSC2C8
    {MT_SHOTGUY,{0x85082e35u,34,55,16,52},{17,15,14,25}}, // SPOSD1
    {MT_SHOTGUY,{0x0ff5f92eu,28,55,11,51},{15,14,14,26}}, // SPOSD2D8
    {MT_SHOTGUY,{0xe3dca1b3u,26,55,12,50},{14,14,14,25}}, // SPOSE1
    {MT_SHOTGUY,{0xe596bea1u,39,54,17,50},{28,15,27,26}}, // SPOSE2E8
    {MT_SHOTGUY,{0xa23b6967u,27,55,13,50},{15,15,15,26}}, // SPOSF1
    {MT_SHOTGUY,{0xb33b8947u,43,54,21,50},{34,15,33,27}}, // SPOSF2F8
    {MT_SHOTGUY,{0x6fce6888u,31,55,12,52},{19,14,18,26}}, // SPOSG1
    {MT_SHOTGUY,{0x1ee56a07u,33,54,13,51},{23,14,23,25}}, // SPOSG2G8
    {MT_TROOP,{0x04699755u,41,57,19,52},{20,19,21,27}}, // TROOA1
    {MT_TROOP,{0xa0ff683au,40,55,17,50},{21,18,21,26}}, // TROOA2A8
    {MT_TROOP,{0x7387ca06u,39,56,17,51},{19,19,20,27}}, // TROOB1
    {MT_TROOP,{0x36ed6d86u,38,57,13,52},{20,18,21,26}}, // TROOB2B8
    {MT_TROOP,{0x8572763au,39,60,17,55},{20,19,21,28}}, // TROOC1
    {MT_TROOP,{0x1ca98717u,41,58,14,53},{22,18,22,26}}, // TROOC2C8
    {MT_TROOP,{0x7580afe5u,37,57,16,52},{17,19,18,27}}, // TROOD1
    {MT_TROOP,{0xf7b200a3u,45,55,17,50},{22,18,23,26}}, // TROOD2D8
    {MT_TROOP,{0xa955db99u,49,60,30,55},{28,21,30,29}}, // TROOE1
    {MT_TROOP,{0x117c55abu,32,56,11,51},{16,17,21,26}}, // TROOE2E8
    {MT_TROOP,{0x5dd6ca03u,44,55,18,50},{22,19,22,27}}, // TROOF1
    {MT_TROOP,{0x1926a172u,45,54,25,49},{23,16,25,24}}, // TROOF2F8
    {MT_TROOP,{0x3ef163c9u,32,55,5,50},{16,19,15,27}}, // TROOG1
    {MT_TROOP,{0x625c8700u,55,55,25,50},{33,18,31,25}}, // TROOG2G8
    {MT_TROOP,{0x81430be7u,41,55,18,50},{25,17,23,25}}, // TROOH1
    {MT_TROOP,{0xae40f8a4u,31,56,6,51},{16,18,16,24}}, // TROOH2H8
    {MT_FATSO,{0x0a12ad24u,73,65,39,60},{40,27,43,35}}, // FATTA1
    {MT_FATSO,{0xd17bb8f2u,80,66,39,61},{53,25,57,38}}, // FATTA2A8
    {MT_FATSO,{0x957f5de4u,88,70,43,65},{43,28,44,42}}, // FATTB1
    {MT_FATSO,{0x2bcdd2d4u,83,70,43,65},{48,25,48,37}}, // FATTB2B8
    {MT_FATSO,{0x0e4b17d5u,82,70,43,65},{43,26,44,38}}, // FATTC1
    {MT_FATSO,{0x136e0df9u,83,68,43,63},{37,24,40,35}}, // FATTC2C8
    {MT_FATSO,{0x05ea76e8u,73,65,38,60},{39,24,38,32}}, // FATTD1
    {MT_FATSO,{0x182e3250u,87,68,43,63},{46,25,45,38}}, // FATTD2D8
    {MT_FATSO,{0xc74d0188u,88,70,43,65},{45,24,45,36}}, // FATTE1
    {MT_FATSO,{0x73e0cb64u,81,66,43,61},{40,25,44,40}}, // FATTE2E8
    {MT_FATSO,{0x198f7fe9u,82,70,42,65},{43,25,43,37}}, // FATTF1
    {MT_FATSO,{0xfa702546u,69,67,43,62},{42,25,44,33}}, // FATTF2F8
    {MT_FATSO,{0x261460efu,87,65,42,60},{43,26,44,41}}, // FATTG1
    {MT_FATSO,{0xeb4d85b2u,98,64,51,59},{61,23,63,30}}, // FATTG2G8
    {MT_FATSO,{0x3f971864u,83,65,42,60},{43,26,43,39}}, // FATTH1
    {MT_FATSO,{0x1e94f2fau,101,64,50,59},{64,22,59,30}}, // FATTH2H8
    {MT_FATSO,{0xe9d06f91u,84,68,42,63},{44,28,44,41}}, // FATTI1
    {MT_FATSO,{0x90d197ebu,95,69,51,64},{59,28,55,35}}, // FATTI2I8
    {MT_FATSO,{0x9bc00468u,74,67,32,62},{43,30,43,42}}, // FATTJ1
    {MT_FATSO,{0xe1883566u,88,66,47,62},{39,29,40,40}}, // FATTJ2
};

}
#endif

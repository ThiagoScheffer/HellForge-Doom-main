#pragma once

#include <array>
#include <cstdint>
#include <cstring>

namespace doom_wall_immersion {
enum class Effect : uint8_t { BleedingBody, CorpseAmbience, SkullEyes, ReactiveEyes, RedLED, GreenLED, Fluorescent };

using Rect=std::array<int,4>;
struct EyePair { float lx,ly,rx,ry,width,height,tilt; };
struct Artwork {
    const char *name;
    Effect effect;
    int width,height;
    uint32_t fingerprint;
    std::array<Rect,4> regions{};
    EyePair eyes{};
};

using E=Effect;
// Texel rectangles are half-open and relative to the original patch. The
// fingerprints and landmarks were read from the local Doom and Doom II IWADs.
inline constexpr Artwork artworks[]={
    {"BODY_1", E::BleedingBody,128,128,0x31b202f2, {{{39,78,40,79},{54,90,55,91}}}},
    {"BODY_2", E::BleedingBody,128,128,0x1adcedac, {{{75,44,76,45},{88,75,89,76}}}},
    {"WALL50_1",E::CorpseAmbience,128,128,0x0da35b3e, {{{50,58,65,89}}}},
    {"WALL50_2",E::CorpseAmbience,128,128,0xc0a337f8, {{{57,44,75,70}}}},
    {"WALL51_2",E::CorpseAmbience,64,128,0x2be92aa0, {{{26,51,40,91}}}},
    {"WALL51_3",E::CorpseAmbience,64,128,0x7f91e78b, {{{28,69,39,94}}}},
    {"W108_2", E::SkullEyes,16,24,0x2b0f9e6e, {{{5,10,6,11},{11,10,12,11}}}},
    {"W108_3", E::SkullEyes,16,24,0x9f2b865a, {{{5,14,6,15},{11,14,12,15}}}},
    {"W108_4", E::SkullEyes,16,24,0x308112b5, {{{5,14,6,15},{11,14,12,15}}}},
    {"WALL42_1",E::ReactiveEyes,40,128,0x67b5a37e, {}, {12,21,27,21,3.2f,2.1f,.27f}},
    {"WALL42_6",E::ReactiveEyes,40,128,0x86ce8b6d, {}, {12,20,27,20,3.4f,2.1f,.21f}},
    {"WALL42_5",E::ReactiveEyes,40,128,0xead1a5b0, {}, {15,21,24,21,3.f,2.1f,.30f}},
    {"WALL30_2",E::ReactiveEyes,64,128,0x30674bc2, {}, {25,31,39,31,3.4f,2.1f,.21f}},
    {"WALL30_3",E::ReactiveEyes,64,128,0x675c44cd, {}, {24,38,39,38,3.2f,2.1f,.27f}},
    {"WALL30_4",E::ReactiveEyes,64,128,0x81ed3568, {}, {27,33,39,33,3.1f,2.1f,.30f}},
    {"DOOR9_2", E::RedLED,128,128,0x45195072, {{{103,2,114,13},{43,85,48,102},{7,100,12,105},{115,100,120,105}}}},
    {"DOOR15_1",E::RedLED,64,128,0x9addabe8, {{{39,2,50,13},{31,77,33,87},{8,109,13,111},{51,101,56,111}}}},
    {"DOOR15_2",E::RedLED,64,128,0x4c20f12f, {{{39,2,50,13},{31,77,33,87},{8,101,13,111},{51,101,56,111}}}},
    {"DOOR15_3",E::GreenLED,64,128,0xd57d9ee1, {{{9,82,11,84},{12,82,14,84},{15,82,17,84}}}},
    {"RW38_1", E::Fluorescent,64,128,0xf0f05578, {{{5,2,60,12}}}},
    {"RW38_2", E::Fluorescent,64,128,0xf5be8b48, {{{5,116,60,126}}}},
    {"RW38_3", E::Fluorescent,64,128,0x34333573, {{{3,120,29,124},{35,120,61,124}}}},
    {"RW38_4", E::Fluorescent,64,128,0xe3f5b1c1, {{{3,120,29,124},{35,120,61,124}}}},
    {"RW39_1", E::Fluorescent,64,128,0x688a055c, {{{4,4,59,12}}}}
};

inline const Artwork *findArtwork(const char *name) {
    char canonical[9]={};
    for(int i=0;i<8&&name[i];++i)
        canonical[i]=name[i]>='a'&&name[i]<='z'?char(name[i]-'a'+'A'):name[i];
    for(const auto &art:artworks)if(std::strcmp(art.name,canonical)==0)return &art;
    return nullptr;
}
}

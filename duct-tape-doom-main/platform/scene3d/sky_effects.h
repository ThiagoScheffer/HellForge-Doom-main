#ifndef DOOM_SKY_EFFECTS_H
#define DOOM_SKY_EFFECTS_H

#include <array>
#include <cstdint>

namespace doom3d {

enum class SkyEffectDepthSlot : uint8_t {
    FarBackground, BehindMiddle, BetweenMiddleAndNear, InFrontOfNear
};

struct SkyEffectAnchor {
    float azimuth, rise, radius; // radians, tangent of elevation, map units
};

struct SkyFlameParams {
    float width, height, intensity, flickerRate;
    bool enabled;
};

struct SkySmokeParams {
    float width, height, density, riseSpeed, windResponse, turbulence;
    bool enabled;
};

struct SkyFireIlluminationParams {
    float radius, strength, verticalFalloff;
};

struct SkyFireSmokeSource {
    SkyEffectAnchor anchor;
    SkyEffectDepthSlot slot;
    SkyFlameParams flame;
    SkySmokeParams smoke;
    SkyFireIlluminationParams illumination;
    uint32_t seed;
};

constexpr unsigned MaxSkyFireSources=4;

struct SkyFireSmokeProfile {
    uint64_t artwork, palette;
    int width, height;
    bool requiresScenery;
    float windSpeed, hazeBlend;
    std::array<float,3> smokeDark, smokeWarm, flameOuter, flameCore;
    std::array<SkyFireSmokeSource,MaxSkyFireSources> sources;
    unsigned count;
};

struct alignas(16) SkyFireSmokeGpuSource {
    std::array<float,4> anchor, flame, smoke, motion, illumination;
};

struct alignas(16) SkyFireSmokeGpuBlock {
    std::array<float,4> header, smokeDark, smokeWarm, flameOuter, flameCore;
    std::array<SkyFireSmokeGpuSource,MaxSkyFireSources> sources;
};

struct Image;
const SkyFireSmokeGpuBlock *skyFireSmokeData(const Image &sky,bool hasScenery);

} // namespace doom3d

#endif

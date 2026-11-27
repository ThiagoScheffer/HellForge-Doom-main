#ifndef DOOM_MATERIAL_LIGHT_POLICY_H
#define DOOM_MATERIAL_LIGHT_POLICY_H

#include "emissive.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace doom_material_light {

enum class Kind : uint8_t { None, Indicator, Display, Fixture, Diffuser, Glass, Fire, Liquid, Custom };

inline Kind classify(int rule, bool flat, bool indicator, bool explicitMask,
                     bool correctedLed, bool correctedDiffuse, float weight) {
    if (!(weight > 0.f)) return Kind::None;
    if (indicator) return Kind::Indicator;
    switch (rule) {
    case DOOM_EMISSIVE_LAMP: return Kind::Fixture;
    case DOOM_EMISSIVE_PANEL: return Kind::Display;
    case DOOM_EMISSIVE_DIFFUSE: return Kind::Diffuser;
    case DOOM_EMISSIVE_GLASS: return Kind::Glass;
    case DOOM_EMISSIVE_LAVA:
    case DOOM_EMISSIVE_MAGMA: return flat ? Kind::Liquid : Kind::Fire;
    case DOOM_EMISSIVE_SLIME: return Kind::Liquid;
    default:
        if (explicitMask) return Kind::Custom;
        if (correctedDiffuse) return Kind::Diffuser;
        if (correctedLed) return Kind::Indicator;
        return Kind::Fixture;
    }
}

inline float effectivePower(float nominal, float shine) {
    if (!std::isfinite(nominal) || !std::isfinite(shine) || nominal <= 0.f) return 0.f;
    return nominal * std::clamp(shine, 0.f, 1.f);
}

inline bool canEmit(float weight, float radius) {
    return std::isfinite(weight) && std::isfinite(radius) && weight > 0.f && radius > 0.f;
}

// Caps apply to short point emitters. Pool transport uses its own geometry.
inline float sourceRadius(float legacy, Kind kind, bool sourceDriven) {
    if (!std::isfinite(legacy) || legacy <= 0.f) return 0.f;
    if (!sourceDriven) return legacy;
    if (kind == Kind::None) return 0.f;
    if (kind == Kind::Indicator) return std::min(legacy, 88.f);
    if (kind == Kind::Display) return std::min(legacy * .65f, 128.f);
    return legacy;
}

inline size_t maskSize(int width, int height) {
    constexpr int limit = 8192;
    if (width <= 0 || height <= 0 || width > limit || height > limit) return 0;
    return static_cast<size_t>(width) * height;
}

// Indexed coverage is binary here. The PNG's own alpha is already in value.
inline uint8_t explicitTexel(float value, uint8_t coverage) {
    if (!coverage || !std::isfinite(value)) return 0;
    return static_cast<uint8_t>(std::clamp(value * 255.f, 0.f, 255.f));
}

// Zero denotes a missing mask. A valid all-zero replacement has an identity.
inline uint64_t explicitMaskIdentity(const uint8_t *mask, size_t size, int width, int height) {
    if (!mask || !size || size != maskSize(width, height)) return 0;
    uint64_t hash = 14695981039346656037ull;
    for (int dimension : {width, height}) {
        hash = (hash ^ (dimension & 255)) * 1099511628211ull;
        hash = (hash ^ (dimension >> 8)) * 1099511628211ull;
    }
    for (size_t i = 0; i < size; ++i) hash = (hash ^ mask[i]) * 1099511628211ull;
    return hash ? hash : 1;
}

} // namespace doom_material_light
#endif

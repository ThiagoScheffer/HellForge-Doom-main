#ifndef DOOM_LIGHTING_MODEL_H
#define DOOM_LIGHTING_MODEL_H

#include <algorithm>
#include <cmath>

namespace doom_lighting_model {

enum Model : int { Enhanced = 0, Hybrid = 1, Physical = 2 };

constexpr float hybridAmbientRetention = .25f;
constexpr float physicalAmbientRetention = .05f;

inline int validatedModel(int model) {
    return model == Hybrid || model == Physical ? model : Enhanced;
}

inline int effectiveModel(int requested, bool accelerated, bool bakedLights, bool emissive) {
    return accelerated && bakedLights && emissive ? validatedModel(requested) : Enhanced;
}

inline bool sourceDriven(int model) {
    return model == Hybrid || model == Physical;
}

inline float ambientRetention(int model) {
    switch (model) {
    case Hybrid: return hybridAmbientRetention;
    case Physical: return physicalAmbientRetention;
    default: return 1.f;
    }
}

// Interior ambient before shot-out room dimming.
inline float interiorAmbient(float authoredLevel, int model) {
    if (!sourceDriven(model))
        return std::clamp(authoredLevel, .12f, 1.f);
    return std::clamp(authoredLevel, 0.f, 1.f) * ambientRetention(model);
}

inline float legacyFixtureOutput(int peak) {
    const float f = std::clamp((static_cast<float>(peak) - 96.f) / 112.f, 0.f, 1.f);
    return .08f + .92f * f * f;
}

// Nominal powered output. Material strength and current power are applied separately.
inline float fixtureOutput(int peak, int model) {
    switch (model) {
    case Hybrid: return .5f + .5f * legacyFixtureOutput(peak);
    case Physical: return 1.f;
    default: return legacyFixtureOutput(peak);
    }
}

inline float exposureUpperLimit(int model) {
    switch (model) {
    case Hybrid: return 1.25f;
    case Physical: return 1.f;
    default: return 1.5f;
    }
}

inline float quantizedExposure(float ratio, int model) {
    return 1.f + std::round((std::clamp(ratio, .6f, exposureUpperLimit(model)) - 1.f) * 16.f) / 16.f;
}

} // namespace doom_lighting_model

#endif

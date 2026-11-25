#pragma once

#include <algorithm>
#include <cmath>

namespace doom_terror {

enum Mode { Off = 0, Cinematic = 1, Terror = 2, Hybrid = 3 };

constexpr float maxDisplacement1080 = 14.f;
constexpr float terrorPulseFrequency = 0.45f;
constexpr float terrorPulseAmplitude = 0.18f;

inline bool dynamicMode(int mode)
{
    return mode == Terror || mode == Hybrid;
}

inline float lowHealthStrength(int health)
{
    if (health >= 75) return 0.f;
    if (health >= 25) return (75 - health) * (0.20f / 25.f);
    if (health >= 10) return 0.40f + (25 - health) * (0.20f / 15.f);
    return 0.60f;
}

inline float intensityGain(float percent)
{
    return std::isfinite(percent) ? std::clamp(percent / 100.f, 0.f, 1.f) : 0.f;
}

struct Envelope {
    float previous = 0.f;
    float current = 0.f;

    float interpolate(float fraction) const
    {
        return previous + (current - previous) * fraction;
    }
};

struct DamageEnvelope : Envelope {
    float remaining = 0.f;

    void hit(int healthLost)
    {
        if (healthLost <= 0) return;
        float strength, seconds;
        if (healthLost <= 5) { strength = 0.30f; seconds = 0.30f; }
        else if (healthLost <= 15) { strength = 0.45f; seconds = 0.60f; }
        else if (healthLost <= 35) { strength = 0.65f; seconds = 1.20f; }
        else { strength = 0.80f; seconds = 1.50f; }
        if (strength < current) return;
        current = strength;
        remaining = std::max(remaining, seconds);
    }

    void advance(float dt)
    {
        previous = current;
        if (remaining <= dt) {
            current = remaining = 0.f;
            return;
        }
        current *= (remaining - dt) / remaining;
        remaining -= dt;
    }
};

inline void advanceLowHealth(Envelope &health, int hp, float dt)
{
    health.previous = health.current;
    const float target = lowHealthStrength(hp);
    const float seconds = target > health.current ? 0.12f : 0.55f;
    health.current += (target - health.current) * (1.f - std::exp(-dt / seconds));
    if (std::abs(target - health.current) < 0.0001f) health.current = target;
}

}

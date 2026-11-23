#include "gore_giblets.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace hellforge::giblets {
namespace {
constexpr float kGravity = 950.0f; // Doom world units / second squared; tune in-game
constexpr float kStep = 1.0f / 70.0f;
constexpr float kPi = 3.14159265358979323846f;

Vec3 operator+(Vec3 a, Vec3 b) { return {a.x+b.x, a.y+b.y, a.z+b.z}; }
Vec3 operator-(Vec3 a, Vec3 b) { return {a.x-b.x, a.y-b.y, a.z-b.z}; }
Vec3 operator*(Vec3 a, float s) { return {a.x*s, a.y*s, a.z*s}; }
float dot(Vec3 a, Vec3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
float length(Vec3 a) { return std::sqrt(dot(a,a)); }
Vec3 unit(Vec3 a, Vec3 fallback) {
    const float d = length(a);
    return d > 0.0001f ? a*(1.0f/d) : fallback;
}
bool finite(Vec3 a) {
    return std::isfinite(a.x) && std::isfinite(a.y) && std::isfinite(a.z);
}
std::uint32_t random32(std::uint32_t &s) {
    // Independent from P_Random / M_Random. No shared mutable global state.
    s ^= s << 13; s ^= s >> 17; s ^= s << 5;
    return s;
}
float random01(std::uint32_t &s) {
    return float(random32(s) & 0x00ffffffu) / float(0x01000000u);
}
float between(std::uint32_t &s, float lo, float hi) {
    return lo + (hi-lo)*random01(s);
}
struct Material { float bounce, tangentialDrag, minRestSpeed, lifetime, sizeLow, sizeHigh; };
Material material(Kind k) {
    switch (k) {
        case Kind::Bone: return {0.43f,0.79f,24.0f,4.5f,1.9f,4.3f};
        case Kind::Viscera: return {0.07f,0.42f,38.0f,3.6f,3.8f,8.5f};
        case Kind::HeavyTissue: return {0.17f,0.57f,33.0f,3.3f,2.8f,6.5f};
        case Kind::Flesh: return {0.22f,0.63f,28.0f,2.8f,1.4f,3.4f};
    }
    return {0.2f,0.6f,28.0f,3.0f,1.4f,3.0f};
}
float markScale(Kind kind) {
    switch (kind) {
        case Kind::Viscera: return 3.6f;
        case Kind::HeavyTissue: return 2.6f;
        case Kind::Bone: return 0.8f;
        case Kind::Flesh: return 1.8f;
    }
    return 1.0f;
}
} // namespace

bool System::seenEvent(std::uint64_t id) const {
    if (id == 0) return false;
    for (std::uint64_t recent : recentEvents_)
        if (recent == id) return true;
    return false;
}

int System::emit(const Burst &request) {
    // Safety gates are deliberately repeated here; the caller still owns
    // weapon, monster, severity, palette, F4 and gameplay eligibility.
    if (!request.eligible || request.mapKillCount >= 200 || request.mapKillCount < 0 ||
        !finite(request.origin) || !finite(request.direction) ||
        !std::isfinite(request.intensity) || request.pieces == 0 ||
        seenEvent(request.eventId)) return 0;

    const Vec3 forward = unit(request.direction, {1,0,0});
    const Vec3 side = unit({-forward.y, forward.x, 0}, {0,1,0});
    const Vec3 lift = unit({-forward.x*forward.z, -forward.y*forward.z,
                            forward.x*forward.x+forward.y*forward.y}, {0,0,1});
    const Material m = material(request.kind);
    const float intensity = std::clamp(request.intensity, 0.25f, 2.0f);
    std::uint32_t rng = request.cosmeticSeed ? request.cosmeticSeed : 0x9e3779b9u;
    int added = 0;
    const int count = std::min<int>(MaxPiecesPerBurst, request.pieces);
    for (int i = 0; i < count; ++i) {
        Piece *free = nullptr;
        for (auto &candidate : pieces_) if (!candidate.used) { free = &candidate; break; }
        if (!free) { ++stats_.droppedByBudget; continue; }
        const float speed = between(rng, 95.0f, 215.0f)*intensity;
        const Vec3 spread = side * between(rng,-0.50f,0.50f) +
                            lift * between(rng,-0.18f,0.65f);
        const Vec3 direction = unit(forward+spread, forward);
        free->p = request.origin + direction * between(rng,0.0f,1.2f);
        free->v = direction*speed + Vec3{0,0,between(rng,5.0f,70.0f)};
        free->size = between(rng,m.sizeLow,m.sizeHigh)*std::sqrt(intensity);
        free->radius = std::clamp(free->size*0.3f,0.5f,3.0f);
        free->rotation = between(rng,-kPi,kPi);
        free->angularSpeed = between(rng,-11.0f,11.0f);
        free->age = 0;
        free->lifespan = m.lifetime + between(rng,-0.3f,0.5f);
        free->kind = request.kind;
        free->tint = request.tint;
        free->variant = std::uint8_t(random32(rng)%8);
        free->contacts = 0;
        free->resting = false;
        free->used = true;
        ++added; ++stats_.spawned;
    }
    if (request.eventId) {
        recentEvents_[eventCursor_] = request.eventId;
        eventCursor_ = (eventCursor_+1)%recentEvents_.size();
    }
    stats_.active = static_cast<std::uint32_t>(activeCount());
    return added;
}

void System::step(float dt, const Host &host, int &impactBudget) {
    for (Piece &p : pieces_) {
        if (!p.used) continue;
        p.age += dt;
        if (p.age >= p.lifespan) { p.used = false; continue; }
        if (p.resting) continue;
        // Semi-implicit Euler: integrate gravity before tracing the displacement.
        p.v.z -= kGravity*dt;
        const Vec3 next = p.p + p.v*dt;
        TraceHit hit{};
        ++stats_.traces;
        if (!host.trace(host.context,p.p,next,p.radius,hit)) {
            p.p = next; p.rotation += p.angularSpeed*dt; continue;
        }
        if (!finite(hit.center) || !finite(hit.normal) ||
            !std::isfinite(hit.fraction) || hit.fraction < 0 || hit.fraction > 1 ||
            hit.surface == Surface::Sky || hit.surface == Surface::Void ||
            hit.surface == Surface::Liquid) { p.used = false; continue; }

        const Vec3 normal = unit(hit.normal, {0,0,1});
        const float incoming = length(p.v);
        const float inward = dot(p.v,normal);
        p.p = hit.center + normal*0.1f;
        const Material m = material(p.kind);
        // Only apply impact callbacks to geometrically valid, explicitly markable
        // surfaces. Host applies F4 wall-blood and shared decal budgets again.
        if (hit.canMark && incoming >= 48.0f && p.contacts < 2) {
            if (impactBudget > 0 && host.impact) {
                Impact e;
                e.position = hit.center;
                e.normal = normal;
                e.kind = p.kind;
                e.tint = p.tint;
                e.surface = hit.surface;
                e.strength = std::clamp(incoming/300.0f,0.0f,1.0f);
                e.markSize = std::clamp(p.size*markScale(p.kind),2.0f,32.0f);
                e.variant = p.variant;
                e.cling = (hit.surface == Surface::Wall && p.kind == Kind::Viscera);
                host.impact(host.context,e);
                --impactBudget; ++stats_.emittedImpacts;
            } else ++stats_.suppressedImpacts;
        }
        ++p.contacts;
        // Viscera impacts a wall as a single sticky mark; the host owns the
        // mark's lasting appearance. No sprite decal is directly generated here.
        if (p.kind == Kind::Viscera && hit.surface == Surface::Wall) {
            p.used = false; continue;
        }
        if (p.contacts >= 4) {
            if (hit.surface == Surface::Floor) { p.resting = true; p.v = {}; }
            else p.used = false;
            continue;
        }
        if (inward < 0) {
            const Vec3 nvel = normal * (-inward*m.bounce);
            const Vec3 tvel = (p.v-normal*inward)*m.tangentialDrag;
            p.v = nvel + tvel;
        } else p.v = p.v*m.tangentialDrag;
        if ((hit.surface == Surface::Floor || hit.surface == Surface::Ceiling) &&
            length(p.v) <= m.minRestSpeed) {
            p.resting = true; p.v = {};
        } else p.rotation += p.angularSpeed*dt;
    }
}

void System::advance(float seconds, const Host &host) {
    if (!std::isfinite(seconds) || seconds <= 0) return;
    if (!host.trace) return; // fail closed: no known collision geometry
    accumulator_ += std::min(seconds,0.12f);
    int impactBudget = MaxImpactEventsPerAdvance;
    int iterations = 0;
    while (accumulator_ >= kStep && iterations < 8) {
        step(kStep,host,impactBudget);
        accumulator_ -= kStep;
        ++iterations;
    }
    // Never perform a catch-up burst after a stalled/paused frame.
    if (iterations == 8) accumulator_ = 0;
    stats_.active = static_cast<std::uint32_t>(activeCount());
}

void System::forEachVisual(VisualFn draw, void *context) const {
    if (!draw) return;
    for (const Piece &p : pieces_) if (p.used) {
        draw(context, {p.p,p.kind,p.tint,p.size,p.rotation,
                       std::clamp(p.age/p.lifespan,0.0f,1.0f),p.variant,p.resting});
    }
}

void System::clear() {
    pieces_ = {};
    recentEvents_ = {};
    eventCursor_ = 0;
    accumulator_ = 0;
    stats_ = {};
}

std::size_t System::activeCount() const {
    std::size_t total = 0;
    for (const Piece &p : pieces_) if (p.used) ++total;
    return total;
}

} // namespace hellforge::giblets

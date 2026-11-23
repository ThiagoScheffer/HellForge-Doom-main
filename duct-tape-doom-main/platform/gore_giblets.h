#pragma once

// HellForge original cosmetic giblet simulation foundation, C++17.
// Standalone: no Doom/GZDoom engine dependencies and no external assets.
// Integration: Gore resolver -> emit(); tick -> advance(); renderer -> forEachVisual();
// Existing HellForge blood/decal code remains the only owner of world marks.

#include <array>
#include <cstddef>
#include <cstdint>

namespace hellforge::giblets {

struct Vec3 { float x = 0, y = 0, z = 0; };

enum class Kind : std::uint8_t { Flesh, HeavyTissue, Viscera, Bone };
enum class Tint : std::uint8_t { Red, Blue, Green, PaleBone };
enum class Surface : std::uint8_t { Wall, Floor, Ceiling, Liquid, Sky, Void };

// The host performs a swept collision test for a small sphere travelling
// from 'start' to 'finish'. The result is the sphere CENTER at first contact;
// fraction is in [0, 1]; normal points away from the surface. A false result
// means the trajectory is unobstructed. The host must reject sky/invalid
// geometry and must not return a gameplay mobj collision.
struct TraceHit {
    Vec3 center{};
    Vec3 normal{};
    float fraction = 1.0f;
    Surface surface = Surface::Void;
    bool canMark = false; // valid textured wall / solid non-liquid floor / ceiling
};

struct Impact {
    Vec3 position{};
    Vec3 normal{};
    Kind kind = Kind::Flesh;
    Tint tint = Tint::Red;
    Surface surface = Surface::Floor;
    float strength = 0; // visual-only [0, 1]
    float markSize = 0; // suggested Doom units, not a guaranteed decal
    std::uint8_t variant = 0;
    bool cling = false; // suggests a thick smear on a wall; does not create geometry
};

using TraceFn = bool (*)(void *context, Vec3 start, Vec3 finish,
                         float radius, TraceHit &out);
using ImpactFn = void (*)(void *context, const Impact &event);

struct Host {
    void *context = nullptr;
    TraceFn trace = nullptr;
    ImpactFn impact = nullptr;
};

struct Burst {
    Vec3 origin{};
    Vec3 direction{1, 0, 0}; // along the shot's outgoing direction
    Kind kind = Kind::Flesh;
    Tint tint = Tint::Red;
    float intensity = 1.0f; // bounded [0.25, 2.0]
    std::uint8_t pieces = 6; // capped to 12 per burst
    std::uint32_t cosmeticSeed = 1; // cosmetic RNG only
    std::uint64_t eventId = 0; // nonzero: unique shot/victim token for deduplication
    int mapKillCount = 0; // strict '< 200' HellForge policy
    bool eligible = false; // existing gore/F4/demo/monster policy approves this event
};

struct Visual {
    Vec3 center{};
    Kind kind = Kind::Flesh;
    Tint tint = Tint::Red;
    float size = 1;
    float rotation = 0;
    float age01 = 0;
    std::uint8_t variant = 0; // placeholder visual identity for future custom atlas
    bool resting = false;
};

using VisualFn = void (*)(void *context, const Visual &visual);

struct Stats {
    std::uint32_t spawned = 0;
    std::uint32_t droppedByBudget = 0;
    std::uint32_t emittedImpacts = 0;
    std::uint32_t suppressedImpacts = 0;
    std::uint32_t traces = 0;
    std::uint32_t active = 0;
};

// Small, bounded, deliberately non-gameplay particle system.
class System final {
public:
    static constexpr std::size_t MaxPieces = 64;
    static constexpr int MaxPiecesPerBurst = 12;
    static constexpr int MaxImpactEventsPerAdvance = 12;

    System() = default;
    System(const System &) = delete;
    System &operator=(const System &) = delete;

    // Returns number of spawned visual pieces. Existing Doom actor state is
    // never changed. Renderer must supply its own visual atlas or procedural mesh.
    int emit(const Burst &request);

    // Seconds from game/render time; internally fixed-step (70 Hz), bounded.
    // Without a collision service, advances nothing rather than tunneling.
    void advance(float seconds, const Host &host);

    void forEachVisual(VisualFn draw, void *context) const;
    void clear(); // level change, shutdown, demo/replay transition
    std::size_t activeCount() const;
    const Stats &stats() const { return stats_; }

private:
    struct Piece {
        Vec3 p{}, v{};
        Kind kind = Kind::Flesh;
        Tint tint = Tint::Red;
        float radius = 1, size = 1;
        float rotation = 0, angularSpeed = 0;
        float age = 0, lifespan = 0;
        std::uint8_t variant = 0, contacts = 0;
        bool used = false, resting = false;
    };
    std::array<Piece, MaxPieces> pieces_{};
    std::array<std::uint64_t, 32> recentEvents_{};
    std::size_t eventCursor_ = 0;
    float accumulator_ = 0;
    Stats stats_{};

    bool seenEvent(std::uint64_t id) const;
    void step(float dt, const Host &host, int &impactBudget);
};

} // namespace hellforge::giblets

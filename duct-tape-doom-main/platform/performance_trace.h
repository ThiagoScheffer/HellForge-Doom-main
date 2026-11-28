#ifndef DOOM_PERFORMANCE_TRACE_H
#define DOOM_PERFORMANCE_TRACE_H

#include <array>
#include <cstdint>
#include <string>

namespace doom3d {
namespace performance {

constexpr unsigned lightKinds = 8;
constexpr unsigned maxTraceLights = 63;
enum Pass : unsigned {
    LiquidReflection = 1, GlassReflection = 2, GlassProbes = 4,
    MistDepth = 8, MainView = 16, Bloom = 32, Mist = 64
};
enum State : unsigned {
    Panel = 1, Menu = 2, Paused = 4, Unfocused = 8, NoWorld = 16,
    Loading = 32, Targets = 64, Screenshot = 128, NoSwapchain = 256
};
enum Phase { Relight, Camera, Geometry, Effects, Masks, Packing, PreparePhases };

struct Frame {
    uint64_t serial = 0, time = 0;
    unsigned level = 0, revision = 0, segment = 0;
    int gameTic = 0, levelTic = 0;
    char map[16]{};
    float interval = -1, outside = -1, loading = 0, acquire = -1, prepare = -1;
    float upload = -1, encode = -1, submit = -1, targets = 0;
    std::array<float, PreparePhases> preparation{{-1,-1,-1,-1,-1,-1}};
    bool hasView = false;
    float player[3]{}, eye[3]{}, yaw = 0, pitch = 0, fovX = 0, fovY = 0;
    int playerSector = -1, cameraSector = -1, subsector = -1;
    char floor[9]{}, ceiling[9]{};
    float floorHeight = 0, ceilingHeight = 0;
    int width = 0, height = 0, outputWidth = 0, outputHeight = 0, samples = 0;
    unsigned featureFlags = 0, passes = 0, state = 0, probes = 0;
    std::array<unsigned, lightKinds> lights{};
    unsigned blockers = 0, mistVertices = 0, fogLights = 0, particleVertices = 0;
    float slicedBlockers = 0;
    int triangles = -1, subsectors = -1, rejectedSubsectors = -1, lines = -1, rejectedLines = -1;
};

struct Light {
    unsigned category = 0;
    float x = 0, y = 0, z = 0, radius = 0, strength = 0, baked = 0, slicedBlockers = 0;
    unsigned blockers = 0;
    int actorType = -1;
};
struct Config {
    int target = 60, mode = 2, duration = 500;
    bool detailed = true, autoExport = true;
};

void configure(bool enabled, Config config, unsigned revision, std::string settings);
void identify(const char *backend, const char *gpu, const char *presentation, int vsync);
void files(const char *iwad, std::string addons);
void submit(Frame &frame, bool fromSave);
bool wantsLights(const Frame &frame);
void lights(const Frame &frame, const Light *values, unsigned count);
void boundary(uint64_t timestamp, const char *reason);
bool exportReport();
bool openFolder();
void shutdown();

} // namespace performance
void exportPerformanceReport();
void openPerformanceFolder();
} // namespace doom3d
#endif

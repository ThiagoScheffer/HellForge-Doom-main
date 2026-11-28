#ifndef DOOM_PERFORMANCE_TRACE_H
#define DOOM_PERFORMANCE_TRACE_H

#include <array>
#include <cstdint>
#include <string>

namespace doom3d {
namespace performance {

constexpr unsigned lightKinds = 8;
constexpr unsigned maxTraceLights = 63;
constexpr unsigned maxMaskJobs = 8192;
enum Pass : unsigned {
    LiquidReflection = 1, GlassReflection = 2, GlassProbes = 4,
    MistDepth = 8, MainView = 16, Bloom = 32, Mist = 64
};
enum State : unsigned {
    Panel = 1, Menu = 2, Paused = 4, Unfocused = 8, NoWorld = 16,
    Loading = 32, Targets = 64, Screenshot = 128, NoSwapchain = 256
};
enum Phase { Relight, Camera, Geometry, Effects, Masks, Packing, PreparePhases };

enum CullReason : unsigned {
    CullForcedOff=1, CullLiquid=2, CullGlassSetting=4, CullGlassCapture=8, CullBakes=16, CullLoading=32,
    CullDigging=64, CullDigMesh=128, CullCuts=256, CullFloors=512, CullWalls=1024
};
struct Visibility {
    unsigned reasons=0;
    int xy=-1, spatial=-1;
    bool valid=false,enabled=false,solid=false,shared=false;
    int floors=0,ceilings=0,walls=0,sky=0,rejectedFloors=0,rejectedCeilings=0,rejectedWalls=0;
};
struct MaskWork {
    uint32_t processed=0,lit=0,pairs=0,surface=0,decoration=0,maximum=0;
    void add(const MaskWork &b) {
        processed+=b.processed;lit+=b.lit;pairs+=b.pairs;surface+=b.surface;decoration+=b.decoration;
        if(b.maximum>maximum)maximum=b.maximum;
    }
    void account(const unsigned *bits,const unsigned *surfaceBits,const unsigned *decorationBits) {
        auto pop=[](uint32_t x) {
            x-= (x>>1)&0x55555555u;
            x=(x&0x33333333u)+((x>>2)&0x33333333u);
            return ((x+(x>>4))&0x0f0f0f0fu)*0x01010101u>>24;
        };
        unsigned n=pop(bits[0])+pop(bits[1]);
        ++processed;lit+=n!=0;pairs+=n;
        surface+=pop(bits[0]&surfaceBits[0])+pop(bits[1]&surfaceBits[1]);
        decoration+=pop(bits[0]&decorationBits[0])+pop(bits[1]&decorationBits[1]);
        if(n>maximum)maximum=n;
    }
};
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
    Visibility visibility;
    bool maskAvailable=false;
    float traceCollection=-1,traceSnapshot=-1;
    std::array<MaskWork,2> masks{}; // main and mirror populations
    int triangles = -1, subsectors = -1, rejectedSubsectors = -1, lines = -1, rejectedLines = -1;
};

struct Light {
    unsigned category = 0;
    float x = 0, y = 0, z = 0, radius = 0, strength = 0, baked = 0, slicedBlockers = 0;
    unsigned blockers = 0;
    int actorType = -1;
    int assignedTriangles=-1;
    int originSector=-1,originLine=-1,originSide=-1;
    unsigned contributors=0;
    char material[9]{};

};
struct MaterialBinding {
    char material[9]{},next[9]{};
    float blend=-1;
    unsigned livingProfile=0;
    bool loaded=false,emissive=false,relief=false,liquid=false;
};
struct FocusedSurface {
    MaterialBinding binding;
    char source[9]{};

    int kind=0,sector=-1,subsector=-1,line=-1,side=-1;
    float distance=0,position[3]{},normal[3]{};
};
struct MaterialWork {
    MaterialBinding binding;
    bool flat=false;
    MaskWork masks;
};
struct Evidence {
    unsigned count=0,materialCount=0;
    bool attributionAttempted=false,maskSampleAvailable=false;
    std::array<uint32_t,maxTraceLights+1> maskHistogram{};
    std::array<Light,maxTraceLights> lights{};
    std::array<MaterialWork,16> materials{};
    FocusedSurface focus;
};
struct Config {
    int target = 60, mode = 2, duration = 500;
    bool detailed = true, autoExport = true;
};

void configure(bool enabled, Config config, unsigned revision, std::string settings);
void identify(const char *backend, const char *gpu, const char *presentation, int vsync);
void files(const char *iwad, std::string addons);
void submit(Frame &frame, bool fromSave, const Evidence *evidence=nullptr);
bool detailed();
bool wantsAttribution(const Frame &frame);
void boundary(uint64_t timestamp, const char *reason);
bool exportReport();
bool openFolder();
void shutdown();

} // namespace performance
void exportPerformanceReport();
void openPerformanceFolder();
} // namespace doom3d
#endif

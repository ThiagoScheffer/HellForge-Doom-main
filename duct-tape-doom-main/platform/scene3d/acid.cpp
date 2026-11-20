/* Procedural floor decals for the independent corrosive-barrel system. */
#include "internal.h"

namespace doom3d {
namespace {
constexpr int acidTextureKey = -0x30000000;
constexpr int acidTextureSize = 64;

Image &acidImage() {
    auto found = images.find(acidTextureKey);
    if (found != images.end()) return found->second;

    std::vector<byte> pixels((size_t)acidTextureSize * acidTextureSize * 2, 0);
    const uint32_t seed = 0x6a09e667u;
    for (int y = 0; y < acidTextureSize; ++y) {
        for (int x = 0; x < acidTextureSize; ++x) {
            const float dx = (x + 0.5f - acidTextureSize * 0.5f) / (acidTextureSize * 0.5f);
            const float dy = (y + 0.5f - acidTextureSize * 0.5f) / (acidTextureSize * 0.5f);
            const float angle = std::atan2(dy, dx);
            const float edge = 0.79f + 0.075f * std::sin(angle * 3.0f + 0.7f) +
                               0.045f * std::sin(angle * 7.0f + 1.8f) +
                               0.035f * (hashUnit(x, y, seed) - 0.5f);
            const float radius = std::hypot(dx, dy);
            const float noise = hashUnit(x * 3, y * 5, seed ^ 0x9e3779b9u);
            const bool body = radius < edge && !(radius > edge * 0.72f && noise < 0.075f);
            const size_t at = ((size_t)y * acidTextureSize + x) * 2;
            pixels[at] = (byte)(48 + (hash32((uint32_t)x * 73856093u ^
                                               (uint32_t)y * 19349663u ^ seed) % 180));
            pixels[at + 1] = body ? 255 : 0;
        }
    }
    return images.emplace(acidTextureKey,
                          upload(acidTextureSize, acidTextureSize, pixels.data())).first->second;
}

void flatAcid(int sectorIndex, float cx, float cy, float radius, float age,
              float fade, uint32_t seed, const Uniforms &camera) {
    if (sectorIndex < 0 || sectorIndex >= numsectors || radius < 1.0f) return;
    const sector_t &sector = sectors[sectorIndex];
    if (sector.floorpic == skyflatnum || sectorIndex >= (int)sectorFloors.size()) return;

    const Image &image = acidImage();
    const float angle = (hashUnit((int)seed, (int)(seed >> 8), seed ^ 0x85ebca6bu) - 0.5f) * 0.8f;
    const float c = std::cos(angle), s = std::sin(angle);
    const float h = radius, w = radius * 1.0f;
    const float reach = std::hypot(w, h);
    const Point across={c,s}, along={-s,c};
    auto corner=[&](float u,float v) {
        return Point{cx+across.x*u+along.x*v,cy+across.y*u+along.y*v};
    };
    std::vector<Point> square={corner(-w,-h),corner(w,-h),corner(w,h),corner(-w,h)};
    const float z=floorZ(&sector)+decalLift(cx,cy,camera);
    const float light=lighting(&sector);
    const unsigned mode=MODE_ACID|sectorMode(&sector);
    auto &out=batches[acidTextureKey];

    for (int leaf : sectorFloors[sectorIndex]) {
        if (leaf < 0 || leaf >= (int)floors.size() || leaf >= (int)floorBounds.size()) continue;
        const auto &bounds=floorBounds[leaf];
        if (bounds[2] < cx-reach || bounds[0] > cx+reach ||
            bounds[3] < cy-reach || bounds[1] > cy+reach) continue;
        auto polygon=floors[leaf];
        for (size_t n=0;n<square.size()&&!polygon.empty();++n) {
            const Point a=square[n],d=square[(n+1)%square.size()];
            polygon=clip(polygon,a,{d.x-a.x,d.y-a.y},false);
        }
        auto vertex=[&](Point p) {
            const float u=((p.x-cx)*across.x+(p.y-cy)*across.y+w)/(2*w);
            const float v=(h-((p.x-cx)*along.x+(p.y-cy)*along.y))/(2*h);
            /* red carries the remaining-life fade for the acid shader. */
            return Vertex{p.x,p.y,z,u*image.width,v*image.height,light,mode,fade,0,age};
        };
        for (size_t n=1;n+1<polygon.size();++n) {
            out.push_back(vertex(polygon[0]));
            out.push_back(vertex(polygon[n]));
            out.push_back(vertex(polygon[n+1]));
        }
    }
}
} // namespace

static void leakDrops(const p_acid_pool_view_t &pool, float age, float fade,
                      const Uniforms &camera) {
    if (!pool.hasOwner || pool.sector < 0 || pool.sector >= numsectors) return;
    const float x=pool.ownerX/(float)FRACUNIT;
    const float y=pool.ownerY/(float)FRACUNIT;
    const float baseZ=(pool.ownerZ+pool.ownerHeight*0.42f)/(float)FRACUNIT;
    const float right[3]={camera.right[0],camera.right[1],camera.right[2]};
    const float up[3]={camera.up[0],camera.up[1],camera.up[2]};
    const sector_t &sector=sectors[pool.sector];
    const float light=lighting(&sector);
    const unsigned mode=MODE_ACID|sectorMode(&sector);
    auto vertex=[&](float cx,float cy,float cz,float rx,float uy,float u,float v) {
        return Vertex{cx+right[0]*rx+up[0]*uy,
                      cy+right[1]*rx+up[1]*uy,
                      cz+right[2]*rx+up[2]*uy,
                      u,v,light,mode,fade,0,age};
    };
    auto &out=batches[acidTextureKey];
    for(int n=0;n<2;++n) {
        float phase=std::fmod(age*2.5f+n*0.47f,1.0f);
        float cx=x+right[0]*(n?1.4f:-1.1f);
        float cy=y+right[1]*(n?1.4f:-1.1f);
        float cz=baseZ-phase*8.0f;
        const float w=0.75f,h=1.35f;
        const float u0=27,u1=37,v0=25,v1=39;
        quad(out,vertex(cx,cy,cz,-w,h,u0,v0),vertex(cx,cy,cz,w,h,u1,v0),
             vertex(cx,cy,cz,w,-h,u1,v1),vertex(cx,cy,cz,-w,-h,u0,v1));
    }
}

void buildAcid(const Uniforms &camera) {
    if (!P_AcidEnabled() || gamestate != GS_LEVEL || floors.empty()) return;
    const int count=P_AcidPoolCount();
    for (int i=0;i<count;++i) {
        p_acid_pool_view_t pool{};
        if (!P_AcidPoolGet(i,&pool) || pool.sector<0 || pool.sector>=numsectors) continue;
        const float age=pool.age/(float)TICRATE;
        const float radius=pool.radius/(float)FRACUNIT;
        const float grow=pool.kind==P_ACID_POOL_LEAK
            ? 0.68f+0.32f*std::min(1.0f,pool.age/24.0f)
            : 0.55f+0.45f*std::min(1.0f,pool.age/14.0f);
        const float fade=std::clamp(pool.remaining/(float)(TICRATE*2),0.28f,1.0f);
        const float x=pool.x/(float)FRACUNIT,y=pool.y/(float)FRACUNIT;
        const uint32_t seed=hash32((uint32_t)pool.x ^ ((uint32_t)pool.y<<1) ^
                                   (uint32_t)pool.sector*0x9e3779b9u);
        flatAcid(pool.sector,x,y,radius*grow,age,fade,seed,camera);

        /* A damaged barrel leaves several small beads around its growing leak. */
        if (pool.kind==P_ACID_POOL_LEAK) {
            const float phase=age*2.1f+(seed&1023u)*0.0061f;
            for (int n=0;n<3;++n) {
                const float a=phase+n*2.0943951f;
                const float d=std::max(4.0f,radius*0.52f);
                flatAcid(pool.sector,x+std::cos(a)*d,y+std::sin(a)*d,
                         2.6f+0.5f*std::sin(phase+n),age,fade,seed+n*31u,camera);
            }
            leakDrops(pool,age,fade,camera);
        }
    }
}
} // namespace doom3d

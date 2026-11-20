/* Procedural floor decals for the independent corrosive-barrel system. */
#include "internal.h"

namespace doom3d {
namespace {
constexpr int acidTextureKey = -0x30000000;
constexpr int acidBatch = -1-acidTextureKey;
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
            const float edge = 0.89f + 0.050f * std::sin(angle * 3.0f + 0.7f) +
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
    const std::array<Point,4> square={corner(-w,-h),corner(w,-h),corner(w,h),corner(-w,h)};
    const float z=floorZ(&sector)+decalLift(cx,cy,camera);
    const float light=.55f+.45f*lighting(&sector);
    auto &out=effectBatches[acidBatch];

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
            const float ripple=.5f+.5f*std::sin(age*5.f+p.x*.075f+p.y*.051f);
            return Vertex{p.x,p.y,z,u*image.width,v*image.height,fade*.92f,0u,
                          .055f*light,(.39f+.20f*ripple)*light,.035f*light};
        };
        for (size_t n=1;n+1<polygon.size();++n) {
            out.push_back(vertex(polygon[0]));
            out.push_back(vertex(polygon[n]));
            out.push_back(vertex(polygon[n+1]));
        }
    }
}
} // namespace

static void acidDrop(const Uniforms &camera,float x,float y,float z,float w,float h,float opacity) {
    auto vertex=[&](float rx,float uy,float u,float v) {
        return Vertex{x+camera.right[0]*rx+camera.up[0]*uy,
                      y+camera.right[1]*rx+camera.up[1]*uy,
                      z+camera.right[2]*rx+camera.up[2]*uy,
                      u,v,opacity,0u,.17f,.72f,.055f};
    };
    quad(effectBatches[acidBatch],vertex(-w,h,0,0),vertex(w,h,64,0),
         vertex(w,-h,64,64),vertex(-w,-h,0,64));
}

static void leakDrops(const p_acid_pool_view_t &pool,float age,float fade,const Uniforms &camera) {
    if(!pool.hasOwner)return;
    const float x=units(pool.ownerX),y=units(pool.ownerY);
    const float floor=floorZ(&sectors[pool.sector]);
    const float top=std::max(floor,units(pool.ownerZ)+units(pool.ownerHeight)*.42f);
    const int drops=2+std::clamp((pool.radius/FRACUNIT-12)/6,0,3);
    for(int n=0;n<drops;++n) {
        const float phase=std::fmod(age*(2.1f+.17f*drops)+n*.27f,1.f);
        const float z=std::max(floor+1.f,top-(top-floor)*phase*phase);
        acidDrop(camera,x+std::cos(n*2.1f)*2.5f,y+std::sin(n*2.1f)*2.5f,
                 z,.75f,1.25f+phase,fade);
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
        const float fade=std::clamp(pool.remaining/(float)(TICRATE*2),0.f,1.f);
        const float x=pool.x/(float)FRACUNIT,y=pool.y/(float)FRACUNIT;
        const uint32_t seed=hash32((uint32_t)pool.x ^ ((uint32_t)pool.y<<1) ^
                                   (uint32_t)pool.sector*0x9e3779b9u);
        flatAcid(pool.sector,x,y,radius,age,fade,seed,camera);
        const float floor=floorZ(&sectors[pool.sector]);
        // Bubbles rise only on the owning floor. Breakout droplets follow
        // the already validated pool centers and stay below solid ceilings.
        for(int n=0;n<3;++n) {
            const float phase=std::fmod(age*(1.f+n*.12f)+hashUnit(n,17,seed),1.f);
            if(phase>.65f)continue;
            const float angle=hashUnit(n,18,seed)*2.f*doomPi;
            const float distance=radius*.50f*hashUnit(n,19,seed);
            const float bx=x+std::cos(angle)*distance,by=y+std::sin(angle)*distance;
            const sector_t *bubble=R_PointInSubsector(fixed_t(bx*FRACUNIT),fixed_t(by*FRACUNIT))->sector;
            if(bubble==&sectors[pool.sector])
                acidDrop(camera,bx,by,floor+.65f+phase*2.f,.9f,.75f,fade*(1.f-phase));
        }
        if (pool.kind==P_ACID_POOL_LEAK) {
            leakDrops(pool,age,fade,camera);
        } else if(radius>=48.f&&age<.52f) {
            for(int n=0;n<7;++n) {
                const float t=age-n*.019f;
                if(t<0.f)continue;
                const float angle=hashUnit(n,20,seed)*2.f*doomPi;
                const float distance=(24.f+42.f*hashUnit(n,21,seed))*t;
                const float px=x+std::cos(angle)*distance,py=y+std::sin(angle)*distance;
                const float z=floor+9.f+t*(100.f+70.f*hashUnit(n,22,seed))-290.f*t*t;
                const auto *sub=R_PointInSubsector(fixed_t(px*FRACUNIT),fixed_t(py*FRACUNIT));
                if(sub->sector!=&sectors[pool.sector]||z<=floor||z+2.f>=ceilZ(sub->sector))continue;
                WallHit wall;
                if(distance>.01f&&firstSolidWall(x,y,floor+9.f,std::cos(angle),std::sin(angle),
                                                  (z-floor-9.f)/distance,distance,wall))continue;
                acidDrop(camera,px,py,z,1.1f,2.5f,fade*(1.f-age/.52f));
            }
        }
    }
}
} // namespace doom3d

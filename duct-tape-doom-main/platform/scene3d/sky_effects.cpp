#include "sky_effects.h"
#include "../scene3d.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>

extern "C" {
#include "doomstat.h"
}

namespace doom3d {
namespace {

// A2 artwork coordinates measured at the two rooflines. X is in the
// 1024-column panorama and may cross the original 256-column tile edge.
constexpr SkyEffectAnchor cityRoof(float column,float row,float radius) {
    return {-column*(6.283185307f/1024.f),(.70f-row/128.f)/.68f,radius};
}

constexpr SkyFireSmokeProfile profiles[]={
    {
        0x69c14db637c6ce05ULL,0x43701a484700347fULL,256,128,true,63.f,.31f,
        {.044f,.031f,.026f},{.67f,.256f,.087f},
        {.77f,.219f,.027f},{1.00f,.645f,.238f},
        {{
            {cityRoof(-51.f,83.f,114000.f),SkyEffectDepthSlot::BehindMiddle,
             {.075f,.137f,.80f,1.36f,true},
             {.218f,.69f,.91f,.84f,.88f,.32f,true},
             {.110f,.96f,3.4f},0x9bc17452u},
            {cityRoof(80.f,59.f,58500.f),SkyEffectDepthSlot::BetweenMiddleAndNear,
             {.098f,.178f,.93f,1.08f,true},
             {.285f,.81f,.94f,.68f,1.0f,.39f,true},
             {.136f,1.05f,3.1f},0x6fd8c133u},
        }},
        2
    }
};

static_assert(offsetof(SkyFireSmokeGpuSource,flame)==16 &&
              offsetof(SkyFireSmokeGpuSource,smoke)==32 &&
              offsetof(SkyFireSmokeGpuSource,motion)==48 &&
              offsetof(SkyFireSmokeGpuSource,illumination)==64 &&
              sizeof(SkyFireSmokeGpuSource)==80);
static_assert(offsetof(SkyFireSmokeGpuBlock,smokeDark)==16 &&
              offsetof(SkyFireSmokeGpuBlock,smokeWarm)==32 &&
              offsetof(SkyFireSmokeGpuBlock,flameOuter)==48 &&
              offsetof(SkyFireSmokeGpuBlock,flameCore)==64 &&
              offsetof(SkyFireSmokeGpuBlock,sources)==80 &&
              sizeof(SkyFireSmokeGpuBlock)==400);

SkyFireSmokeGpuBlock packProfile(const SkyFireSmokeProfile &profile) {
    if(profile.count==0||profile.count>MaxSkyFireSources)return {};
    SkyFireSmokeGpuBlock block{};
    auto pack=[](const std::array<float,3> &color) {
        return std::array<float,4>{color[0],color[1],color[2],0.f};
    };
    block.smokeDark=pack(profile.smokeDark);
    block.smokeWarm=pack(profile.smokeWarm);
    block.flameOuter=pack(profile.flameOuter);
    block.flameCore=pack(profile.flameCore);
    float nearest=profile.sources[0].anchor.radius;
    float lowestRise=profile.sources[0].anchor.rise,highestRise=lowestRise;
    bool onlyOccludedMist=true;
    for(unsigned i=0;i<profile.count;++i) {
        const auto &source=profile.sources[i];
        const auto &anchor=source.anchor;
        const auto &flame=source.flame;
        const auto &smoke=source.smoke;
        const auto &light=source.illumination;
        if(!std::isfinite(anchor.azimuth)||!std::isfinite(anchor.rise)||
           !std::isfinite(anchor.radius)||anchor.radius<=0.f||static_cast<unsigned>(source.slot)>3||
           (flame.enabled&&!(flame.width>0.f&&flame.height>0.f&&flame.intensity>0.f))||
           (smoke.enabled&&!(smoke.width>0.f&&smoke.height>0.f&&smoke.density>0.f))||
           (light.strength>0.f&&!(light.radius>0.f&&light.verticalFalloff>0.f)))return {};
        nearest=std::min(nearest,anchor.radius);
        onlyOccludedMist &= smoke.enabled&&smoke.valleyMist&&
            !flame.enabled&&light.strength<=0.f&&source.slot!=SkyEffectDepthSlot::InFrontOfNear;
        lowestRise=std::min(lowestRise,anchor.rise);
        highestRise=std::max(highestRise,anchor.rise+smoke.height);
        auto &gpu=block.sources[i];
        gpu.anchor={anchor.azimuth,anchor.rise,anchor.radius,static_cast<float>(source.slot)};
        gpu.flame={flame.width,flame.height,flame.enabled?flame.intensity:0.f,flame.flickerRate};
        gpu.smoke={smoke.width,smoke.height,smoke.enabled?smoke.density:0.f,smoke.riseSpeed};
        const uint32_t phase=(source.seed^(source.seed>>16))*0x7feb352du;
        const float smokeReach=smoke.valleyMist?1.20f:
            1.32f+.74f*std::abs(smoke.windResponse)*profile.windSpeed/63.f+.45f*smoke.turbulence;
        const float reach=std::max({flame.enabled?flame.width:0.f,
            smoke.enabled?smoke.width*smokeReach:0.f,
            light.strength>0.f?1.12f*light.radius:0.f});
        gpu.motion={smoke.windResponse,smoke.valleyMist?-1.f:smoke.turbulence,float(phase>>8)/16777216.f,reach};
        const float top=std::max({gpu.flame[1],gpu.smoke[1],2.f*light.radius});
        gpu.illumination={light.radius,light.strength,light.verticalFalloff,top};
    }
    // Each depth slot is resolved back to front even if a later profile
    // lists two sources in that slot in a different order.
    std::sort(block.sources.begin(),block.sources.begin()+profile.count,
              [](const auto &a,const auto &b){return a.anchor[2]>b.anchor[2];});
    if(onlyOccludedMist) {
        // Include the maximum vertical and horizontal displacement of the
        // shared bounded eye before rejecting rays outside the mist band.
        const float pad=.035f+.12f*std::max(std::abs(lowestRise),std::abs(highestRise));
        block.smokeDark[3]=lowestRise-pad;
        block.smokeWarm[3]=highestRise+pad;
    }
    block.header={float(profile.count),nearest,profile.windSpeed,profile.hazeBlend};
    return block;
}

const auto &preparedProfiles() {
    static const auto blocks=[] {
        std::array<SkyFireSmokeGpuBlock,std::size(profiles)> result{};
        for(std::size_t index=0;index<result.size();++index) {
            const auto &profile=profiles[index];
            const auto &prepared=result[index]=packProfile(profile);
            if(devparm) {
                std::fprintf(stderr,"Sky A3: artwork %016llx, palette %016llx, %s, %u sources.\n",
                             (unsigned long long)profile.artwork,(unsigned long long)profile.palette,
                             prepared.header[0]>0.f?"valid":"invalid",unsigned(prepared.header[0]));
                for(unsigned i=0;i<unsigned(prepared.header[0]);++i) {
                    const auto &source=profile.sources[i];
                    std::fprintf(stderr,"Sky A3: source %u azimuth %.4f rise %.4f radius %.0f slot %u flame %s smoke %s.\n",
                                 i+1,source.anchor.azimuth,source.anchor.rise,source.anchor.radius,
                                 unsigned(source.slot),source.flame.enabled?"on":"off",source.smoke.enabled?"on":"off");
                }
            }
        }
        return result;
    }();
    return blocks;
}

} // namespace

SkyFireSmokeGpuBlock prepareSkyFireSmoke(const SkyFireSmokeProfile &profile) {
    return packProfile(profile);
}

const SkyFireSmokeGpuBlock *skyFireSmokeData(const Image &sky,bool hasScenery) {
    for(std::size_t i=0;i<std::size(profiles);++i) {
        const auto &profile=profiles[i];
        if(sky.skyFingerprint!=profile.artwork||sky.skyPalette!=profile.palette||
           sky.width!=profile.width||sky.height!=profile.height||
           (profile.requiresScenery&&!hasScenery))continue;
        const auto &gpu=preparedProfiles()[i];
        return gpu.header[0]>0.f?&gpu:nullptr;
    }
    return nullptr;
}

} // namespace doom3d

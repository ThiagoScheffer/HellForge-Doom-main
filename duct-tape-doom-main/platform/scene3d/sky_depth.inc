// Included by images.cpp. Derived sky layers persist across maps in the same
// renderer session. Only recognized artwork can occupy a cache slot.
struct SkyDepthCacheEntry {
    bool attempted=false;
    GpuTextureRef texture;
    SkyDepthScene scene;
};
static std::array<SkyDepthCacheEntry,(size_t)SkyArtwork::Count> skyDepthCache;
struct SkyAtmosphereCacheEntry {
    SkyAtmosphereProfile profile;
    SkyPaintedScenery scenery;
};
// Only validated 256x128 composites using stock PLAYPAL enter this cache.
// Distinct composites of the same sky fit the same tuning without sharing masks.
static std::unordered_map<uint64_t,SkyAtmosphereCacheEntry> skyAtmosphereCache;
std::array<float,3> skyDepthOrigin{};
static constexpr uint64_t stockSkyPalette=0x43701a484700347fULL;

static uint64_t skyFingerprint(const byte *pixels,size_t count) {
    uint64_t hash=14695981039346656037ULL;
    for(size_t i=0;i<count;++i)hash=(hash^pixels[i])*1099511628211ULL;
    return hash;
}

static SkyArtwork classifySkyArtwork(Image &image) {
    if(image.width!=256||image.height!=128)return SkyArtwork::Unknown;
    image.skyFingerprint=skyFingerprint(image.pixels.data(),image.pixels.size());
    switch(image.skyFingerprint) {
        case 0x4903f3c6668a384eULL: // Registered Doom's SKY1 composite starts eight rows early.
        case 0xbc9519b06539ef69ULL: return SkyArtwork::ColdMountains;
        case 0xb451911c760f176eULL: return SkyArtwork::RedMountains;
        case 0x43e2c7f2a7d8837cULL: return SkyArtwork::Inferno;
        case 0xef99d3d0fc56c3abULL: return SkyArtwork::OrangeClouds;
        case 0x85864a7716794480ULL: return SkyArtwork::BrownClouds;
        case 0x69c14db637c6ce05ULL: return SkyArtwork::BurningCity;
        case 0xe75fd27cf7bfebc8ULL: return SkyArtwork::BloodHell;
    }
    return SkyArtwork::Unknown;
}

// A profile scores colors for a boundary search, never defines a mask.
// Hell rock and red sky can both be bright; red concentration and neighboring
// above/below agreement distinguish them. The city weights dark opaque forms
// heavily but lets the integrated boundary bridge lit windows in those forms.
static float foregroundEvidence(SkyArtwork art,const byte *rgb) {
    const float r=rgb[0],g=rgb[1],b=rgb[2],lum=.299f*r+.587f*g+.114f*b;
    float evidence=0;
    switch(art) {
        case SkyArtwork::ColdMountains: evidence=(168-lum)/65; break;
        case SkyArtwork::RedMountains: evidence=(g+.5f*b-.38f*r+8)/32; break;
        case SkyArtwork::Inferno: evidence=(105-r)/70; break;
        case SkyArtwork::BurningCity: evidence=(37-lum)/32; break;
        case SkyArtwork::BloodHell: evidence=(g+.25f*b-.39f*r+5)/48; break;
        default: break;
    }
    return std::clamp(evidence,-1.0f,1.0f);
}

static std::vector<int> findSkyline(const Image &sky,const byte *palette) {
    const int w=sky.width,h=sky.height;
    const SkyArtwork art=sky.skyArtwork;
    const bool city=art==SkyArtwork::BurningCity;
    const float slope=art==SkyArtwork::RedMountains||art==SkyArtwork::Inferno?.18f:.11f;
    const float jump=city?.86f:1.2f; // Allow actual vertical walls and cliffs.
    // Search over the region these profiles can plausibly show land.
    const int first=art==SkyArtwork::BloodHell?h*42/128:city?h*25/128:
        art==SkyArtwork::ColdMountains?h*16/128:art==SkyArtwork::Inferno?h*13/128:h*6/128;
    const int end=art==SkyArtwork::BloodHell?h*121/128:city?h*106/128:
        art==SkyArtwork::ColdMountains?h*100/128:art==SkyArtwork::Inferno?h*115/128:h*102/128;
    const int rows=end-first;
    std::vector<float> scores((size_t)w*rows),support(scores.size()),prefix(h+1);
    for(int x=0;x<w;++x) {
        for(int y=0;y<h;++y)prefix[y+1]=prefix[y]+foregroundEvidence(art,palette+3*sky.pixels[2*((size_t)y*w+x)]);
        auto mean=[&](int a,int b){return (prefix[b]-prefix[a])/(b-a);};
        for(int y=first;y<end;++y) {
            const float below=mean(y,std::min(y+10,h));
            const float contrast=below-mean(y-6,y);
            const float edge=mean(y,y+2)-mean(y-2,y);
            const float bottom=mean(y,h)-mean(0,y);
            const size_t cell=(size_t)x*rows+y-first;
            support[cell]=contrast;
            scores[cell]=1.05f*contrast+.95f*edge+.45f*bottom;
            if(city)scores[cell]+=.40f*mean(y,y+2)+.30f*below+.20f*(y-first)/rows;
        }
    }
    // Find the maximum-evidence path. Trace the center of three repeats
    // so a feature crossing the texture seam is treated like any other.
    // The two passes evaluate a linear neighbor cost in O(rows) per column.
    std::vector<int> previous((size_t)w*3*rows),origin(rows),line(w);
    std::vector<float> path(rows),transition(rows);
    for(int x=0;x<3*w;++x) {
        transition=path;
        const int best=(int)(std::max_element(path.begin(),path.end())-path.begin());
        for(int i=0;i<rows;++i)origin[i]=i;
        for(int i=1;i<rows;++i)if(transition[i-1]-slope>transition[i]) {
            transition[i]=transition[i-1]-slope;
            origin[i]=origin[i-1];
        }
        for(int i=rows-2;i>=0;--i)if(transition[i+1]-slope>transition[i]) {
            transition[i]=transition[i+1]-slope;
            origin[i]=origin[i+1];
        }
        for(int i=0;i<rows;++i) {
            previous[(size_t)x*rows+i]=path[best]-jump>transition[i]?best:origin[i];
        }
        const float jumpScore=path[best]-jump;
        for(int i=0;i<rows;++i)path[i]=std::max(transition[i],jumpScore)+scores[(size_t)(x%w)*rows+i];
    }
    int row=(int)(std::max_element(path.begin(),path.end())-path.begin()),supported=0,clamped=0;
    for(int x=3*w-1;x>=w;--x) {
        if(x<2*w) {
            line[x-w]=row+first;
            supported+=support[(size_t)(x-w)*rows+row]>.16f;
            clamped+=row==0||row==rows-1;
        }
        row=previous[(size_t)x*rows+row];
    }
    if(supported<w*3/4||clamped>w/8||std::abs(line.front()-line.back())>h/4)return {};
    return line;
}

static float skyLuma(const byte *rgb) {
    return .299f*rgb[0]+.587f*rgb[1]+.114f*rgb[2];
}

static SkyAtmosphereKind atmosphereKind(const Image &sky) {
    if(sky.skyPalette!=stockSkyPalette)return SkyAtmosphereKind::Legacy;
    switch(sky.skyArtwork) {
        case SkyArtwork::BrownClouds: return SkyAtmosphereKind::DuskStorm;
        case SkyArtwork::BurningCity: return SkyAtmosphereKind::BurningCityDusk;
        case SkyArtwork::BloodHell: return SkyAtmosphereKind::InfernalAbyss;
        case SkyArtwork::ColdMountains: return SkyAtmosphereKind::DoomMountains;
        case SkyArtwork::RedMountains: return SkyAtmosphereKind::DoomDeimos;
        case SkyArtwork::Inferno: return SkyAtmosphereKind::DoomInferno;
        case SkyArtwork::OrangeClouds: return SkyAtmosphereKind::DoomOrange;
        default: return SkyAtmosphereKind::Legacy;
    }
}

const SkyAtmosphereProfile *skyAtmosphereProfile(const Image &sky) {
    if(sky.skyPalette!=stockSkyPalette)return nullptr;
    const auto cached=skyAtmosphereCache.find(sky.skyFingerprint);
    if(cached==skyAtmosphereCache.end()||cached->second.profile.kind==SkyAtmosphereKind::Legacy||
       cached->second.profile.kind!=atmosphereKind(sky))return nullptr;
    return &cached->second.profile;
}

const SkyPaintedScenery *skyPaintedScenery(const Image &sky) {
    const auto cached=skyAtmosphereCache.find(sky.skyFingerprint);
    return cached!=skyAtmosphereCache.end()&&cached->second.scenery.texture?&cached->second.scenery:nullptr;
}

static const SkyAtmosphereProfile &cacheSkyAtmosphere(const Image &sky,const byte *palette,
                                                       const byte *mask=nullptr) {
    static const SkyAtmosphereProfile invalid;
    const auto kind=atmosphereKind(sky);
    if(kind==SkyAtmosphereKind::Legacy)return invalid;
    auto [entry,added]=skyAtmosphereCache.try_emplace(sky.skyFingerprint);
    auto &cached=entry->second;
    if(!added)return cached.profile;
    cached.profile=analyzeMaskedSkyAtmosphere(sky.pixels.data(),sky.width,sky.height,palette,mask,kind);
    if(devparm&&kind==SkyAtmosphereKind::BurningCityDusk) {
        const auto &p=cached.profile;
        if(p.kind==SkyAtmosphereKind::Legacy)fprintf(stderr,"Sky A2: RSKY2 atmosphere mask rejected.\n");
        else fprintf(stderr,"Sky A2: RSKY2 %dx%d, %zu reliable samples, coverage %.4f, "
                     "zenith %.3f/%.3f/%.3f, horizon %.3f/%.3f/%.3f.\n",
                     sky.width,sky.height,(size_t)std::count(mask,mask+sky.width*sky.height,byte(255)),p.coverage,
                     p.zenith[0],p.zenith[1],p.zenith[2],p.horizon[0],p.horizon[1],p.horizon[2]);
    }
    return cached.profile;
}

static void cachePaintedSky(const Image &sky,const byte *palette,const char *name) {
    const auto kind=atmosphereKind(sky);
    if(kind<SkyAtmosphereKind::DoomMountains||skyAtmosphereCache.count(sky.skyFingerprint))return;
    const auto start=SDL_GetPerformanceCounter();
    const int w=sky.width,h=sky.height;
    if(kind==SkyAtmosphereKind::DoomOrange) {
        cacheSkyAtmosphere(sky,palette);
    } else {
        auto skyline=findSkyline(sky,palette);
        if(!skyline.empty()) {
            if(kind==SkyAtmosphereKind::DoomInferno) {
                // Include the low pillars across the valley where dark cloud
                // and lit rock use the same red ramp. Only extend the mask
                // upward in this section of the verified stock painting.
                constexpr int valley[][2]={{72,48},{84,55},{107,77},{115,58},{121,49},{129,47},{135,50},{146,70}};
                for(size_t i=1;i<std::size(valley);++i) {
                    const auto &a=valley[i-1],&b=valley[i];
                    for(int x=a[0];x<=b[0];++x)
                        skyline[x]=std::min(skyline[x],a[1]+(b[1]-a[1])*(x-a[0])/(b[0]-a[0]));
                }
            }
            std::vector<byte> mask(size_t(w)*h),layers(mask.size()*2);
            for(int x=0;x<w;++x) {
                const int ridge=skyline[x];
                byte last=0;
                for(int y=0;y<h;++y) {
                    const size_t at=size_t(y)*w+x;
                    if(sky.pixels[2*at+1]==255)last=sky.pixels[2*at];
                    mask[at]=y<ridge-2?255:0;
                    layers[2*at]=last;
                    layers[2*at+1]=byte(std::clamp(128+(y-ridge)*96,0,255));
                }
            }
            const auto &profile=cacheSkyAtmosphere(sky,palette,mask.data());
            if(profile.kind!=SkyAtmosphereKind::Legacy) {
                auto &cached=skyAtmosphereCache.at(sky.skyFingerprint);
                cached.scenery.texture=gpuCreateTexture(GpuFormat::RG8,w,h,layers.data(),w*2);
                cached.scenery.firstRow=float(*std::min_element(skyline.begin(),skyline.end())-2)/h;
                if(!cached.scenery.texture)cached.profile={};
            }
        } else skyAtmosphereCache.try_emplace(sky.skyFingerprint);
    }
    if(devparm) {
        const auto *profile=skyAtmosphereProfile(sky);
        const double ms=(SDL_GetPerformanceCounter()-start)*1000.0/SDL_GetPerformanceFrequency();
        fprintf(stderr,"Sky environment: %s %dx%d %016llx / %016llx, %s, coverage %.3f, %.2f ms.\n",
                name,w,h,(unsigned long long)sky.skyFingerprint,(unsigned long long)sky.skyPalette,
                profile?"Doom atmosphere":"legacy fallback",profile?profile->coverage:0.f,ms);
    }
}
#include "sky_depth_city.inc"
#include "sky_depth_mountains.inc"

static std::vector<byte> buildDeepSky(const Image &sky,const byte *palette,const std::vector<int> &roof,
                                      const std::vector<SkyDepthBuilding> &buildings) {
    const int w=sky.width,h=sky.height;
    const bool city=sky.skyArtwork==SkyArtwork::BurningCity;
    auto pixel=[&](int x,int y){
        return palette+3*sky.pixels[2*((size_t)std::clamp(y,0,h-1)*w+(x+w)%w)];
    };
    auto luma=[&](int x,int y){return skyLuma(pixel(x,y));};
    std::vector<float> fire(w),distance(w);
    std::vector<int> distantRoof(w,h);
    auto distantTone=[&](int x,int y) {
        const byte *p=pixel(x,y);
        const float lum=skyLuma(p);
        return p[0]-p[2]>21&&p[0]-p[1]>10&&p[0]<146&&lum>=29&&lum<104;
    };
    if(city) {
        std::vector<float> heat(w);
        std::fill(distance.begin(),distance.end(),.4f);
        for(const auto &building:buildings)
            for(int x=building.first;x<building.end;++x)distance[x%w]=building.haze;
        for(int x=0;x<w;++x) {
            for(int y=roof[x]-22;y<roof[x]+34;++y) {
                const byte *p=pixel(x,y);
                // Bright warm paint near street level or in the smoke
                // immediately above a rooftop. Exclude white cloud cores.
                const float warm=std::clamp((p[0]-p[2]-36)/106.0f,0.0f,1.0f);
                const float bright=std::clamp((luma(x,y)-65)/135,0.0f,1.0f);
                heat[x]+=warm*bright;
            }
        }
        for(int x=0;x<w;++x) {
            for(int j=-8;j<=8;++j)fire[x]+=heat[(x+j+w)%w]*(9-std::abs(j))/81;
            fire[x]=std::clamp((fire[x]-.85f)/6.4f,0.0f,1.0f);
            // A distant facade needs a warm, vertically continuous
            // body and a darker edge under brighter smoke.
            for(int y=std::max(31,roof[x]-28);y<roof[x]-4;++y) {
                if(!distantTone(x,y))continue;
                int vertical=0;
                for(int j=0;j<7;++j)vertical+=distantTone(x,y+j);
                if(vertical<4)continue;
                float above=(luma(x,y-2)+luma(x,y-1))*.5f;
                float below=(luma(x,y)+luma(x,y+1)+luma(x,y+2)+luma(x,y+3))*.25f;
                if(above-below<12)continue;
                distantRoof[x]=y;
                break;
            }
        }
        const auto candidates=distantRoof;
        for(int x=0;x<w;++x) {
            int support=0;
            for(int j=-3;j<=3;++j)
                support+=std::abs(candidates[(x+j+w)%w]-candidates[x])<=3;
            if(support<3)distantRoof[x]=h;
        }
    }
    std::vector<byte> layers((size_t)w*h*4);
    auto quantize=[](float value){return (byte)std::lround(255*std::clamp(value,0.0f,1.0f));};
    for(int y=0;y<h;++y)for(int x=0;x<w;++x) {
        const int edge=roof.empty()?-h:y-roof[x];
        const int source=edge<0?y:std::max(0,roof[x]-1-std::min(edge,h/12));
        const size_t at=4*((size_t)y*w+x);
        layers[at]=sky.pixels[2*((size_t)source*w+x)];
        layers[at+1]=(byte)std::clamp(134+edge*12,0,255);
        if(city) {
            if(edge<0) {
                // Preserve the dim painted structures already in the
                // backdrop. Only spans supported by the art receive haze.
                int support=0;
                for(int j=std::max(distantRoof[x],y-2);j<std::min(y+3,roof[x]);++j)
                    support+=distantTone(x,j);
                if(y>=distantRoof[x]&&support>=2&&(distantTone(x,y)||luma(x,y)<70))
                    layers[at+2]=quantize(.78f);
                layers[at+3]=quantize(fire[x]*std::clamp(1+edge/53.0f,0.0f,1.0f));
            } else {
                layers[at+2]=quantize(distance[x]);
                if(edge<5)continue;
                const byte *p=pixel(x,y);
                const float lit=luma(x,y);
                const float surround=.25f*(luma(x-2,y)+luma(x+2,y)+luma(x,y-2)+luma(x,y+2));
                if(lit>22&&lit<155&&p[0]>p[2]+12&&surround<57)
                    layers[at+3]=quantize((lit-surround-9)/58);
            }
        } else if(edge<0) {
            // Filter at artwork scale so procedural motion does not follow
            // individual palette pixels. Clamp vertical taps above the cut.
            float density=0;
            for(int j=-2;j<=2;++j)for(int k=-2;k<=2;++k) {
                int column=(x+k+w)%w;
                int row=roof.empty()?y+j:std::min(y+j,roof[column]-1);
                const byte *p=pixel(column,row);
                density+=sky.skyArtwork==SkyArtwork::BloodHell?(p[0]-p[2])*.65f+skyLuma(p)*.35f:skyLuma(p);
            }
            layers[at+2]=quantize(1.0f-(density/25-20)/118);
            if(!roof.empty())layers[at+2]=quantize((density/25-26)/125);
        }
    }
    return layers;
}

static const SkyDepthCacheEntry &loadSkyDepth(const Image &sky) {
    static const SkyDepthCacheEntry invalid{};
    if(sky.skyPalette!=stockSkyPalette)return invalid;
    auto &cached=skyDepthCache[(size_t)sky.skyArtwork];
    if(cached.attempted)return cached;
    cached.attempted=true;
    const int paletteLump=W_GetNumForName((char*)"PLAYPAL");
    const byte *palette=(const byte*)W_CacheLumpNum(paletteLump,PU_CACHE);
    if(const auto *profile=skyMountainProfile(sky)) {
        cached.scene=buildSkyDepthMountains(sky,palette,*profile);
        if(cached.scene.atlas)return cached;
        if(devparm)fprintf(stderr,"Sky A5: mountain profile rejected; using legacy SKY3.\n");
    }
    const bool cloudOnly=sky.skyArtwork==SkyArtwork::BrownClouds;
    const auto skyline=cloudOnly?std::vector<int>{}:findSkyline(sky,palette);
    if(!cloudOnly&&skyline.empty()) {
        if(sky.skyArtwork==SkyArtwork::BurningCity&&devparm)
            fprintf(stderr,"Sky depth: RSKY2 skyline confidence too low.\n");
        return cached;
    }

    const int w=sky.width,h=sky.height;
    const bool city=sky.skyArtwork==SkyArtwork::BurningCity;
    const auto buildings=city?segmentSkyBuildings(sky,palette,skyline):std::vector<SkyDepthBuilding>{};
    auto layers=buildDeepSky(sky,palette,skyline,buildings);
    if(city) {
        cached.scene=buildSkyDepthCity(sky,palette,skyline,buildings,layers);
        if(cached.scene.atlas)return cached;
        if(layers.size()!=(size_t)w*h*4)layers=buildDeepSky(sky,palette,skyline,buildings);
    }
    cached.texture=gpuCreateTexture(GpuFormat::RGBA8,w,h,layers.data(),w*4);
    return cached;
}

const GpuTextureRef &skyDepthTexture(const Image &sky) {
    return loadSkyDepth(sky).texture;
}

const SkyDepthScene &skyDepthScene(const Image &sky) {
    return loadSkyDepth(sky).scene;
}

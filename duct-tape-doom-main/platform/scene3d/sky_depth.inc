// Included by images.cpp. Derived sky layers persist across maps in the same
// renderer session. Only recognized artwork can occupy a cache slot.
struct SkyDepthCacheEntry {
    bool attempted=false;
    GpuTextureRef texture;
    SkyDepthScene scene;
};
static std::array<std::array<SkyDepthCacheEntry,2>,(size_t)SkyArtwork::Count> skyDepthCache;
std::array<float,3> skyDepthOrigin{};
static constexpr uint64_t stockSkyPalette=0x43701a484700347fULL;

static uint64_t skyFingerprint(const byte *pixels,size_t count) {
    uint64_t hash=14695981039346656037ULL;
    for(size_t i=0;i<count;++i)hash=(hash^pixels[i])*1099511628211ULL;
    return hash;
}

static SkyArtwork classifySkyArtwork(const Image &image,int index,const char *name) {
    if(SDL_strncasecmp(name,"SKY",3)!=0||name[3]<'1'||name[3]>'4'||name[4])return SkyArtwork::Unknown;
    if(name[3]=='4') {
        // Ultimate Doom's cloud-only sky must come from the original retail
        // data, including the composite and every patch. Verify its colors too.
        if(gamemode!=retail||!R_TextureFromBaseWad(index))return SkyArtwork::Unknown;
        int lump=W_GetNumForName((char*)"PLAYPAL");
        if(W_LumpLength(lump)<768)return SkyArtwork::Unknown;
        const byte *palette=(const byte*)W_CacheLumpNum(lump,PU_CACHE);
        size_t covered=0,warm=0;
        for(size_t i=0;i<image.pixels.size();i+=2) {
            if(image.pixels[i+1]!=255)continue;
            ++covered;
            const byte *rgb=palette+3*image.pixels[i];
            warm+=rgb[0]>80&&rgb[0]>rgb[2]+28&&rgb[0]>rgb[1]+10;
        }
        return covered>=image.pixels.size()*7/16&&warm>=covered/8?SkyArtwork::OrangeClouds:SkyArtwork::Unknown;
    }
    if(image.width!=256||image.height!=128)return SkyArtwork::Unknown;
    const uint64_t hash=skyFingerprint(image.pixels.data(),image.pixels.size());
    switch(hash) {
        case 0x4903f3c6668a384eULL: // Registered Doom's SKY1 composite starts eight rows early.
        case 0xbc9519b06539ef69ULL: if(name[3]=='1')return SkyArtwork::ColdMountains; break;
        case 0xb451911c760f176eULL: if(name[3]=='2')return SkyArtwork::RedMountains; break;
        case 0x43e2c7f2a7d8837cULL: if(name[3]=='3')return SkyArtwork::Inferno; break;
        case 0x85864a7716794480ULL: if(name[3]=='1')return SkyArtwork::BrownClouds; break;
        case 0x69c14db637c6ce05ULL: if(name[3]=='2')return SkyArtwork::BurningCity; break;
        case 0xe75fd27cf7bfebc8ULL: if(name[3]=='3')return SkyArtwork::BloodHell; break;
    }
    return SkyArtwork::Unknown;
}

bool skyDepthCloudOnly(SkyArtwork artwork) {
    return artwork==SkyArtwork::OrangeClouds||artwork==SkyArtwork::BrownClouds;
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

#include "sky_depth_city.inc"

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

static const SkyDepthCacheEntry &loadSkyDepth(const Image &sky,bool deep) {
    static const SkyDepthCacheEntry invalid{};
    if(sky.skyPalette!=stockSkyPalette)return invalid;
    auto &cached=skyDepthCache[(size_t)sky.skyArtwork][deep?1:0];
    if(cached.attempted)return cached;
    cached.attempted=true;
    const int paletteLump=W_GetNumForName((char*)"PLAYPAL");
    const byte *palette=(const byte*)W_CacheLumpNum(paletteLump,PU_CACHE);
    const bool cloudOnly=skyDepthCloudOnly(sky.skyArtwork);
    const auto skyline=cloudOnly?std::vector<int>{}:findSkyline(sky,palette);
    if(!cloudOnly&&skyline.empty()) {
        if(deep&&sky.skyArtwork==SkyArtwork::BurningCity&&devparm)
            fprintf(stderr,"Sky depth: RSKY2 skyline confidence too low.\n");
        return cached;
    }

    const int w=sky.width,h=sky.height;
    if(deep) {
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
    std::vector<byte> layers((size_t)w*h*2);
    for(int x=0;x<w;++x)for(int y=0;y<h;++y) {
        const int distance=y-skyline[x];
        // Behind the cutout, extend a short strip of clean sky. Motion falls
        // to zero near the cutout, so this fill is only exposed by filtering.
        const int source=distance<0?y:std::max(0,skyline[x]-1-std::min(distance,h/12));
        const size_t at=2*((size_t)y*w+x);
        layers[at]=sky.pixels[2*((size_t)source*w+x)];
        layers[at+1]=(byte)std::clamp(134+distance*12,0,255);
    }
    cached.texture=gpuCreateTexture(GpuFormat::RG8,w,h,layers.data(),w*2);
    return cached;
}

const GpuTextureRef &skyDepthTexture(const Image &sky,bool deep) {
    return loadSkyDepth(sky,deep).texture;
}

const SkyDepthScene &skyDepthScene(const Image &sky) {
    return loadSkyDepth(sky,true).scene;
}

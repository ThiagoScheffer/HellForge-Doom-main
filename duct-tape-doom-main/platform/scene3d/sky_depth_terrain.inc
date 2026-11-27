// Shaped, indexed terrain in the shared scenery atlas. Near and middle
// occupy independent planes so a foreground slope can expose hidden rock.
struct SkyTerrainPoint { int column,row; };
struct SkyTerrainRidge {
    const SkyTerrainPoint *crest;
    size_t count;
    int peak;
    bool near;
};
struct SkyTerrainPocket {
    float column,row,radius,width,height,density,drift,rise;
    SkyEffectDepthSlot slot;
};
struct SkyTerrainProfile {
    std::array<uint64_t,2> artwork;
    std::array<SkyTerrainRidge,4> ridges;
    std::array<SkyTerrainPocket,MaxSkyFireSources> pockets;
    std::array<float,4> shells;
    float farRadius,extinction;
    uint32_t seed;
};

static const SkyTerrainProfile *skyTerrainProfile(const Image &sky) {
    // Control points trace the stock contours where painted ridges overlap.
    // The west peak is one formation that crosses the source tile boundary.
    static constexpr SkyTerrainPoint west[]={
        {176,120},{183,86},{194,72},{204,58},{215,38},{222,28},{230,23},
        {240,23},{249,25},{256,31},{265,34},{273,43},{281,63},{292,77},
        {299,101},{308,115},{314,120}
    };
    static constexpr SkyTerrainPoint center[]={
        {59,120},{76,112},{82,103},{89,84},{97,60},{101,43},{110,32},
        {121,26},{137,22},{149,29},{157,39},{166,50},{173,64},{179,77},
        {184,104},{187,120}
    };
    static constexpr SkyTerrainPoint rear[]={
        {3,47},{20,39},{28,31},{37,23},{48,18},{59,19},{80,16},
        {87,20},{100,22},{108,28},{120,33},{136,48},{150,70},{160,120}
    };
    static constexpr SkyTerrainPoint saddle[]={
        {137,119},{154,65},{167,51},{179,54},{191,60},{195,58},
        {201,56},{209,49},{214,43},{223,68},{239,119}
    };
    static constexpr SkyTerrainProfile phobos={
        {0x4903f3c6668a384eULL,0xbc9519b06539ef69ULL},
        {{{west,std::size(west),237,true},{center,std::size(center),136,true},
          {rear,std::size(rear),78,false},{saddle,std::size(saddle),192,false}}},
        {{{54,103,19500,.18f,.27f,.34f,.85f,.29f,SkyEffectDepthSlot::BetweenMiddleAndNear},
          {183,86,18800,.15f,.24f,.27f,1.f,.23f,SkyEffectDepthSlot::BetweenMiddleAndNear},
          {190+256,69,40100,.21f,.23f,.28f,.64f,.19f,SkyEffectDepthSlot::BehindMiddle},
          {184+512,68,43200,.23f,.24f,.32f,.92f,.26f,SkyEffectDepthSlot::BehindMiddle}}},
        {27000.f,12200.f,3900.f,2400.f},59000.f,.108f,0x2be4c9b7u
    };
    if(sky.skyArtwork==SkyArtwork::ColdMountains&&sky.skyPalette==stockSkyPalette&&
       sky.width==256&&sky.height==128&&
       std::find(phobos.artwork.begin(),phobos.artwork.end(),sky.skyFingerprint)!=phobos.artwork.end())
        return &phobos;
    return nullptr;
}

static uint32_t skyTerrainHash(uint32_t seed) {
    seed=(seed^(seed>>16))*0x7feb352du;
    seed=(seed^(seed>>15))*0x846ca68bu;
    return seed^(seed>>16);
}

static SkyDepthScene buildSkyTerrain(const Image &sky,const byte *palette,
                                     const SkyTerrainProfile &profile,SkyAtmosphereProfile &atmosphere) {
    const auto start=SDL_GetPerformanceCounter();
    const int w=sky.width,h=sky.height,panorama=4*w;
    const int shift=sky.skyFingerprint==profile.artwork[0]?0:8;
    const size_t plane=size_t(w)*h*4;
    const auto skyline=findSkyline(sky,palette);
    if(skyline.empty())return {};
    auto wrap=[w](int x){return (x%w+w)%w;};
    auto pixel=[&](int x,int y) {return size_t(std::clamp(y,0,h-1))*w+wrap(x);};
    auto paint=[&](int x,int y) {
        // Fill the cropped composite's empty bottom rows from its last
        // covered texel in the same column.
        return sky.pixels[2*pixel(x,std::min(y,h-9+shift))];
    };
    auto luma=[&](int x,int y){return skyLuma(palette+3*paint(x,y));};
    struct Column { int crest=128,id=0; float shoulder=0; };
    std::vector<Column> near(w),middle(w);
    for(size_t id=0;id<profile.ridges.size();++id) {
        const auto &ridge=profile.ridges[id];
        auto &layer=ridge.near?near:middle;
        const int first=ridge.crest[0].column,last=ridge.crest[ridge.count-1].column;
        for(size_t segment=1;segment<ridge.count;++segment) {
            const auto &a=ridge.crest[segment-1],&b=ridge.crest[segment];
            for(int x=a.column;x<=b.column;++x) {
                const int col=wrap(x);
                const float fraction=float(x-a.column)/(b.column-a.column);
                int row=int(std::lround(a.row+(b.row-a.row)*fraction))+shift;
                if(row>=h-1)continue;
                if(ridge.near) {
                    if(std::abs(skyline[col]-row)<=3)row=skyline[col];
                    else if(row<h-13) {
                        // Separate the dark rock shoulder from the grey ridge
                        // behind it. Bound the search to the traced edge.
                        int best=row;
                        float support=7.f;
                        for(int dy=-2;dy<=2;++dy) {
                            float edge=0;
                            for(int dx=-1;dx<=1;++dx)
                                edge+=(luma(x+dx,row+dy-2)-luma(x+dx,row+dy+2))/3.f;
                            edge-=std::abs(dy)*3.f;
                            if(edge>support) {best=row+dy;support=edge;}
                        }
                        row=best;
                    }
                } else if(std::abs(skyline[col]-row)<=4)row=skyline[col];
                if(row>=layer[col].crest)continue;
                const float distance=x<ridge.peak?float(ridge.peak-x)/(ridge.peak-first):
                                                   float(x-ridge.peak)/(last-ridge.peak);
                layer[col]={row,int(id)+1,1.f-distance*distance};
            }
        }
    }
    std::vector<byte> atlas(plane*6+w*4),mask(size_t(w)*h);
    auto middleStone=[&](int x,int depth) {
        const int start=middle[x].crest+3;
        const int span=std::max(1,std::min(near[x].crest-6,h-9+shift)-start);
        const int fold=depth%(span*2);
        return paint(x,start+std::min(fold,span*2-fold));
    };
    int firstRow=h,skyPixels=0;
    std::array<int,2> rockPixels{};
    for(int x=0;x<w;++x)for(int y=0;y<h;++y) {
        const size_t at=pixel(x,y);
        // Bound the usable sky with both the measured skyline and the
        // independent contours. Palette statistics see only covered air.
        if(y+2<std::min({skyline[x],near[x].crest,middle[x].crest})) {
            mask[at]=255;
            ++skyPixels;
        }
        for(int band=0;band<2;++band) {
            const auto &columns=band==0?near:middle;
            const auto &col=columns[x];
            byte *out=atlas.data()+plane*band+4*at;
            // The radius field extends above each crest. Use the same
            // projected UV to locate the face and its painted material.
            const float down=std::clamp(float(y-col.crest)/std::max(1,h-col.crest),0.f,1.f);
            const float relief=.76f-.99f*col.shoulder-.68f*down;
            out[2]=(byte)std::lround(std::clamp(relief*.5f+.5f,0.f,1.f)*255.f);
            if(y<col.crest)continue;
            ++rockPixels[band];
            firstRow=std::min(firstRow,col.crest);
            out[1]=(byte)col.id;
            out[0]=paint(x,y);
            const float value=skyLuma(palette+3*out[0]);
            if(band==1&&(y+3>=near[x].crest||value<75.f||value>154.f)) {
                // Fill hidden and mixed edge texels from the exposed grey
                // ridge. Keep a dark duplicate of the foreground from
                // appearing along the valley when the two faces separate.
                out[0]=middleStone(36+wrap(x+17)%41,y-col.crest);
            } else if(band==0&&y-col.crest<6&&value>117.f) {
                // The art is a composite. Guard high-contrast border
                // texels against revealing a bright strip of sky as rock.
                const int direction=near[wrap(x-1)].crest<near[wrap(x+1)].crest?-1:1;
                out[0]=paint(x+direction*2,y+6);
            }
            const float flank=columns[wrap(x+2)].shoulder-columns[wrap(x-2)].shoulder;
            // Preserve the painting's illumination. Shade the shoulders
            // gently according to the baked orientation of the ridge.
            out[3]=(byte)std::lround(128.f+std::clamp(flank*156.f,-46.f,38.f));
        }
    }
    if(skyPixels<w*h/6||rockPixels[0]<w*h/4||rockPixels[1]<w*h/4)return {};
    atmosphere=analyzeMaskedSkyAtmosphere(sky.pixels.data(),w,h,palette,mask.data(),SkyAtmosphereKind::DoomMountains);
    if(atmosphere.kind==SkyAtmosphereKind::Legacy)return {};
    atmosphere.paintedSky=0.f;
    atmosphere.coverage=std::min(atmosphere.coverage+.08f,.83f);
    atmosphere.windSpeed=39.f;

    // Broad, overlapping ridges across the unwrapped four-tile panorama.
    // Their irregular shoulders are sampled from the measured source crest.
    constexpr int formations=16;
    const int spacing=panorama/formations;
    std::vector<int> farCrest(panorama,79);
    for(int i=0;i<formations;++i) {
        const auto seed=skyTerrainHash(profile.seed+uint32_t(i));
        const int center=i*spacing+int(seed%31)-15;
        const int left=40+int((seed>>5)%33);
        const int right=37+int((seed>>10)%38);
        const int height=21+int((seed>>16)%26);
        const bool mirror=seed&0x80000000u;
        for(int dx=-left;dx<=right;++dx) {
            const float t=float(dx)/(dx<0?left:right);
            const int source=136+int((mirror?-t:t)*38.f);
            const float measured=std::clamp((65.f+shift-near[source].crest)/43.f,0.f,1.f);
            const float shoulder=(1.f-t*t)*(.18f+.82f*measured);
            const int x=(center+dx+panorama)%panorama;
            farCrest[x]=std::min(farCrest[x],79-int(shoulder*height));
        }
    }
    for(int x=0;x<panorama;++x)for(int y=farCrest[x];y<h;++y) {
        const int sampleX=36+((x*13/19+x/128*7)%41);
        byte *out=atlas.data()+4*(size_t((2+x/w)*h+1+y)*w+x%w);
        out[0]=middleStone(sampleX,(y-farCrest[x])*2/3);
        out[1]=(byte)std::min(255,165+(y-farCrest[x])*49);
    }

    SkyDepthScene scene;
    scene.atlas=gpuCreateTexture(GpuFormat::RGBA8,w,6*h+1,atlas.data(),w*4);
    if(!scene.atlas)return {};
    scene.style=SkyDepthScene::Style::ShapedTerrain;
    scene.shells=profile.shells;
    scene.firstRow=float(firstRow)/h;
    scene.farRadius=profile.farRadius;
    scene.farFirstRow=float(*std::min_element(farCrest.begin(),farCrest.end()))/h;
    scene.terrainExtinction=profile.extinction;
    scene.atmosphere=&atmosphere;

    SkyFireSmokeProfile mist{};
    mist.windSpeed=atmosphere.windSpeed;
    for(int c=0;c<3;++c)mist.smokeDark[c]=atmosphere.horizon[c]*.66f+atmosphere.cloudMiddle[c]*.15f;
    for(const auto &p:profile.pockets) {
        if(near[wrap(int(p.column))].crest<p.row+shift-9.f)continue;
        auto &source=mist.sources[mist.count++];
        source.anchor={-p.column*(6.283185307f/panorama),(.70f-(p.row+shift)/h)/.68f,p.radius};
        source.slot=p.slot;
        source.smoke={p.width,p.height,p.density,p.rise,p.drift,0.f,true,true};
        source.seed=skyTerrainHash(profile.seed+uint32_t(p.column));
    }
    auto effects=prepareSkyFireSmoke(mist);
    if(effects.header[0]>0)scene.mist=std::make_shared<SkyFireSmokeGpuBlock>(std::move(effects));
    if(devparm) {
        const double ms=(SDL_GetPerformanceCounter()-start)*1000.0/SDL_GetPerformanceFrequency();
        const unsigned sources=scene.mist?unsigned(scene.mist->header[0]):0;
        fprintf(stderr,"Sky terrain: %016llx valid, %zu painted/%d distant formations, near/middle/far %.0f/%.0f/%.0f, %u mist pockets, %zu atlas bytes, %.2f ms%s.\n",
                (unsigned long long)sky.skyFingerprint,profile.ridges.size(),formations,
                scene.shells[1],scene.shells[0],scene.farRadius,sources,atlas.size(),ms,
                sources?"":"; optional mist disabled");
    }
    return scene;
}

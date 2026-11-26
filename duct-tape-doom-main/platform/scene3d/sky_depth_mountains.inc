// Mountain profiles share the scenery atlas and cylindrical projection
// used by buildings. Coordinates below are in stock texture texels.
struct SkyMountainSeep {
    int column,row,length,drift;
    byte strength;
};

struct SkyMountainProfile {
    uint64_t artwork;
    int width,height;
    std::array<std::array<int,2>,2> nearRidges;
    std::array<SkyMountainSeep,4> seeps;
    std::array<float,4> shells;
    float farRadius,wallOpacity;
};

static const SkyMountainProfile *skyMountainProfile(const Image &sky) {
    static constexpr SkyMountainProfile profiles[]={
        {0xe75fd27cf7bfebc8ULL,256,128,{{{23,79},{112,161}}},
         {{{45,68,27,4,108},{132,73,29,-4,150},{178,71,23,6,92},{220,93,18,2,125}}},
         {62000.f,23500.f,8400.f,3200.f},157000.f,.69f}
    };
    if(sky.skyPalette!=stockSkyPalette)return nullptr;
    for(const auto &profile:profiles)
        if(sky.skyFingerprint==profile.artwork&&sky.width==profile.width&&sky.height==profile.height)
            return &profile;
    return nullptr;
}

static SkyDepthScene buildSkyDepthMountains(const Image &sky,const byte *palette,
                                            const SkyMountainProfile &profile) {
    const int w=sky.width,h=sky.height;
    const int panorama=w*4;
    const size_t plane=(size_t)w*h*4;
    std::vector<byte> atlas(plane*6+w*4),backdrop((size_t)w*h);
    std::vector<int> ridgeline(w,h);
    auto cell=[&](int x,int y){return (size_t)std::clamp(y,0,h-1)*w+(x%w+w)%w;};
    auto index=[&](int x,int y){return sky.pixels[2*cell(x,y)];};
    auto rock=[&](int x,int y) {
        const byte *p=palette+3*index(x,y);
        return p[0]-p[1]<21||p[1]*2>p[0];
    };

    // Require vertically supported stone and a substantial body below the
    // crest. The wall's palette is saturated red, even between its bright ribs.
    int reliable=0;
    for(int x=0;x<w;++x) {
        for(int y=h/3;y<h-8;++y) {
            if(!rock(x,y))continue;
            int support=0;
            for(int d=0;d<8;++d)support+=rock(x,y+d);
            if(support<7)continue;
            ridgeline[x]=y;
            break;
        }
        reliable+=ridgeline[x]<h;
    }
    if(reliable<w*15/16)return {};
    // Center a short median on unsupported columns only. Preserve the
    // measured pixel edges and steep notches everywhere else.
    const auto measured=ridgeline;
    for(int x=0;x<w;++x)if(measured[x]==h) {
        std::array<int,5> neighbors{};
        for(int j=0;j<5;++j)neighbors[j]=measured[(x+j-2+w)%w];
        std::sort(neighbors.begin(),neighbors.end());
        ridgeline[x]=neighbors[2];
        if(ridgeline[x]==h)return {};
    }

    std::array<int,2> stonePixels{};
    int wallPixels=0;
    for(int x=0;x<w;++x) {
        int group=1;
        for(size_t i=0;i<profile.nearRidges.size();++i) {
            const auto &range=profile.nearRidges[i];
            if(x>=range[0]&&x<range[1])group=int(i)+2;
        }
        const int crest=ridgeline[x],layer=group==1?1:2;
        atlas[plane*2+4*x]=(byte)group;
        for(int y=0;y<h;++y) {
            const size_t at=cell(x,y);
            if(y>=crest) {
                byte *meta=atlas.data()+plane+4*at;
                meta[0]=(byte)group;
                meta[1]=(byte)layer;
                meta[2]=index(x-2,y+3);
                ++stonePixels[layer-1];
            }
            // Keep intact source pixels in front of the clouds. Taper a
            // four-texel edge fill into the atmosphere where parallax exposes
            // wall that was covered by stone in the original painting.
            const int sourceY=std::min(y,crest-1);
            atlas[4*at]=index(x,sourceY);
            atlas[4*at+1]=(byte)(255-64*std::clamp(y-crest+1,0,3));
            if(y>=crest+3)atlas[4*at+1]=0;
            if(y+3<crest&&!rock(x,y)) {
                backdrop[at]=255;
                ++wallPixels;
            }
        }
    }
    if(wallPixels<w*h/3||stonePixels[0]<w*h/12||stonePixels[1]<w*h/12)return {};
    if(cacheSkyAtmosphere(sky,palette,backdrop.data()).kind==SkyAtmosphereKind::Legacy)return {};

    // These short tracks follow warm seams on the artwork. Require a warm
    // rock candidate at each step and keep well below the silhouette.
    int detailPixels=0;
    for(const auto &seep:profile.seeps)for(int step=0;step<seep.length;++step) {
        const int y=seep.row+step;
        const int center=seep.column+step*seep.drift/seep.length+(step/4)%2;
        int best=-1,bestWarmth=5;
        for(int dx=-1;dx<=1;++dx) {
            const int x=((center+dx)%w+w)%w;
            if(y<ridgeline[x]+4||y>=h-3)continue;
            const byte *p=palette+3*index(x,y);
            const int warmth=p[0]-p[2];
            if(p[0]<150&&p[0]>p[1]&&warmth>bestWarmth) {
                best=x;
                bestWarmth=warmth;
            }
        }
        if(best<0)continue;
        const int taper=std::min({step+1,seep.length-step,5});
        atlas[plane+4*cell(best,y)+3]=(byte)(seep.strength*taper/5);
        ++detailPixels;
    }

    // Stretch and phase the measured crest into a single periodic
    // panorama. Broken peaks are taken from a second offset into the same
    // source, so the distant silhouette keeps the characteristic rock forms.
    int farRow=h;
    for(int x=0;x<panorama;++x) {
        const int source=(x*3/4+71)%w;
        const int offset=(x+143)%w;
        int crest=25+(ridgeline[source]*5+ridgeline[offset]*2)/10;
        const int segment=x/128;
        const int spire=19+(31+segment*41+segment*segment*13)%88;
        const int flank=x%128-spire;
        const int height=7+(segment*5)%7;
        crest-=std::max(0,height-std::abs(flank)*(flank<0?2:3));
        farRow=std::min(farRow,crest);
        for(int y=crest;y<h;++y) {
            const size_t at=4*((size_t)((2+x/w)*h+1+y)*w+x%w);
            atlas[at]=index(source,ridgeline[source]+(y-crest)*2/3);
            atlas[at+1]=255;
        }
    }
    SkyDepthScene scene;
    scene.atlas=gpuCreateTexture(GpuFormat::RGBA8,w,6*h+1,atlas.data(),w*4);
    if(!scene.atlas)return {};
    scene.style=SkyDepthScene::Style::Mountains;
    scene.shells=profile.shells;
    scene.firstRow=float(*std::min_element(ridgeline.begin(),ridgeline.end()))/h;
    scene.farRadius=profile.farRadius;
    scene.farFirstRow=float(farRow)/h;
    scene.backdropOpacity=profile.wallOpacity;
    if(devparm)fprintf(stderr,"Sky A5: %d/%d supported ridge columns, %d wall samples, %d middle/%d near rock texels, %d seam texels.\n",
                       reliable,w,wallPixels,stonePixels[0],stonePixels[1],detailPixels);
    return scene;
}

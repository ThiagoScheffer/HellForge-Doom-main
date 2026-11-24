// RSKY2 analyzer. Coordinates are in the loaded, horizontally wrapped WAD
// image. The renderer only consumes the resulting indexed atlas and shells.
struct SkyDepthBuilding {
    int first,end;
    float haze;
    byte sideIndex,layer;
};

static std::vector<SkyDepthBuilding> segmentSkyBuildings(const Image &sky,const byte *palette,
                                                       const std::vector<int> &roof) {
    const int w=sky.width,h=sky.height;
    auto index=[&](int x,int y){return sky.pixels[2*((size_t)std::min(y,h-1)*w+(x+w)%w)];};
    auto luma=[&](int x,int y){return skyLuma(palette+3*index(x,y));};
    struct Edge {int x;float strength;};
    std::vector<Edge> candidates;
    for(int x=0;x<w;++x) {
        const int step=std::abs(roof[x]-roof[(x+w-1)%w]);
        const int top=std::max(roof[x],roof[(x+w-1)%w]);
        float vertical=0,windowPattern=0;
        for(int y=top+3;y<std::min(h,top+34);++y) {
            const float left=luma(x-1,y),right=luma(x,y);
            vertical+=std::clamp((std::abs(left-right)-std::abs(luma(x-2,y)-left)
                                  -std::abs(luma(x+1,y)-right))/24.0f,0.0f,1.0f);
            windowPattern+=(left>30)!=(right>30);
        }
        if(step>=4||vertical>=8.0f)
            candidates.push_back({x,2.0f*std::min(step,18)+vertical*.55f+windowPattern*.10f});
    }
    std::stable_sort(candidates.begin(),candidates.end(),[](const Edge &a,const Edge &b){
        return a.strength>b.strength;
    });
    constexpr int maxBuildings=48,minWidth=3;
    std::vector<int> edges;
    for(const Edge &edge:candidates) {
        if(edges.size()==maxBuildings)break;
        if(std::none_of(edges.begin(),edges.end(),[&](int x){
            const int distance=std::abs(x-edge.x);
            return std::min(distance,w-distance)<minWidth;
        }))edges.push_back(edge.x);
    }
    std::sort(edges.begin(),edges.end());
    std::vector<SkyDepthBuilding> buildings;
    for(size_t i=0;i<edges.size();++i) {
        const int first=edges[i],end=i+1<edges.size()?edges[i+1]:edges[0]+w;
        float top=0,facade=0,contrast=0;
        std::array<int,256> tones{};
        for(int col=first;col<end;++col) {
            const int x=col%w,row=roof[x];
            top+=row;
            for(int dy=4;dy<18;++dy) {
                const byte ink=index(x,row+dy);
                const float value=skyLuma(palette+3*ink);
                facade+=std::min(50.0f,value)/14;
                if(value<36)++tones[ink];
            }
            contrast+=std::max(0.0f,luma(x,row-3)-luma(x,row+2));
        }
        const float span=float(end-first);
        const float low=std::clamp((top/span-h*.33f)/(h*.29f),0.0f,1.0f);
        const float soft=std::clamp((facade/span-8)/25,0.0f,1.0f);
        const float haze=std::clamp(.11f+.66f*low+.24f*soft,0.11f,.92f);
        const byte layer=haze<.42f&&contrast/span>10.0f&&end-first>=4?2:1;
        const byte ink=(byte)(std::max_element(tones.begin(),tones.end())-tones.begin());
        buildings.push_back({first,end,haze,ink,layer});
    }
    return buildings;
}

static int distantCity(const Image &sky,const byte *palette,const std::vector<int> &roof,
                       const std::vector<SkyDepthBuilding> &buildings,
                       std::vector<byte> &atlas,int &firstRow) {
    const int w=sky.width,h=sky.height,turn=w*4;
    const size_t first=(size_t)w*(2*h+1)*4;
    atlas.resize(first+size_t(turn)*h*4);
    auto texel=[&](int longitude,int y) {
        int x=(longitude+turn)%turn;
        return atlas.data()+first+size_t((x/w)*h+y)*w*4+(x%w)*4;
    };
    auto nearestInk=[&](float r,float g,float b) {
        float best=1e9f;
        byte ink=0;
        for(int i=0;i<256;++i) {
            float dr=r-palette[3*i],dg=g-palette[3*i+1],db=b-palette[3*i+2];
            float error=dr*dr+dg*dg+db*db;
            if(error<best){best=error;ink=(byte)i;}
        }
        return ink;
    };
    uint32_t seed=0x72d8ac04u;
    auto pick=[&](int range) {
        seed=seed*1664525u+1013904223u;
        return int((seed>>16)%range);
    };
    // Span and height distributions come from the extracted architecture.
    // The four longitude tiles store one panorama, including seam crossings.
    constexpr int count=44;
    firstRow=h;
    for(int i=0;i<count;++i) {
        const auto &model=buildings[pick((int)buildings.size())];
        const int center=i*turn/count+pick(13)-6;
        const int width=std::clamp(8+(model.end-model.first)/2+pick(7),10,23);
        std::array<int,9> nearby{};
        for(int j=0;j<9;++j)nearby[j]=roof[(center+turn+(j-4)*3)%w];
        std::sort(nearby.begin(),nearby.end());
        const int top=std::clamp(nearby[4]-4-pick(12),h*43/100,h*62/100);
        const int bottom=std::min(h,top+h/3+pick(16));
        const int crown=2+pick(6),windowPitch=3+pick(2),phase=pick(windowPitch),floorPitch=4+pick(3);
        const bool spire=pick(4)==0;
        const byte *body=palette+3*model.sideIndex;
        const float warmth=float(pick(17));
        const byte wall=nearestInk(18+body[0]*.32f+warmth,15+body[1]*.30f+warmth*.5f,12+body[2]*.26f);
        const byte side=nearestInk(12+body[0]*.26f+warmth,11+body[1]*.24f,9+body[2]*.22f);
        const byte pillar=nearestInk(34+body[0]*.28f+warmth,23+body[1]*.27f+warmth*.5f,14+body[2]*.25f);
        const byte pane=nearestInk(125+warmth*2,73+warmth,30+warmth);
        firstRow=std::min(firstRow,top-(spire?2:0));
        for(int col=0;col<width;++col) {
            const int x=center-width/2+col;
            // Some rooftops step down to a wing, leaving a narrower tower.
            const int row=top+(col<width/5?crown:col>=width*3/4?crown/2:
                              spire&&col==width/2?-2:0);
            for(int y=row;y<bottom;++y) {
                auto *pixel=texel(x,y);
                const bool bay=(col+phase)%windowPitch==0;
                const bool lit=col>1&&col<width-3&&bay&&y>row+4&&(y-top)%floorPitch==1&&pick(4)==0;
                const bool rim=y==row||(y>top+3&&y%floorPitch==0);
                pixel[0]=lit?pane:col>=width*3/4?side:bay||rim?pillar:wall;
                pixel[1]=(byte)std::clamp((bottom-y)*36,0,255);
            }
        }
    }
    return count;
}

static SkyDepthScene buildSkyDepthCity(const Image &sky,const byte *palette,
                                      const std::vector<int> &roof,
                                      const std::vector<SkyDepthBuilding> &buildings,
                                      std::vector<byte> &atlas) {
    const int nearCount=(int)std::count_if(buildings.begin(),buildings.end(),
                                        [](const SkyDepthBuilding &b){return b.layer==2;});
    if(nearCount==0||nearCount==(int)buildings.size()) {
        if(devparm)fprintf(stderr,"Sky depth: RSKY2 has insufficient support for two depths.\n");
        return {};
    }
    const int w=sky.width,h=sky.height;
    const size_t plane=(size_t)w*h*4;
    // Preserve the existing atmosphere channels on plane 0. Plane 1 is
    // solid object ID, shell index, unlit side palette index, window strength.
    // The final row identifies the complete object interval in each column.
    atlas.resize(plane*2+w*4);
    auto cell=[&](int x,int y){return (size_t)std::clamp(y,0,h-1)*w+(x+w)%w;};
    auto source=[&](size_t at){return sky.pixels[2*at];};
    auto rgb=[&](size_t at){return palette+3*source(at);};
    auto brightFire=[&](size_t at) {
        const byte *p=rgb(at);
        return p[0]>=150&&p[0]-p[2]>=70&&skyLuma(p)>=96;
    };
    for(size_t id=0;id<buildings.size();++id) {
        const auto &building=buildings[id];
        for(int col=building.first;col<building.end;++col) {
            const int x=col%w;
            atlas[plane*2+4*x]=(byte)(id+1);
            for(int y=roof[x];y<h;++y) {
                const size_t at=cell(x,y);
                int flame=0;
                if(brightFire(at)) {
                    for(int dy=-2;dy<=2;++dy)
                        for(int dx=-2;dx<=2;++dx)flame+=brightFire(cell(x+dx,y+dy));
                }
                // Broad patches of painted fire are part of the backdrop.
                // Isolated bright panes and their dark mullions stay together.
                if(flame>=17)continue;
                byte *metadata=atlas.data()+plane+4*at;
                metadata[0]=(byte)(id+1);
                metadata[1]=building.layer;
                metadata[2]=building.sideIndex;
                metadata[3]=atlas[4*at+3];
            }
        }
    }

    std::vector<byte> skyMask((size_t)w*h);
    for(int y=0;y<h;++y)for(int x=0;x<w;++x) {
        const size_t at=(size_t)y*w+x;
        const auto *tone=rgb(at);
        const float luminance=skyLuma(tone);
        // Keep a guard band above foreground roofs, and reject the distant
        // buildings marked by the original extraction. Brilliant neutral
        // smoke cores and flames do not represent stable dusk colors.
        if(y+5<roof[x]&&atlas[at*4+2]<128&&luminance>=20&&luminance<=156&&
           !(tone[0]>tone[1]*1.65f&&tone[1]>tone[2]*1.7f))
            skyMask[at]=255;
    }
    const auto &atmosphere=cacheSkyAtmosphere(sky,palette,skyMask.data());
    SkyDepthScene scene;
    if(atmosphere.kind==SkyAtmosphereKind::BurningCityDusk) {
        int farRow=h;
        const int count=distantCity(sky,palette,roof,buildings,atlas,farRow);
        scene.atlas=gpuCreateTexture(GpuFormat::RGBA8,w,6*h+1,atlas.data(),w*4);
        if(scene.atlas) {
            scene.farRadius=180000.f;
            scene.farFirstRow=float(farRow)/h;
            if(devparm)fprintf(stderr,"Sky A2: %d distant buildings, %zu atlas bytes.\n",count,atlas.size());
        } else if(devparm)fprintf(stderr,"Sky A2: expanded atlas unavailable; using D1.\n");
    }
    if(!scene.atlas) {
        // Only the D1 fallback needs the reconstructed painted backdrop.
        atlas.resize(plane*2+w*4);
        using RGB=std::array<float,3>;
        std::vector<RGB> fill((size_t)w*h),next(fill.size()),seed(w);
        for(int x=0;x<w;++x) {
            float total=0;
            for(int dx=-9;dx<=9;++dx) {
                const int column=(x+dx+w)%w;
                const float weight=(10.0f-std::abs(dx))/(1.0f+std::abs(roof[column]-roof[x])*.09f);
                for(int dy=2;dy<=10;dy+=2) {
                    const auto *rgbAbove=rgb(cell(column,roof[column]-dy));
                    for(int c=0;c<3;++c)seed[x][c]+=weight*rgbAbove[c];
                    total+=weight;
                }
            }
            for(float &channel:seed[x])channel/=total;
        }
        for(int y=0;y<h;++y)for(int x=0;x<w;++x) {
            const auto at=cell(x,y);
            const bool solid=atlas[plane+4*at]!=0;
            const float fade=std::clamp(float(y-53)/64.0f,0.0f,1.0f);
            const RGB distant={43,30,24};
            for(int c=0;c<3;++c)
                fill[at][c]=solid?seed[x][c]+(distant[c]-seed[x][c])*fade:float(rgb(at)[c]);
            if(y>=roof[x]) {
                const auto above=cell(x,roof[x]-2);
                atlas[4*at+2]=(byte)(atlas[4*above+2]*(1.0f-fade));
                atlas[4*at+3]=std::max((byte)(atlas[4*above+3]*(1.0f-.65f*fade)),
                                      (byte)(!solid&&brightFire(at)?160:0));
            }
        }
        // Relax the extension in both axes, with original sky, distant paint
        // and fire as fixed constraints. This also joins the panorama seam.
        next=fill;
        for(int iteration=0;iteration<96;++iteration) {
            for(int y=0;y<h;++y)for(int x=0;x<w;++x) {
                const auto at=cell(x,y);
                if(!atlas[plane+4*at])continue;
                const auto left=cell(x-1,y),right=cell(x+1,y);
                const auto above=cell(x,y-1),below=cell(x,y+1);
                for(int c=0;c<3;++c)
                    next[at][c]=(fill[left][c]+fill[right][c]+fill[above][c]+fill[below][c])*.25f;
            }
            fill.swap(next);
        }
        for(size_t at=0;at<fill.size();++at) {
            int best=source(at);
            if(atlas[plane+4*at]) {
                float nearest=1e9f;
                for(int i=0;i<256;++i) {
                    const float r=fill[at][0]-palette[3*i],g=fill[at][1]-palette[3*i+1],
                                b=fill[at][2]-palette[3*i+2];
                    const float distance=r*r+g*g+b*b;
                    if(distance<nearest){nearest=distance;best=i;}
                }
            }
            atlas[4*at]=(byte)best;
        }
        scene.atlas=gpuCreateTexture(GpuFormat::RGBA8,w,2*h+1,atlas.data(),w*4);
    }
    if(!scene.atlas) {
        // The original plane was edited in place. Tell the caller to
        // regenerate it if the depth atlas cannot be uploaded.
        if(devparm)fprintf(stderr,"Sky depth: RSKY2 atlas allocation failed.\n");
        return {};
    }
    scene.shells={90000.0f,48000.0f,19800.0f,8640.0f};
    scene.firstRow=float(*std::min_element(roof.begin(),roof.end()))/h;
    if(devparm)fprintf(stderr,"Sky depth: RSKY2 extracted %d near and %zu middle buildings.\n",
                       nearCount,buildings.size()-nearCount);
    return scene;
}

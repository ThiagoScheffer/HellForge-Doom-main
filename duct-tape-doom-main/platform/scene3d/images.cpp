/* Textures from the WAD: upload with palette mips (sprites with their normals), bump
   relief, emissive masks and monitor glass; average and brightest colors. */
#include "internal.h"
#include "sky_atmosphere.h"
#include "../shaders/living_surfaces.h"
#include "../monster_regions.h"
#include "../monster_skin_damage.h"
#include "../monster_skin_regions.h"
#include "../corpse_decomposition.h"

namespace doom3d {
std::vector<unsigned> paletteLUTColors(const unsigned *palette) {
    std::vector<unsigned> colors(32*32*32);
    for(int b=0;b<32;++b)for(int g=0;g<32;++g)for(int r=0;r<32;++r) {
        int red=(r*255+15)/31,green=(g*255+15)/31,blue=(b*255+15)/31,best=0,bestDistance=INT_MAX;
        for(int i=0;i<256;++i) {
            int dr=red-int((palette[i]>>16)&255),dg=green-int((palette[i]>>8)&255),db=blue-int(palette[i]&255);
            int distance=dr*dr+dg*dg+db*db;
            if(distance<bestDistance) {bestDistance=distance;best=i;}
        }
        colors[(b*32+g)*32+r]=palette[best];
    }
    return colors;
}
void buildCloudTexture() {
    // Periodic, deterministic low-frequency cloud texture; no downloaded assets.
    std::array<byte,64*64> pixels;
    for(int y=0;y<64;++y)for(int x=0;x<64;++x) {
        float u=x*doomPi/32,v=y*doomPi/32;
        float cloud=0.5f+0.19f*sin(u+sin(v))+0.16f*cos(v*2-u)+0.10f*sin(u*3+v*2);
        pixels[y*64+x]=(byte)(std::clamp(cloud,0.0f,1.0f)*255);
    }
    cloudTexture=gpuCreateTexture(GpuFormat::R8,64,64,pixels.data(),64);
    if(!cloudTexture)I_Error((char*)"Could not allocate cloud texture");
}
// Palette mip levels for walls and flats, as software Quake built them: each
// level texel averages the colors it covers (weighted by coverage) and snaps
// back to the nearest PLAYPAL entry, so the shader keeps applying the live
// palette (damage and pickup flashes). Coverage and the emissive mask
// average; monitor glass takes the middle texel. Returns every level after
// the first, packed at the texture's channel count, and counts all levels.
static std::vector<byte> paletteMips(int w,int h,const byte *base,int channels,int &levels) {
    const byte *palette=(const byte*)W_CacheLumpName((char*)"PLAYPAL",PU_CACHE);
    // Nearest entries by 5-bit color cell, filled as cells are first used.
    static std::array<byte,768> cachedPalette={};
    static std::vector<short> nearest;
    if(nearest.empty()||memcmp(cachedPalette.data(),palette,768)) {
        memcpy(cachedPalette.data(),palette,768);nearest.assign(32*32*32,-1);
    }
    auto snap=[&](float r,float g,float b) {
        int cell=(std::min(31,(int)(r/8))<<10)|(std::min(31,(int)(g/8))<<5)|std::min(31,(int)(b/8));
        if(nearest[cell]<0) {
            float cr=(cell>>10)*8+4.0f,cg=((cell>>5)&31)*8+4.0f,cb=(cell&31)*8+4.0f,best=1e30f;
            for(int i=0;i<256;++i) {
                float dr=palette[i*3]-cr,dg=palette[i*3+1]-cg,db=palette[i*3+2]-cb;
                float d=2*dr*dr+4*dg*dg+3*db*db;
                if(d<best) {best=d;nearest[cell]=(short)i;}
            }
        }
        return (byte)nearest[cell];
    };
    std::vector<byte> out;levels=1;
    for(int lw=w,lh=h;lw>1||lh>1;++levels) {
        lw=std::max(1,lw/2);lh=std::max(1,lh/2);
        for(int y=0;y<lh;++y) for(int x=0;x<lw;++x) {
            int x0=x*w/lw,x1=std::max(x0+1,(x+1)*w/lw),y0=y*h/lh,y1=std::max(y0+1,(y+1)*h/lh);
            double rgb[3]={},coverage=0,mask=0;int count=0;
            for(int sy=y0;sy<y1;++sy) for(int sx=x0;sx<x1;++sx) {
                const byte *p=base+((size_t)sy*w+sx)*channels;
                for(int c=0;c<3;++c)rgb[c]+=palette[p[0]*3+c]*(double)p[1];
                coverage+=p[1];if(channels==4)mask+=p[2];++count;
            }
            const byte *middle=base+((size_t)((y0+y1)/2)*w+(x0+x1)/2)*channels;
            out.push_back(coverage>0?snap(float(rgb[0]/coverage),float(rgb[1]/coverage),float(rgb[2]/coverage)):middle[0]);
            out.push_back((byte)std::lround(coverage/count));
            if(channels==4) {out.push_back((byte)std::lround(mask/count));out.push_back(middle[3]);}
        }
    }
    return out;
}
// Channels: palette index, coverage, then the emissive mask (or sprite
// normal x) and, on walls and flats, monitor glass height (or sprite normal y).
// mipmaps: walls and flats also get palette mip levels.
Image upload(int w,int h,const byte *pixels,int left,int top,const byte *mask,const byte *normals,const byte *glass,bool mipmaps) {
    Image image; image.pixels.assign(pixels,pixels+(size_t)w*h*2); image.width=w; image.height=h; image.left=left; image.top=top;
    for(size_t i=0;i<(size_t)w*h;++i)if(pixels[2*i+1]!=255) {image.opaque=false;break;}
    std::vector<byte> packed;
    bool wide=mask||normals||glass;
    if(wide) {
        packed.resize((size_t)w*h*4);
        for(size_t i=0;i<(size_t)w*h;++i) {
            packed[4*i]=pixels[2*i];packed[4*i+1]=pixels[2*i+1];
            packed[4*i+2]=normals?normals[2*i]:mask?mask[i]:0;
            packed[4*i+3]=normals?normals[2*i+1]:glass?glass[i]:DOOM_GLASS_NONE;
        }
    }
    if(glass)image.screens.assign(glass,glass+(size_t)w*h); // Monitor glass, for shots (shot_lights.cpp).
    if(mask) {
        int lump=W_GetNumForName((char*)"PLAYPAL");
        if(W_LumpLength(lump)<768) I_Error((char*)"Invalid emissive palette");
        const byte *palette=(const byte*)W_CacheLumpNum(lump,PU_CACHE);
        double weight=0;
        for(int y=0;y<h;++y) for(int x=0;x<w;++x) {
            size_t i=(size_t)y*w+x;float e=mask[i]/255.0f;
            weight+=e;image.emissionU+=(x+0.5f)*e;image.emissionV+=(y+0.5f)*e;
            for(int c=0;c<3;++c) image.emissionColor[c]+=palette[pixels[2*i]*3+c]/255.0f*e;
        }
        if(weight>0) {
            image.emission.assign(mask,mask+(size_t)w*h); // Where shots break the glow (shot_lights.cpp).
            image.emissionU/=weight;image.emissionV/=weight;
            float peak=*std::max_element(image.emissionColor.begin(),image.emissionColor.end());
            for(float &c:image.emissionColor)c=std::max(0.04f,c/std::max(peak,0.001f));
            image.emissionCoverage=(float)weight/(w*h);
            image.emissionWeight=std::clamp(image.emissionCoverage*8.0f,0.25f,1.0f);
        }
    }
    int channels=wide?4:2,levels=1;
    const byte *levelData=wide?packed.data():pixels;
    std::vector<byte> chain;
    if(mipmaps) {
        std::vector<byte> mips=paletteMips(w,h,levelData,channels,levels);
        chain.assign(levelData,levelData+(size_t)w*h*channels);chain.insert(chain.end(),mips.begin(),mips.end());
        levelData=chain.data();
    }
    image.texture=gpuCreateTexture(wide?GpuFormat::RGBA8:GpuFormat::RG8,w,h,levelData,w*channels,levels);
    if(!image.texture) I_Error((char*)"Could not allocate a GPU texture");
    return image;
}
// Screen glass heights, or empty when the texture has no screens; frame
// receives where each glass pixel sits on its screen (doom_screen_glass),
// and each readout texel where it sits on its readout (doom_screen_readouts;
// empty when there are neither). Switches keep their lamps still, so the
// lit state stays plain.
static std::vector<byte> screenGlass(const char *name,bool flat,int w,int h,const byte *pixels,std::vector<byte> &frame) {
    frame.clear();
    if(!Game_ScreenTexture(name,flat))return {};
    const byte *palette=(const byte*)W_CacheLumpName((char*)"PLAYPAL",PU_CACHE);
    std::vector<byte> glass((size_t)w*h);frame.assign((size_t)w*h*4,0);
    int screens=doom_screen_glass(w,h,pixels,palette,glass.data(),frame.data());
    bool switches=!strncmp(name,"SW1",3)||!strncmp(name,"SW2",3);
    int readouts=switches?0:doom_screen_readouts(w,h,pixels,palette,glass.data(),frame.data());
    if(!readouts&&!screens)frame.clear();
    if(!screens)return {};
    return glass;
}
// Uploads the screen frame beside the image the shader refracts through it.
static Image &withGlassFrame(Image &image,const std::vector<byte> &frame) {
    if(frame.empty())return image;
    image.glassFrame=gpuCreateTexture(GpuFormat::RGBA8,image.width,image.height,frame.data(),image.width*4);
    if(!image.glassFrame)I_Error((char*)"Could not allocate a GPU texture");
    return image;
}
// Bump relief for walls and flats (Image::relief), from the artwork's log
// luminance, so a dark texture's seams count as much as a bright one's (in
// CEIL3_5 they are a single palette step). R/G: the slope along u and v, 128
// level and 255 just under 45 degrees; B: how grainy the texels around are,
// for varied highlights. Seams and mortar get real depth: the height is
// averaged 11 texels along each axis before taking its slope across, so long
// straight bands stand out while blotches a few texels wide wash out instead
// of turning into craters. A weak per-texel Sobel adds only fine pitting.
// Repeats like the texture. tiles: finds the painted tiles of a wall or flat
// (painted_tiles.h) so each gets a crooked lie; the relief is then stored at
// 16 bits with the anchor offset in alpha (noPaintedTile: none), the other
// channels as at 8 bits.
using Tiler=std::vector<uint16_t>(*)(const std::vector<float> &,int,int);
// depth scales the broad bands (seams, streaks); smooth leaves no grain
// (glassy highlights) and halves the per-texel tilt.
static Image &withRelief(Image &image,Tiler tiles=nullptr,float depth=1,bool smooth=false) {
    int w=image.width,h=image.height;size_t count=(size_t)w*h;
    const byte *palette=(const byte*)W_CacheLumpName((char*)"PLAYPAL",PU_CACHE);
    std::vector<float> height(count),across(count),down(count),broad(count),grain(count),rough(count),scratch(count);
    for(size_t i=0;i<count;++i) {
        const byte *rgb=palette+image.pixels[2*i]*3;
        height[i]=std::log((0.299f*rgb[0]+0.587f*rgb[1]+0.114f*rgb[2])/255.0f+0.03f);
    }
    auto at=[&](const std::vector<float> &v,int x,int y) {return v[(size_t)((y%h+h)%h)*w+(x%w+w)%w];};
    // Separable filter: taps along x into scratch, then along y into to.
    auto filter=[&](const std::vector<float> &from,std::vector<float> &to,const std::vector<float> &alongX,const std::vector<float> &alongY) {
        int rx=(int)alongX.size()/2,ry=(int)alongY.size()/2;
        for(int y=0;y<h;++y)for(int x=0;x<w;++x) {
            float sum=0;for(int k=-rx;k<=rx;++k)sum+=alongX[k+rx]*at(from,x+k,y);
            scratch[(size_t)y*w+x]=sum;
        }
        for(int y=0;y<h;++y)for(int x=0;x<w;++x) {
            float sum=0;for(int k=-ry;k<=ry;++k)sum+=alongY[k+ry]*at(scratch,x,y+k);
            to[(size_t)y*w+x]=sum;
        }
    };
    const std::vector<float> line(11,1/11.0f),tent={0.25f,0.5f,0.25f},binomial={1/16.0f,4/16.0f,6/16.0f,4/16.0f,1/16.0f};
    filter(height,across,tent,line); // Vertical bands, for the slope along u.
    filter(height,down,line,tent);   // Horizontal bands, for the slope along v.
    filter(height,broad,binomial,binomial);
    for(size_t i=0;i<count;++i)grain[i]=std::fabs(height[i]-broad[i]);
    filter(grain,rough,binomial,binomial);
    // Faint seams: some artwork paints its seams only a few palette steps
    // off the surface (FLAT18, COMPSPAN), so they barely tilt. Where the
    // strongest bands (99th percentile) clearly stand out from the typical
    // texel, they are deepened toward seamDepth; grain and organic artwork
    // without straight seams keep their relief. Only bands well above the
    // typical one deepen, so averaged speckle inside a slab stays shallow
    // instead of turning into streaks.
    std::vector<float> bands(count);
    for(int y=0;y<h;++y)for(int x=0;x<w;++x)
        bands[(size_t)y*w+x]=std::hypot(at(across,x+1,y)-at(across,x-1,y),at(down,x,y+1)-at(down,x,y-1))*1.5f;
    std::vector<float> ranked=bands;
    std::nth_element(ranked.begin(),ranked.begin()+count/2,ranked.end());float typical=ranked[count/2];
    std::nth_element(ranked.begin(),ranked.begin()+count*99/100,ranked.end());float strongest=ranked[count*99/100];
    const float seamDepth=0.35f,deepest=3.0f,standout=4.0f;
    float gain=strongest>0&&strongest>=standout*typical?std::clamp(seamDepth/strongest,1.0f,deepest):1.0f;
    std::vector<byte> cells(count*4);
    for(int y=0;y<h;++y)for(int x=0;x<w;++x) {
        float fx=(at(height,x+1,y-1)+2*at(height,x+1,y)+at(height,x+1,y+1)-at(height,x-1,y-1)-2*at(height,x-1,y)-at(height,x-1,y+1))*0.25f;
        float fy=(at(height,x-1,y+1)+2*at(height,x,y+1)+at(height,x+1,y+1)-at(height,x-1,y-1)-2*at(height,x,y-1)-at(height,x+1,y-1))*0.25f;
        float t=std::clamp((bands[(size_t)y*w+x]-2*typical)/(2*typical+1e-6f),0.0f,1.0f);
        float deepen=1+(gain-1)*t*t*(3-2*t);
        float fine=smooth?0.125f:0.25f;
        float sx=fine*fx+1.5f*deepen*depth*(at(across,x+1,y)-at(across,x-1,y));
        float sy=fine*fy+1.5f*deepen*depth*(at(down,x,y+1)-at(down,x,y-1));
        float soften=1/(1+std::sqrt(sx*sx+sy*sy)); // Steep edges ease toward 45 degrees.
        byte *cell=&cells[((size_t)y*w+x)*4];
        cell[0]=(byte)std::lround(128+127*sx*soften);cell[1]=(byte)std::lround(128+127*sy*soften);
        cell[2]=smooth?0:(byte)std::lround(std::clamp(rough[(size_t)y*w+x]/0.25f,0.0f,1.0f)*255);cell[3]=255;
    }
    std::vector<uint16_t> anchors=tiles?tiles(height,w,h):std::vector<uint16_t>();
    if(std::any_of(anchors.begin(),anchors.end(),[](uint16_t a){return a!=noPaintedTile;})) {
        std::vector<uint16_t> wide(count*4);
        for(size_t i=0;i<count;++i) {
            for(int c=0;c<3;++c)wide[i*4+c]=(uint16_t)(cells[i*4+c]*257);
            wide[i*4+3]=anchors[i];
        }
        image.relief=gpuCreateTexture(GpuFormat::RGBA16Uint,w,h,wide.data(),w*8);
    } else image.relief=gpuCreateTexture(GpuFormat::RGBA8,w,h,cells.data(),w*4);
    if(!image.relief)I_Error((char*)"Could not allocate a GPU texture");
    return image;
}
// Leaded stained glass (Image::leaded): its relief in place of withRelief's.
// The glowing panes are the shards, split by the dark lead between them
// (4-connected groups of glowing texels): each shard is flat glass leaning a
// little its own way, by a hash of its first texel, so a light glints off
// some shards and not their neighbors. The lead is a groove: its texels
// slope down from the panes on either side, so its edges catch the light.
// RGBA8 as withRelief's (no tiles); B (grain) 0: glass is smooth.
static Image &withLeadedRelief(Image &image) {
    int w=image.width,h=image.height;size_t count=(size_t)w*h;
    const std::vector<byte> &mask=image.emission;
    std::vector<int> shard(count,-1);std::vector<size_t> stack;
    auto pane=[&](int x,int y){x=(x%w+w)%w;y=(y%h+h)%h;return !mask.empty()&&mask[(size_t)y*w+x]>0;};
    for(size_t start=0;start<count;++start) {
        if(shard[start]>=0||mask.empty()||!mask[start])continue;
        shard[start]=(int)start;stack.assign(1,start);
        while(!stack.empty()) {
            size_t i=stack.back();stack.pop_back();int x=(int)(i%w),y=(int)(i/w);
            const int steps[4][2]={{1,0},{-1,0},{0,1},{0,-1}};
            for(const auto &s:steps) {
                int nx=x+s[0],ny=y+s[1];if(nx<0||ny<0||nx>=w||ny>=h)continue;
                size_t j=(size_t)ny*w+nx;
                if(shard[j]<0&&mask[j]) {shard[j]=(int)start;stack.push_back(j);}
            }
        }
    }
    std::vector<byte> cells(count*4);
    for(int y=0;y<h;++y)for(int x=0;x<w;++x) {
        size_t i=(size_t)y*w+x;float sx=0,sy=0;
        if(shard[i]>=0) {
            uint32_t hash=(uint32_t)shard[i]*2654435761u;hash^=hash>>13;hash*=0x5bd1e995u;hash^=hash>>15;
            sx=((hash&1023)/1023.0f*2-1)*0.18f;sy=(((hash>>10)&1023)/1023.0f*2-1)*0.18f;
        } else {
            // Down from the panes: the slope points away from them.
            sx=0.45f*((float)pane(x-1,y)-(float)pane(x+1,y));sy=0.45f*((float)pane(x,y-1)-(float)pane(x,y+1));
        }
        byte *cell=&cells[i*4];
        cell[0]=(byte)std::lround(128+127*std::clamp(sx,-1.0f,1.0f));cell[1]=(byte)std::lround(128+127*std::clamp(sy,-1.0f,1.0f));
        cell[2]=0;cell[3]=255;
    }
    image.relief=gpuCreateTexture(GpuFormat::RGBA8,w,h,cells.data(),w*4);
    if(!image.relief)I_Error((char*)"Could not allocate a GPU texture");
    return image;
}
// Masks live beside graphics.cfg, or in the directory selected by -emissive.
static std::vector<byte> emissionMask(const char *name,bool flat,int w,int h,const byte *pixels) {
    std::vector<byte> mask((size_t)w*h,0);
    int paletteLump=W_GetNumForName((char*)"PLAYPAL");
    if(W_LumpLength(paletteLump)<768) I_Error((char*)"Invalid emissive palette");
    const byte *palette=(const byte*)W_CacheLumpNum(paletteLump,PU_CACHE);
    int kind=Game_GlowKind(name,flat);
    for(size_t i=0;i<mask.size();++i)
        mask[i]=doom_emissive_pixel(kind,palette+pixels[2*i]*3,pixels[2*i+1]);
    if(!flat && doom_switch_texture(name)) {
        char companion[9];strcpy(companion,name);
        companion[2]=name[2]=='1'?'2':'1';
        int other=R_CheckTextureNumForName(companion),ow=0,oh=0;
        if(other>=0)R_TextureDimensions(other,&ow,&oh);
        if(ow==w && oh==h) {
            std::vector<byte> paired((size_t)w*h*2);
            R_CopyIndexedTexture(other,paired.data());
            for(size_t i=0;i<mask.size();++i)
                mask[i]=pixels[2*i+1]&&paired[2*i+1]?
                    doom_switch_pixel(palette+pixels[2*i]*3,palette+paired[2*i]*3):0;
        }
    }
    std::string root=graphicsConfig;
    size_t slash=root.find_last_of("/\\");
    root=slash==std::string::npos?"Emissive":root.substr(0,slash+1)+"Emissive";
    int option=M_CheckParm((char*)"-emissive");
    if(option && option+1<myargc) root=myargv[option+1];
    while(root.size()>1&&(root.back()=='/'||root.back()=='\\'))root.pop_back();
    std::string file=root+(flat?"/flats/":"/walls/")+name+".png";
    if(FILE *probe=fopen(file.c_str(),"rb")) {
        fclose(probe);
        std::vector<float> values;
        if(!platformReadMask(file.c_str(),w,h,values)) {
            fprintf(stderr,"Emissive mask must be %dx%d: %s\n",w,h,file.c_str());
        } else {
            for(size_t i=0;i<mask.size();++i)
                mask[i]=pixels[i*2+1]?(byte)std::clamp(values[i]*255.0f,0.0f,255.0f):0;
            fprintf(stderr,"Loaded emissive mask: %s\n",file.c_str());
        }
    }
    return mask;
}
// Sprite lumps a living monster shows in its ordinary (not fullbright)
// frames: those its spawn, chase, pain and attack states lead through, read
// from the state table so DEHACKED monsters count. Death frames never get eyes.
static bool livingMonsterLump(int lump) {
    static std::vector<char> living;
    if(living.empty()) {
        living.assign(numlumps,0);
        for(const mobjinfo_t &info:mobjinfo) {
            if(!(info.flags&MF_COUNTKILL))continue;
            for(int start:{info.spawnstate,info.seestate,info.painstate,info.meleestate,info.missilestate})
                for(int s=start,steps=0;s>0&&s<NUMSTATES&&steps<64;s=states[s].nextstate,++steps) {
                    const state_t &state=states[s];int frame=state.frame&FF_FRAMEMASK;
                    if((state.frame&FF_FULLBRIGHT)||state.sprite<0||state.sprite>=numsprites||frame>=sprites[state.sprite].numframes)continue;
                    for(short rotation:sprites[state.sprite].spriteframes[frame].lump)
                        if(rotation>=0&&firstspritelump+rotation<numlumps)living[firstspritelump+rotation]=1;
                }
        }
    }
    return lump>=0&&lump<(int)living.size()&&living[lump];
}
// The general 0.25 emission-weight floor supports small real fixtures,
// but is too strong for tiny switch LEDs and key-door strips. Restrict
// those emitters using their actual glowing texel coverage. Their pixel
// emission remains unchanged in the shader; only cast-light power drops.
// Existing low-coverage computer-panel handling is preserved. Stained
// glass glows softly as a window, not a strong colored point light.
static Image &speckledPanel(Image &image,const char *name,bool flat) {
    int kind=Game_GlowKind(name,flat);
    if(image.emissionCoverage>0&&image.emissionCoverage<0.006f&&kind==DOOM_EMISSIVE_PANEL)
        image.emissionWeight=image.emissionCoverage*8.0f;
    if(!flat&&image.emissionCoverage>0) {
        if(doom_switch_texture(name))
            image.emissionWeight=std::min(image.emissionWeight,image.emissionCoverage*12.0f);
        else if(!strncmp(name,"DOORBLU",7)||!strncmp(name,"DOORRED",7)||!strncmp(name,"DOORYEL",7))
            image.emissionWeight=std::min(image.emissionWeight,
                std::clamp(image.emissionCoverage*5.0f,.03f,.45f));
    }
    if(kind==DOOM_EMISSIVE_GLASS) {image.emissionWeight*=0.2f;image.leaded=!image.emission.empty();}
    if(kind==DOOM_EMISSIVE_DIFFUSE)image.emissionWeight*=0.5f;   // diffuser panels: a dim, broad light
    return image;
}
// Classify once when a wall or flat is loaded. WAD names are eight bytes,
// NUL padded, and compared without ASCII case distinctions.
struct LivingMaterial { unsigned profile=LS_NONE,variant=0; };
static LivingMaterial livingSurfaceProfile(const char *name) {
    static constexpr struct { const char *name; LivingMaterial material; } profiles[]={
        {"HELL5_1",{LS_HELL_FLESH}}, {"HELL5_2",{LS_HELL_FLESH}},
        {"SPINE3_1",{LS_SPINE_WALL}}, {"SPINE3_2",{LS_SPINE_WALL}}, {"SPINE4_1",{LS_SPINE_WALL}},
        {"BODIES",{LS_BODY_MASS}}, {"SFLR6_4",{LS_TENTACLE_MEAT}},
        {"W98_1",{LS_DRY_SKIN}}, {"W98_2",{LS_DRY_SKIN}}, {"W99_1",{LS_DRY_SKIN}}, {"W99_2",{LS_DRY_SKIN}},
        {"W100_1",{LS_DRY_SKIN}}, {"W100_2",{LS_DRY_SKIN}}, {"W101_1",{LS_DRY_SKIN}}, {"W101_2",{LS_DRY_SKIN}},
        {"W103_1",{LS_DRY_SKIN}}, {"W103_2",{LS_DRY_SKIN}},
        {"W102_1",{LS_DRY_SKIN,1}}, {"W102_2",{LS_DRY_SKIN,1}},
        {"WALL25_1",{LS_TORTURED_FACES}},
        {"MWALL4_1",{LS_MARBLE_FACES}}, {"MWALL4_2",{LS_MARBLE_FACES,1}}, {"MWALL5_1",{LS_MARBLE_FACES,2}},
        {"BODY_1",{LS_PINNED_BODY}}, {"BODY_2",{LS_PINNED_BODY,1}},
        {"WALL50_1",{LS_PINNED_BODY,2}}, {"WALL50_2",{LS_PINNED_BODY,3}},
        {"WALL51_2",{LS_PINNED_BODY,4}}, {"WALL51_3",{LS_PINNED_BODY,5}},
        {"GATE1",{LS_TELEPORT}}, {"GATE2",{LS_TELEPORT}}, {"GATE3",{LS_TELEPORT}}, {"GATE4",{LS_TELEPORT,1}},
        {"RP2_1",{LS_BLOOD_FLOW}}, {"RP2_2",{LS_BLOOD_FLOW}}, {"RP2_3",{LS_BLOOD_FLOW}}, {"RP2_4",{LS_BLOOD_FLOW}},
        {"WALL58_1",{LS_BLOOD_FLOW,1}}, {"WALL58_2",{LS_BLOOD_FLOW,1}}, {"WALL58_3",{LS_BLOOD_FLOW,1}},
        {"SW2_5",{LS_EYE_SWITCH}}, {"SW2_6",{LS_EYE_SWITCH,1}},
        {"SW2_7",{LS_SKULL_SWITCH}}, {"SW2_8",{LS_SKULL_SWITCH,1}},
        {"SW2_1",{LS_SKULL_SWITCH,2}}, {"SW2_2",{LS_SKULL_SWITCH,3}}, {"SW2_4",{LS_SKULL_SWITCH,4}},
        {"SNAK7_1",{LS_INTESTINES}}, {"SNAK8_1",{LS_INTESTINES,1}},
        {"W65B_1",{LS_FIRE,1}}, {"W65B_2",{LS_FIRE,1}},
        {"W73A_1",{LS_FIRE}}, {"W73A_2",{LS_FIRE}}, {"W73B_1",{LS_FIRE}}, {"W73B_2",{LS_FIRE}},
        {"W74A_1",{LS_FIRE}}, {"W74A_2",{LS_FIRE}}, {"W74B_1",{LS_FIRE}},
        {"WALL22_1",{LS_FIRE}}, {"WALL22_2",{LS_FIRE}}, {"WALL23_1",{LS_FIRE}}, {"WALL23_2",{LS_FIRE}},
        {"RSKY1",{LS_SKY,1}}, {"RSKY2",{LS_SKY,2}}, {"RSKY3",{LS_SKY,3}}
    };
    char canonical[9]={};
    for(int i=0;i<8&&name[i];++i)
        canonical[i]=name[i]>='a'&&name[i]<='z'?char(name[i]-'a'+'A'):name[i];
    for(const auto &entry:profiles)
        if(strcmp(canonical,entry.name)==0)return entry.material;
    return {};
}
// The new eye landmarks use texel coordinates on the complete patch.
static bool livingArtworkFits(LivingMaterial material,int width,int height) {
    if(material.profile==LS_MARBLE_FACES)return width==128&&height==128;
    if(material.profile==LS_INTESTINES)return width==64&&height==128;
    if(material.profile!=LS_SKULL_SWITCH)return true;
    if(material.variant<2)return width==36&&height==38;
    if(material.variant<4)return width==22&&height==26;
    return width==53&&height==41;
}
// Adjacent patches with the same material can animate as one wall. Leave
// unrelated or ambiguous overlapping composites on the normal path.
static void setLivingWallProfile(Image &image,int index,const char *name) {
    LivingMaterial material=livingSurfaceProfile(name);
    if(material.profile!=LS_NONE&&livingArtworkFits(material,image.width,image.height)) {
        image.livingProfile=material.profile;image.livingVariant=material.variant;
        image.livingBounds={0,0,image.width,image.height};
        return;
    }
    char patchName[9];std::array<int,4> bounds;
    int selected=-1,joined=-1;
    for(int part=0;R_TexturePatchInfo(index,part,patchName,bounds.data());++part) {
        LivingMaterial candidate=livingSurfaceProfile(patchName);
        if(candidate.profile==LS_NONE||bounds[0]>=bounds[2]||bounds[1]>=bounds[3]||
           !livingArtworkFits(candidate,bounds[2]-bounds[0],bounds[3]-bounds[1]))continue;
        if(selected<0) {
            selected=part;material=candidate;image.livingBounds=bounds;
            continue;
        }
        auto &region=image.livingBounds;
        // WOOD4 stacks the compact lit skull over the dormant skull.
        if(joined<0&&material.profile==LS_SKULL_SWITCH&&material.variant==3&&
           candidate.profile==LS_SKULL_SWITCH&&candidate.variant==2&&
           region[0]==bounds[0]&&region[2]==bounds[2]&&bounds[1]-region[1]==58) {
            joined=part;material.variant=5;region[3]=bounds[3];
            continue;
        }
        if(joined>=0||material.profile!=candidate.profile||material.variant!=candidate.variant||
           bounds[1]!=region[1]||bounds[3]!=region[3]||
           (bounds[0]!=region[2]&&bounds[2]!=region[0]))return;
        joined=part;
        region[0]=std::min(region[0],bounds[0]);
        region[2]=std::max(region[2],bounds[2]);
    }
    if(selected<0)return;
    const auto &region=image.livingBounds;
    for(int part=0;R_TexturePatchInfo(index,part,patchName,bounds.data());++part) {
        // The stock eye switch is painted over its flesh backing. Its shader
        // mask stays inside the eye, where the switch patch is opaque.
        if((material.profile==LS_EYE_SWITCH||material.profile==LS_SKULL_SWITCH)&&part<selected)continue;
        if(part!=selected&&part!=joined&&bounds[0]<region[2]&&bounds[2]>region[0]&&
           bounds[1]<region[3]&&bounds[3]>region[1])return;
    }
    image.livingProfile=material.profile;image.livingVariant=material.variant;
}
#include "sky_depth.inc"

// These two fingerprints identify a single immutable indexed source. Reuse
// the CPU profile when the map reloads and the Image resources are rebuilt.
static SkyAtmosphereProfile stormAtmosphere;
static bool stormAtmosphereAnalyzed=false;

const SkyAtmosphereProfile *skyAtmosphereProfile(const Image &sky) {
    if(sky.skyArtwork!=SkyArtwork::BrownClouds||sky.skyPalette!=stockSkyPalette||
       stormAtmosphere.kind==SkyAtmosphereKind::Legacy)return nullptr;
    return &stormAtmosphere;
}

/* Texture keys: wall index >=0, WAD lump = -1-lump. */
Image &wallImage(int index) {
    auto found=images.find(index); if(found!=images.end()) return found->second;
    loadingTick();
    int w,h; R_TextureDimensions(index,&w,&h);
    if(w<1||h<1||w>8192||h>8192) I_Error((char*)"Unsupported texture dimensions");
    std::vector<byte> pixels((size_t)w*h*2); R_CopyIndexedTexture(index,pixels.data());
    char name[9]; R_TextureName(index,name);
    auto mask=emissionMask(name,false,w,h,pixels.data());
    bool glowing=std::any_of(mask.begin(),mask.end(),[](byte p){return p!=0;});
    std::vector<byte> frame;auto glass=screenGlass(name,false,w,h,pixels.data(),frame);
    // Masked walls (bars, fences) show what lies behind instead of tiles;
    // animated ones would change tiles every frame.
    int next;bool animated=P_PicAnimationNext(true,index,&next)>0;
    Image &image=speckledPanel(images.emplace(index,upload(w,h,pixels.data(),0,0,glowing?mask.data():nullptr,nullptr,
        glass.empty()?nullptr:glass.data(),true)).first->second,name,false);
    image.switchIndicator=doom_switch_texture(name);
    image.skyArtwork=classifySkyArtwork(image,index,name);
    if(image.skyArtwork!=SkyArtwork::Unknown) {
        const int lump=W_GetNumForName((char*)"PLAYPAL");
        if(W_LumpLength(lump)>=768) {
            const auto *palette=(const byte*)W_CacheLumpNum(lump,PU_CACHE);
            image.skyPalette=skyFingerprint(palette,768);
            if(gamemode==commercial&&image.skyArtwork==SkyArtwork::BrownClouds&&
               image.skyPalette==stockSkyPalette&&!stormAtmosphereAnalyzed) {
                stormAtmosphere=analyzeSkyAtmosphere(image.pixels.data(),w,h,palette,SkyAtmosphereKind::DuskStorm);
                stormAtmosphereAnalyzed=true;
            }
        }
    }
    if(settings.doom2DeepSky&&settings.immersiveSky>0&&gamemode==commercial) {
        if(image.skyArtwork==SkyArtwork::BurningCity&&image.skyPalette==stockSkyPalette)
            skyDepthScene(image);
        else if(devparm&&SDL_strcasecmp(name,"SKY2")==0) {
            const char *reason=image.skyArtwork==SkyArtwork::BurningCity?"custom PLAYPAL":"unrecognized artwork";
            fprintf(stderr,"Sky depth: SKY2 fallback (%s).\n",reason);
        }
    }
    setLivingWallProfile(image,index,name);
    if(image.leaded)return withLeadedRelief(withGlassFrame(image,frame));
    return withRelief(withGlassFrame(image,frame),image.opaque&&!animated?wallTiles:nullptr);
}
Image &lumpImage(int lump,bool flat) {
    int key=-1-lump; auto found=images.find(key); if(found!=images.end()) return found->second;
    loadingTick();
    int size=W_LumpLength(lump);
    const byte *data=(const byte*)W_CacheLumpNum(lump,PU_CACHE);
    if(flat) {
        if(size<4096) I_Error((char*)"Invalid floor texture");
        std::vector<byte> pixels(64*64*2);
        for(int i=0;i<4096;++i) {pixels[2*i]=data[i];pixels[2*i+1]=255;}
        char name[9];memcpy(name,lumpinfo[lump].name,8);name[8]=0;
        auto mask=emissionMask(name,true,64,64,pixels.data());
        bool glowing=std::any_of(mask.begin(),mask.end(),[](byte p){return p!=0;});
        std::vector<byte> frame;auto glass=screenGlass(name,true,64,64,pixels.data(),frame);
        // Animated flats (liquids, lights) change their tiles every frame.
        int next;bool animated=P_PicAnimationNext(false,lump-firstflat,&next)>0;
        // Ice: its painted streaks cut grooves twice as deep, in a smooth,
        // glassy surface; no crooked tiles.
        if(Game_IceFlat(name))
            return withRelief(images.emplace(key,upload(64,64,pixels.data(),0,0,nullptr,nullptr,nullptr,true)).first->second,nullptr,2.0f,true);
        Image &result=withRelief(withGlassFrame(speckledPanel(images.emplace(key,upload(64,64,pixels.data(),0,0,glowing?mask.data():nullptr,nullptr,
            glass.empty()?nullptr:glass.data(),true)).first->second,name,true),frame),animated?nullptr:flatTiles);
        int kind=doom_emissive_kind(name,true);
        LivingMaterial material=livingSurfaceProfile(name);
        result.livingProfile=material.profile;result.livingVariant=material.variant;
        if(result.livingProfile!=LS_NONE)result.livingBounds={0,0,64,64};
        result.liquidEmitter=kind==DOOM_EMISSIVE_LAVA||kind==DOOM_EMISSIVE_SLIME;
        return result;
    }
    if(size<8) I_Error((char*)"Invalid sprite texture");
    const patch_t *patch=(const patch_t*)data;
    int w=SHORT(patch->width),h=SHORT(patch->height);
    if(w<1||h<1||w>8192||h>8192||8+(size_t)w*4>(size_t)size)
        I_Error((char*)"Invalid sprite dimensions");
    std::vector<byte> pixels((size_t)w*h*2);
    for(int x=0;x<w;++x) {
        size_t pos=(uint32_t)LONG(patch->columnofs[x]); int previous=-1;
        while(pos<(size_t)size && data[pos]!=255) {
            if(pos+4>(size_t)size||pos+4+data[pos+1]>(size_t)size) I_Error((char*)"Invalid sprite column");
            int top=data[pos]; if(top<=previous) top+=previous; previous=top;
            for(int y=0;y<data[pos+1];++y) if(top+y>=0&&top+y<h) {
                size_t pixel=((size_t)(top+y)*w+x)*2;
                pixels[pixel]=data[pos+3+y]; pixels[pixel+1]=255;
            }
            pos+=data[pos+1]+4;
        }
    }
    int paletteLump=W_GetNumForName((char*)"PLAYPAL");
    if(W_LumpLength(paletteLump)<768) I_Error((char*)"Invalid lighting palette");
    const byte *palette=(const byte*)W_CacheLumpNum(paletteLump,PU_CACHE);
    auto normals=spriteNormals(w,h,pixels.data());
    // Glowing eyes take normals no silhouette has: x all the way right
    // and y over halfway down, the glow in y's top quarter (eyeGlow).
    // Stock monsters' eyes are authored (sprite_eyes_stock.h); other
    // artwork is searched for them (sprite_eyes.h).
    if(livingMonsterLump(lump)) {
        char name[9];memcpy(name,lumpinfo[lump].name,8);name[8]=0;
        std::vector<byte> eyes;
        if(!doom_stock_eyes(name,w,h,pixels.data(),eyes))eyes=doom_sprite_eyes(w,h,pixels.data(),palette);
        for(size_t i=0;i<eyes.size();++i)if(eyes[i]) {normals[2*i]=255;normals[2*i+1]=(byte)(192+eyes[i]*63/255);}
    }
    Image image=upload(w,h,pixels.data(),SHORT(patch->leftoffset),SHORT(patch->topoffset),nullptr,normals.data());
    image.glowWeight=doom_sprite_glow(pixels.data(),(size_t)w*h,palette,image.glow.data());
    image.glowHeight=doom_sprite_glow_height(pixels.data(),w,h,palette,image.glow.data());
    return images.emplace(key,std::move(image)).first->second;
}
Image &playerColourImage(Image &source,int sourceKey,int color,int &key,const byte *voxelNormals) {
    if(!color)return source;
    key=-20000000+sourceKey*4-color;
    auto found=images.find(key);if(found!=images.end())return found->second;
    auto pixels=source.pixels;
    const int ramps[]={0x70,0x60,0x40,0x20};
    for(size_t i=0;i<pixels.size();i+=2)if(pixels[i]>=0x70&&pixels[i]<=0x7f)pixels[i]=ramps[color&3]+(pixels[i]&15);
    auto normals=voxelNormals?std::vector<byte>():spriteNormals(source.width,source.height,pixels.data());
    return images.emplace(key,upload(source.width,source.height,pixels.data(),source.left,source.top,nullptr,voxelNormals?voxelNormals:normals.data())).first->second;
}
// Immutable derived artwork, selected per actor. Original pixels and alpha stay intact.
Image &charredImage(Image &source,int sourceKey,int &key) {
    key=-400000000+sourceKey;
    auto found=images.find(key);if(found!=images.end())return found->second;
    const byte *palette=(const byte*)W_CacheLumpName((char*)"PLAYPAL",PU_CACHE);
    byte lut[256];
    for(int c=0;c<256;++c){
        int grey=(palette[c*3]*30+palette[c*3+1]*59+palette[c*3+2]*11)*23/10000;
        int best=0,distance=INT_MAX;
        for(int n=0;n<256;++n){int d=0;for(int k=0;k<3;++k){int v=palette[n*3+k]-grey;d+=v*v;}if(d<distance){distance=d;best=n;}}
        lut[c]=(byte)best;
    }
    auto pixels=source.pixels;for(size_t i=0;i+1<pixels.size();i+=2)pixels[i]=lut[pixels[i]];
    auto normals=spriteNormals(source.width,source.height,pixels.data());
    return images.emplace(key,upload(source.width,source.height,pixels.data(),source.left,source.top,nullptr,normals.data())).first->second;
}
const std::array<float,3> &averageColor(Image &image) {
    if(image.averaged)return image.average;
    const byte *palette=(const byte*)W_CacheLumpName((char*)"PLAYPAL",PU_CACHE);
    double sum[3]={},count=0;
    for(size_t i=0;i+1<image.pixels.size();i+=2) if(image.pixels[i+1]) {
        for(int c=0;c<3;++c)sum[c]+=palette[image.pixels[i]*3+c]/255.0;
        ++count;
    }
    for(int c=0;c<3;++c)image.average[c]=count?(float)(sum[c]/count):0.5f;
    image.averaged=true;return image.average;
}
// Particles are chunky: opaque blocks of whole Doom pixels in WAD palette
// colors, taken from random texels of the art they come from.
std::array<float,3> paletteColor(const std::array<float,3> &color) {
    static std::vector<int16_t> nearest;
    if(nearest.empty())nearest.assign(32*32*32,-1);
    int cell[3];for(int c=0;c<3;++c)cell[c]=std::clamp((int)std::lround(color[c]*31),0,31);
    int16_t &index=nearest[(cell[0]*32+cell[1])*32+cell[2]];
    const byte *palette=(const byte*)W_CacheLumpName((char*)"PLAYPAL",PU_CACHE);
    if(index<0) {
        int best=1<<30;
        for(int i=0;i<256;++i) {
            int d=0;for(int c=0;c<3;++c){int e=palette[i*3+c]-cell[c]*255/31;d+=e*e;}
            if(d<best){best=d;index=(int16_t)i;}
        }
    }
    return {palette[index*3]/255.0f,palette[index*3+1]/255.0f,palette[index*3+2]/255.0f};
}
// The brightest opaque texel's color.
const std::array<float,3> &brightestColor(Image &image) {
    if(image.brightened)return image.brightest;
    const byte *palette=(const byte*)W_CacheLumpName((char*)"PLAYPAL",PU_CACHE);
    int best=-1,bestSum=-1;
    for(size_t i=0;i+1<image.pixels.size();i+=2) if(image.pixels[i+1]) {
        int index=image.pixels[i],sum=palette[index*3]+palette[index*3+1]+palette[index*3+2];
        if(sum>bestSum){bestSum=sum;best=index;}
    }
    image.brightest=best<0?averageColor(image):std::array<float,3>{palette[best*3]/255.0f,palette[best*3+1]/255.0f,palette[best*3+2]/255.0f};
    image.brightened=true;return image.brightest;
}
std::array<float,3> texelColor(Image &image) {
    const byte *palette=(const byte*)W_CacheLumpName((char*)"PLAYPAL",PU_CACHE);
    size_t count=image.pixels.size()/2;
    for(int tries=0;count&&tries<8;++tries) {
        size_t i=std::min(count-1,(size_t)randomRange(0,(float)count));
        if(image.pixels[2*i+1])return {palette[image.pixels[2*i]*3]/255.0f,palette[image.pixels[2*i]*3+1]/255.0f,palette[image.pixels[2*i]*3+2]/255.0f};
    }
    return averageColor(image);
}
int translucentWallImage(int index) {
    int key=0x20000000+index;if(images.find(key)!=images.end())return key;
    const Image &source=wallImage(index);auto pixels=source.pixels;
    for(size_t i=1;i<pixels.size();i+=2)pixels[i]=(byte)((pixels[i]+1)/2);
    Image result=upload(source.width,source.height,pixels.data(),0,0,
        source.emission.empty()?nullptr:source.emission.data(),nullptr,nullptr,true);
    images.emplace(key,std::move(result));return key;
}
namespace {
constexpr size_t maxSkinPixelsBytes=32u*1024u*1024u;
size_t skinPixelsBytes=0;
std::unordered_map<int,bool> stockSkinArtwork;
std::unordered_set<int> rejectedSkinVariants;
}
void monsterSkinReset() {skinPixelsBytes=0;stockSkinArtwork.clear();rejectedSkinVariants.clear();}

// Three reusable tiers, cached per stock sprite LUMP and health band; never
// per-monster textures. Uses original frame identity/regions; reject custom art.
Image &monsterSkinImage(int type,int lump,Image &source,int health,int spawnHealth,int &key) {
    if(health<=0||spawnHealth<=0||health>=spawnHealth||lump<0||lump>20000000)return source;
    // Preserve Prototype 1 exactly for the original four monster types.
    // New types are enabled only on verified stock artwork.
    doom_skin_regions::Kind added=doom_skin_regions::Kind::None;
    switch(type) {
    case MT_FATSO:added=doom_skin_regions::Kind::Mancubus;break;
    case MT_VILE:added=doom_skin_regions::Kind::Archvile;break;
    case MT_CYBORG:added=doom_skin_regions::Kind::Cyberdemon;break;
    case MT_HEAD:added=doom_skin_regions::Kind::Cacodemon;break;
    case MT_KNIGHT:added=doom_skin_regions::Kind::HellKnight;break;
    case MT_POSSESSED:added=doom_skin_regions::Kind::Zombieman;break;
    default:break;
    }
    const bool legacy=type==MT_TROOP||type==MT_BRUISER||type==MT_SERGEANT||type==MT_SHADOWS;
    if(!legacy && added==doom_skin_regions::Kind::None)return source;
    // Preserve pristine artwork above 75% health; stages 1/2/3 are
    // 51-75%, 26-50%, and 1-25% respectively.
    if((int64_t)health*4>(int64_t)spawnHealth*3)return source;
    int tier=(int64_t)health*2>spawnHealth?1:(int64_t)health*4>spawnHealth?2:3;
    // Keys do not intersect original WAD image keys or existing blood/charred art.
    int variantKey=-800000000-4*lump-tier;
    if(rejectedSkinVariants.count(variantKey))return source;
    auto found=images.find(variantKey);
    if(found!=images.end()) {key=variantKey;return found->second;}
    if(source.width<8||source.height<8||source.pixels.empty()||
       size_t(source.width)*source.height>maxSkinPixelsBytes/6)return source;
    auto known=stockSkinArtwork.find(lump);
    if(known!=stockSkinArtwork.end()&&!known->second)return source;
    // Fingerprint-verified stock artwork ONLY. No guessed annotations on PWAD skins.
    doom_regions::Mask regions=doom_regions::build(type,source.width,source.height,
        source.left,source.top,source.pixels.data());
    if(!regions.stock) {stockSkinArtwork[lump]=false;return source;}
    stockSkinArtwork[lump]=true;
    if(bloodPalette().empty())return source;
    const byte *palette=(const byte*)W_CacheLumpName((char*)"PLAYPAL",PU_CACHE);
    std::vector<byte> allowed((size_t)source.width*source.height,0);
    for(int y=0;y<source.height;++y)for(int x=0;x<source.width;++x) {
        size_t at=(size_t)y*source.width+x;
        if(!source.pixels[at*2+1]||regions.pixels[at]!=WP_BODY)continue;
        const byte *rgb=palette+source.pixels[at*2]*3;
        if(legacy) {
            // Approved Prototype 1 mask: intentionally byte-for-byte identical.
            float part=float(y+0.5f)/source.height;
            if(part<(type==MT_SERGEANT||type==MT_SHADOWS?0.43f:0.32f)||part>0.89f)continue;
            if(rgb[0]<28||rgb[0]<rgb[1]*1.055f||rgb[0]<rgb[2]*0.95f)continue;
        } else {
            // Cacodemon has no distinct head: avoid its central face and eye;
            // permit only orb flanks, except on the authored rear view.
            const bool cacoRear=type==MT_HEAD&&lumpinfo[lump].name[5]=='5';
            if(!doom_skin_regions::permit(added,x,y,source.width,source.height,
                                        source.pixels[at*2],rgb[0],rgb[1],rgb[2],cacoRear))continue;
        }
        allowed[at]=1;
    }
    if(std::none_of(allowed.begin(),allowed.end(),[](byte b){return b!=0;})) {
        stockSkinArtwork[lump]=false;return source;
    }
    size_t bytes=(size_t)source.width*source.height*6;
    if(bytes>maxSkinPixelsBytes-skinPixelsBytes)return source;
    auto pixels=source.pixels;
    const uint32_t seed=(uint32_t)type*0x79b97f4au;
    if(!doom_skin_damage::paint(pixels,source.pixels,allowed,source.width,source.height,tier,seed,
         monsterSkinBloodIndex(type,0.06f),monsterSkinBloodIndex(type,0.38f),monsterSkinBloodIndex(type,0.69f))) {
        rejectedSkinVariants.insert(variantKey);return source;
    }
    // Rebuild normals from SOURCE art, not the dark stains. Preserve stock
    // eye-glow flags which spriteNormals alone cannot reconstruct.
    auto normals=spriteNormals(source.width,source.height,source.pixels.data());
    if(livingMonsterLump(lump)) {
        char name[9];memcpy(name,lumpinfo[lump].name,8);name[8]=0;
        std::vector<byte> eyes;
        if(!doom_stock_eyes(name,source.width,source.height,source.pixels.data(),eyes))
            eyes=doom_sprite_eyes(source.width,source.height,source.pixels.data(),palette);
        for(size_t i=0;i<eyes.size();++i)if(eyes[i]) {normals[2*i]=255;normals[2*i+1]=(byte)(192+eyes[i]*63/255);}
    }
    Image variant=upload(source.width,source.height,pixels.data(),source.left,source.top,nullptr,normals.data());
    variant.glow=source.glow;variant.glowWeight=source.glowWeight;variant.glowHeight=source.glowHeight;
    auto inserted=images.emplace(variantKey,std::move(variant));
    if(!inserted.second)return source;
    skinPixelsBytes+=bytes;
    key=variantKey;
    return inserted.first->second;
}

namespace {
std::size_t decayTextureBytes=0;
std::unordered_set<int> rejectedDecayTextures;
}
void corpseDecayImageReset() {decayTextureBytes=0;rejectedDecayTextures.clear();}

// Each stage is a shared image, never an allocation per corpse. Work on the
// WAD's currently loaded pixels and PLAYPAL, leaving transparency unchanged.
Image &corpseDecayImage(int lump,Image &source,int stage,int family,int &key) {
    if(stage<1||stage>3||lump<0||lump>20000000||family<0||family>4)return source;
    // Distinct image keys for five palette families and three stages.
    // 18 slots per lump prevent cross-family cache aliasing.
    const int variantKey=-1200000000-18*lump-3*family-stage;
    // The lump bound above also keeps this negative key inside signed int.
    if(rejectedDecayTextures.count(variantKey))return source;
    auto found=images.find(variantKey);
    if(found!=images.end()){key=variantKey;return found->second;}
    if(source.width<1||source.height<1||source.width>256||source.height>256||
       source.pixels.size()!=std::size_t(source.width)*source.height*2) {
        if(rejectedDecayTextures.insert(variantKey).second && M_CheckParm((char*)"-decaydebug"))
            std::fprintf(stderr,"HELLFORGE DECAY: TEXTURE-SKIP lump=%d stage=%d dimensions=%dx%d (unsupported sprite)\n",
                         lump,stage,source.width,source.height);
        return source;
    }
    const std::size_t bytes=std::size_t(source.width)*source.height*6u;
    if(bytes>doom_decay::textureBudget-decayTextureBytes) {
        if(rejectedDecayTextures.insert(variantKey).second && M_CheckParm((char*)"-decaydebug"))
            std::fprintf(stderr,"HELLFORGE DECAY: TEXTURE-SKIP lump=%d stage=%d need=%zu remaining=%zu (budget exceeded)\n",
                         lump,stage,bytes,doom_decay::textureBudget-decayTextureBytes);
        return source;
    }
    const byte *palette=(const byte*)W_CacheLumpName((char*)"PLAYPAL",PU_CACHE);
    std::vector<byte> pixels;
    if(!doom_decay::paint(pixels,source.pixels,source.width,source.height,palette,stage,family,
                          doom_decay::mix32(unsigned(lump)*0x79b97f4au))) {
        rejectedDecayTextures.insert(variantKey);return source;
    }
    auto normals=spriteNormals(source.width,source.height,source.pixels.data());
    Image variant=upload(source.width,source.height,pixels.data(),source.left,source.top,
                         nullptr,normals.data());
    variant.glow=source.glow;variant.glowWeight=source.glowWeight;variant.glowHeight=source.glowHeight;
    auto added=images.emplace(variantKey,std::move(variant));
    if(!added.second)return source;
    decayTextureBytes+=bytes;
    if(M_CheckParm((char*)"-decaydebug"))
        std::fprintf(stderr,"HELLFORGE DECAY: TEXTURE built lump=%d stage=%d family=%d dimensions=%dx%d bytes=%zu used=%zu key=%d\n",
                     lump,stage,family,source.width,source.height,bytes,decayTextureBytes,variantKey);
    key=variantKey;return added.first->second;
}

void imagesShutdown() {monsterSkinReset();corpseDecayImageReset();images.clear();skyDepthCache={};cloudTexture=nullptr;}
} // namespace doom3d

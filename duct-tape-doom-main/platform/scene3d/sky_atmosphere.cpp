#include "sky_atmosphere.h"
#include <algorithm>
#include <cmath>
#include <cstddef>

namespace doom3d {

SkyAtmosphereProfile analyzeSkyAtmosphere(const uint8_t *indexed,int width,int height,
                                          const uint8_t *playpal,SkyAtmosphereKind kind) {
    return analyzeMaskedSkyAtmosphere(indexed,width,height,playpal,nullptr,kind);
}

SkyAtmosphereProfile analyzeMaskedSkyAtmosphere(const uint8_t *indexed,int width,int height,
                                                const uint8_t *playpal,const uint8_t *skyMask,
                                                SkyAtmosphereKind kind) {
    const bool city=kind==SkyAtmosphereKind::BurningCityDusk;
    const bool infernal=kind==SkyAtmosphereKind::InfernalAbyss;
    const bool masked=city||infernal;
    if((kind!=SkyAtmosphereKind::DuskStorm&&!masked)||!indexed||!playpal||width<=0||height<8||
       (masked&&!skyMask))return {};

    using RGB=std::array<float,3>;
    struct Tone {RGB rgb;float luminance;int count=0;};
    std::array<Tone,256> tones{};
    for(int i=0;i<256;++i) {
        auto &tone=tones[i];
        for(int channel=0;channel<3;++channel)tone.rgb[channel]=playpal[i*3+channel]/255.0f;
        tone.luminance=.299f*tone.rgb[0]+.587f*tone.rgb[1]+.114f*tone.rgb[2];
    }

    SkyAtmosphereProfile profile;
    const int bandRows=height/7;
    auto coveredPixel=[&](const uint8_t *row,int y,int x) {
        return row[2*x+1]==255&&(!skyMask||skyMask[std::size_t(y)*width+x]>=224);
    };
    int total=0,varied=0,top=0,bottom=0;
    RGB stone{};
    int stoneCount=0;
    for(int y=0;y<height;++y) {
        const auto *row=indexed+std::size_t(y)*width*2;
        int covered=0;
        float mean=0;
        for(int x=0;x<width;++x) {
            if(!coveredPixel(row,y,x)) {
                if(infernal&&y>=height/2&&row[2*x+1]==255) {
                    const auto &rgb=tones[row[2*x]].rgb;
                    if(rgb[0]-rgb[1]<.08f||rgb[1]*2.0f>rgb[0]) {
                        for(int c=0;c<3;++c)stone[c]+=rgb[c];
                        ++stoneCount;
                    }
                }
                continue;
            }
            auto &tone=tones[row[2*x]];
            ++tone.count;
            ++covered;
            mean+=tone.luminance;
            if(y<bandRows) {
                ++top;
                for(int c=0;c<3;++c)profile.zenith[c]+=tone.rgb[c];
            } else if(infernal ? y>=height*3/8
                              : city ? (y>=height*7/16&&tone.rgb[0]-tone.rgb[2]>.10f&&tone.luminance>.19f)
                           : y>=height-bandRows) {
                ++bottom;
                for(int c=0;c<3;++c)profile.horizon[c]+=tone.rgb[c];
            }
        }
        if(!covered)continue;
        total+=covered;
        mean/=covered;
        for(int x=0;x<width;++x) {
            if(!coveredPixel(row,y,x))continue;
            varied+=std::abs(tones[row[2*x]].luminance-mean)>.06f;
        }
    }
    if(!top||!bottom||double(total)<double(width)*height*(masked?.16:.95))return {};
    for(int c=0;c<3;++c) {
        profile.zenith[c]/=top;
        profile.horizon[c]/=bottom;
    }

    std::stable_sort(tones.begin(),tones.end(),[](const Tone &a,const Tone &b){
        return a.luminance<b.luminance;
    });
    // Use ranges around the 20th, 50th and 80th percentiles so palette
    // dithering and isolated highlights cannot dominate an entire band.
    auto percentileColor=[&](float low,float high) {
        RGB rgb{};
        const int first=int(low*total),end=std::max(first+1,int(high*total));
        int seen=0;
        for(const Tone &tone:tones) {
            const int count=std::max(0,std::min(seen+tone.count,end)-std::max(seen,first));
            for(int c=0;c<3;++c)rgb[c]+=tone.rgb[c]*count;
            seen+=tone.count;
        }
        for(float &channel:rgb)channel/=(end-first);
        return rgb;
    };
    profile.cloudDark=percentileColor(.08f,.30f);
    profile.cloudMiddle=percentileColor(.37f,.63f);
    profile.cloudLight=percentileColor(.70f,.91f);
    if((profile.cloudLight[0]-profile.cloudDark[0])<.07f)return {};
    profile.coverage=std::clamp(float(varied)/total,.25f,.85f);
    if(city) {
        // Grade the sampled palette toward charcoal overhead and fire-warmed
        // copper below. Preserve the measured tonal separation.
        constexpr RGB zenithGrade={.67f,.57f,.52f},horizonGrade={1.08f,.84f,.70f};
        constexpr RGB darkGrade={.37f,.31f,.28f},middleGrade={.82f,.66f,.57f},lightGrade={1.05f,.84f,.72f};
        for(int c=0;c<3;++c) {
            profile.zenith[c]*=zenithGrade[c];
            profile.horizon[c]*=horizonGrade[c];
            profile.cloudDark[c]*=darkGrade[c];
            profile.cloudMiddle[c]*=middleGrade[c];
            profile.cloudLight[c]*=lightGrade[c];
        }
        profile.nearAltitude=2500;
        profile.nearScale=8800;
        profile.farScale=1.12f;
        profile.nearOpacity=.91f;
        profile.farOpacity=.87f;
        profile.windSpeed=63;
    } else if(infernal) {
        // The painted wall uses pure red palette ramps. A little reflected
        // color from the sampled stone gives the air and glow distinct tones.
        constexpr RGB zenithGrade={.34f,.41f,.50f},horizonGrade={.69f,.67f,.80f};
        for(int c=0;c<3;++c) {
            const float bounce=stone[c]/std::max(1,stoneCount);
            profile.zenith[c]=profile.zenith[c]*zenithGrade[c]+.08f*bounce;
            profile.horizon[c]=profile.horizon[c]*horizonGrade[c]+.14f*bounce;
            profile.cloudDark[c]=profile.cloudDark[c]*.25f+.035f*bounce;
            profile.cloudMiddle[c]=profile.cloudMiddle[c]*.55f+.10f*bounce;
            profile.cloudLight[c]=profile.cloudLight[c]*.86f+.24f*bounce;
        }
        profile.coverage=std::clamp(profile.coverage+.23f,.54f,.76f);
        profile.nearAltitude=2450;
        profile.nearScale=12000;
        profile.farScale=.86f;
        profile.nearOpacity=.84f;
        profile.farOpacity=.80f;
        profile.windSpeed=29.f;
    }
    profile.kind=kind;
    return profile;
}

} // namespace doom3d

#include "sky_atmosphere.h"
#include <algorithm>
#include <cmath>
#include <cstddef>

namespace doom3d {

SkyAtmosphereProfile analyzeSkyAtmosphere(const uint8_t *indexed,int width,int height,
                                          const uint8_t *playpal,SkyAtmosphereKind kind) {
    if(kind!=SkyAtmosphereKind::DuskStorm||!indexed||!playpal||width<=0||height<8)return {};

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
    int total=0,varied=0,top=0,bottom=0;
    for(int y=0;y<height;++y) {
        const auto *row=indexed+std::size_t(y)*width*2;
        int covered=0;
        float mean=0;
        for(int x=0;x<width;++x) {
            if(row[2*x+1]!=255)continue;
            auto &tone=tones[row[2*x]];
            ++tone.count;
            ++covered;
            mean+=tone.luminance;
            if(y<bandRows) {
                ++top;
                for(int c=0;c<3;++c)profile.zenith[c]+=tone.rgb[c];
            } else if(y>=height-bandRows) {
                ++bottom;
                for(int c=0;c<3;++c)profile.horizon[c]+=tone.rgb[c];
            }
        }
        if(!covered)continue;
        total+=covered;
        mean/=covered;
        for(int x=0;x<width;++x) {
            if(row[2*x+1]!=255)continue;
            varied+=std::abs(tones[row[2*x]].luminance-mean)>.06f;
        }
    }
    if(!top||!bottom||double(total)<double(width)*height*.95)return {};
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
    profile.kind=kind;
    return profile;
}

} // namespace doom3d

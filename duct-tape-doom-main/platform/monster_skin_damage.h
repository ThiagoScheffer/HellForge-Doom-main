// HELLFORGE: bounded, deterministic damage paint on already-recognized monster art.
// No gameplay state, ray impacts, screen-space decals, or generated alpha pixels.
#ifndef HELLFORGE_MONSTER_SKIN_DAMAGE_H
#define HELLFORGE_MONSTER_SKIN_DAMAGE_H
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace doom_skin_damage {
inline uint32_t scramble(uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15;
    x *= 0x846ca68bu; x ^= x >> 16; return x;
}
inline float unit(uint32_t v) { return float(scramble(v) & 0x00ffffffu) / 16777216.0f; }
// Original/source is [palette-index, alpha] per pixel. The destination retains
// every source alpha byte. Allowed pixels are a stock-art body/skin mask.
inline bool paint(std::vector<uint8_t> &destination,
                  const std::vector<uint8_t> &original,
                  const std::vector<uint8_t> &allowed,
                  int width,int height,int tier,uint32_t seed,
                  uint8_t cutDark,uint8_t cutMid,uint8_t cutBright) {
    if(tier<1||tier>3||width<8||height<8||
       original.size()!=size_t(width)*height*2||
       destination.size()!=original.size()||
       allowed.size()!=size_t(width)*height)return false;
    const int marks=tier==1?4:tier==2?10:17;
    bool painted=false;
    auto eligible=[&](int x,int y)->bool {
        return x>=0&&y>=0&&x<width&&y<height &&
               allowed[size_t(y)*width+x]!=0 && original[(size_t(y)*width+x)*2+1]!=0;
    };
    auto apply=[&](int x,int y,uint8_t color) {
        if(!eligible(x,y))return;
        destination[(size_t(y)*width+x)*2]=color;
        painted=true;
    };
    for(int i=0;i<marks;++i) {
        // Same normalized anchors for every animation frame and damage tier.
        // Invalid positions are skipped rather than projected onto the head.
        uint32_t s=scramble(seed^uint32_t(i+1)*0x9e3779b9u);
        float u=0.16f+0.68f*unit(s^0x7a85u);
        float v=0.31f+0.49f*unit(s^0xb3c1u);
        int cx=std::min(width-1,int(u*width)),cy=std::min(height-1,int(v*height));
        // A local bounded search keeps marks on exposed opaque body artwork.
        // Avoid choosing distant body pieces when the view or pose changes.
        int bestX=-1,bestY=-1,bestDist=0x7fffffff;
        int reachX=std::max(2,width/11),reachY=std::max(2,height/13);
        for(int dy=-reachY;dy<=reachY;++dy)for(int dx=-reachX;dx<=reachX;++dx) {
            int x=cx+dx,y=cy+dy;
            if(!eligible(x,y))continue;
            int distance=dx*dx*4+dy*dy*4;
            if(distance<bestDist){bestDist=distance;bestX=x;bestY=y;}
        }
        if(bestX<0)continue;
        int x=bestX,y=bestY;
        // Three families: oblique thin cuts, small bloody tears with downward
        // seepage, and darker irregular stains; all made of original-size texels.
        int style=int(scramble(s^0x128f6dbdu)%3u);
        int length=3+int(scramble(s^0x991e1u)%5u)+(tier-1);
        int slant=(scramble(s^0x817u)&1u)?1:-1;
        if(style==0) {
            for(int k=-length/2;k<=length/2;++k) {
                int xx=x+slant*k/2,yy=y+k;
                apply(xx,yy,(k&3)?cutDark:cutMid);
                if(tier>=2 && (k%4)==0)apply(xx+1,yy,cutBright);
            }
        } else if(style==1) {
            int radius=tier>=3?3:2;
            for(int dy=-radius;dy<=radius;++dy)for(int dx=-radius;dx<=radius;++dx) {
                int d=dx*dx+dy*dy;
                if(d<=radius*radius && (d<2||unit(s^uint32_t(dx*217+dy*911))>0.26f))
                    apply(x+dx,y+dy,d<3?cutDark:cutMid);
            }
            for(int k=radius;k<radius+length;++k) {
                int xx=x+((k/4)%2?1:0);
                apply(xx,y+k,k>radius+length-3?cutDark:cutMid);
            }
        } else {
            for(int dy=-2;dy<=2+(tier>=3?1:0);++dy)
                for(int dx=-2;dx<=2;++dx)
                    if(dx*dx+dy*dy<8 && unit(s^uint32_t((dx+7)*197u+(dy+7)*653u))>0.38f)
                        apply(x+dx,y+dy,cutDark);
        }
    }
    return painted;
}
} // namespace doom_skin_damage
#endif

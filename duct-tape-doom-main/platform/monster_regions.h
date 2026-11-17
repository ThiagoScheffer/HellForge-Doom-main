/* Conservative anatomy masks. Stock-only face/eye/mechanical recipes use
   the existing per-frame artwork fingerprints and authored eye landmarks.
   Replacement humanoids may use a bounded silhouette estimate; other
   unrecognized replacements deliberately have no anatomical bonus. */
#ifndef DOOM_MONSTER_REGIONS_H
#define DOOM_MONSTER_REGIONS_H
#include "death_joints.h"
#include "sprite_eyes_stock.h"
#include "monster_regions_stock.h"
#include "monster_zombieman_stock.h"
#include <queue>
extern "C" {
#include "../linuxdoom-1.10/p_weakpoints.h"
}
namespace doom_regions {
struct Mask {std::vector<unsigned char> pixels,eyes;bool stock=false;};
inline Mask build(int type,int w,int h,int left,int top,const unsigned char *pixels) {
    Mask out;out.pixels.assign((size_t)w*h,WP_UNKNOWN);
    out.eyes.assign((size_t)w*h,0);
    if(!pixels||w<=0||h<=0)return out;
    unsigned hash=2166136261u;for(size_t i=0;i<(size_t)w*h*2;++i)hash=(hash^pixels[i])*16777619u;
    const char *family=nullptr;
    switch(type) {
    case MT_TROOP:family="TROO";break;case MT_UNDEAD:family="SKEL";break;case MT_VILE:family="VILE";break;
    case MT_KNIGHT:family="BOS2";break;case MT_BRUISER:family="BOSS";break;case MT_FATSO:family="FATT";break;
    case MT_CYBORG:family="CYBR";break;case MT_SERGEANT:case MT_SHADOWS:family="SARG";break;
    case MT_POSSESSED:family="POSS";break;
    case MT_HEAD:family="HEAD";break;case MT_PAIN:family="PAIN";break;case MT_BABY:family="BSPI";break;case MT_SPIDER:family="SPID";break;
    default:break;
    }
    const doom_stock_eye_entry *stock=nullptr;
    for(const auto &entry:doom_stock_eye_table)if(family&&!std::strncmp(entry.name,family,4)&&entry.width==w&&entry.height==h&&entry.hash==hash){stock=&entry;break;}
    const doom_region_stock_entry *extra=nullptr;
    for(const auto &entry:doom_region_stock_table)if(family&&!std::strncmp(entry.name,family,4)&&entry.width==w&&entry.height==h&&entry.hash==hash){extra=&entry;break;}
    // The existing stock anatomy tables do not carry POSS entries. An owned
    // IWAD can provide exact artwork fingerprints in a compact generated header;
    // an unverified Zombieman or replacement sprite fails closed.
    out.stock=stock!=nullptr||extra!=nullptr||
        (type==MT_POSSESSED&&doom_zombieman_stock::matches(w,h,hash));
    auto opaque=[&](int x,int y){return x>=0&&y>=0&&x<w&&y<h&&pixels[((size_t)y*w+x)*2+1]!=0;};
    for(int y=0;y<h;++y)for(int x=0;x<w;++x)if(opaque(x,y))out.pixels[(size_t)y*w+x]=WP_BODY;
    if(type==MT_VILE&&extra&&extra->mode!=2) {
        if(extra->mode==1)for(int y=std::max(0,extra->y0);y<std::min(h,extra->y1);++y)
            for(int x=std::max(0,extra->x0);x<std::min(w,extra->x1);++x)
                if(opaque(x,y)&&pixels[((size_t)y*w+x)*2]<160)out.pixels[(size_t)y*w+x]=WP_HEAD;
        return out;
    }
    std::vector<std::pair<int,int>> eyes;
    if(stock) {
        auto mask=doom_stock_eye_pixels(*stock,pixels);
        for(int y=0;y<h;++y)for(int x=0;x<w;++x)if(mask[(size_t)y*w+x])eyes.push_back({x,y});
    }
    if(eyes.empty()&&out.stock&&(type==MT_HEAD||type==MT_PAIN)) {
        // Fullbright attack frames were absent from the old glow-eye table.
        // On fingerprint-verified artwork the Cacodemon green iris and the
        // Pain Elemental's upper red eye ramp remain explicit landmarks.
        int eyeTop=h;
        for(int y=0;y<h*2/3;++y)for(int x=0;x<w;++x)if(opaque(x,y)) {
            unsigned c=pixels[((size_t)y*w+x)*2];
            if(type==MT_HEAD?(c>=112&&c<128):(c>=168&&c<192))eyeTop=std::min(eyeTop,y);
        }
        for(int y=eyeTop;y<std::min(h,eyeTop+9);++y)for(int x=0;x<w;++x)if(opaque(x,y)) {
            unsigned c=pixels[((size_t)y*w+x)*2];
            if(type==MT_HEAD?(c>=112&&c<128):(c>=168&&c<192))eyes.push_back({x,y});
        }
    }
    // An authored empty view is authoritative (rear/closed/hidden eyes).
    // Keep the legacy primary-region recipe, but never use its color fallback
    // to invent a new eye injury on a view explicitly authored without eyes.
    if((type==MT_HEAD||type==MT_PAIN||type==MT_SPIDER)&&(!stock||stock->pixels[0]))
        for(auto eye:eyes)for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx)
            if(opaque(eye.first+dx,eye.second+dy))out.eyes[(size_t)(eye.second+dy)*w+eye.first+dx]=255;
    if(type==MT_HEAD||type==MT_PAIN) {
        // Individual visible painted eye pixels and their immediate socket.
        // Back views and closed eyes have no landmarks and no eye region.
        for(auto eye:eyes)for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx)
            if(opaque(eye.first+dx,eye.second+dy))out.pixels[(size_t)(eye.second+dy)*w+eye.first+dx]=WP_EYE;
        return out;
    }
    if(type==MT_SERGEANT||type==MT_SHADOWS) {
        if(eyes.empty())return out;
        int x0=w,x1=0,y0=h,y1=0;
        for(auto e:eyes){x0=std::min(x0,e.first);x1=std::max(x1,e.first);y0=std::min(y0,e.second);y1=std::max(y1,e.second);}
        int pad=std::max(4,(x1-x0)/3),depth=std::clamp(x1-x0+8,10,22);
        for(int y=y0-1;y<=y1+depth;++y)for(int x=x0-pad;x<=x1+pad;++x)
            if(opaque(x,y))out.pixels[(size_t)y*w+x]=WP_FACE;
        return out;
    }
    if(type==MT_BABY||type==MT_SPIDER) {
        if(!out.stock)return out;
        for(size_t i=0;i<out.pixels.size();++i)if(out.pixels[i])out.pixels[i]=WP_CHASSIS;
        // Organic flesh uses the stock pink/tan/brown ramps. Select the
        // largest connected organic patch, excluding metallic gray pixels.
        std::vector<unsigned char> organic((size_t)w*h),seen((size_t)w*h);
        for(size_t i=0;i<organic.size();++i){unsigned c=pixels[i*2];organic[i]=pixels[i*2+1]&&((c>=16&&c<80)||(c>=128&&c<148));}
        std::vector<int> brain;
        for(int y=0;y<h;++y)for(int x=0;x<w;++x) {
            int index=y*w+x;if(!organic[index]||seen[index])continue;
            std::vector<int> component;std::queue<int> todo;todo.push(index);seen[index]=1;
            while(!todo.empty()) {
                int at=todo.front();todo.pop();component.push_back(at);
                for(auto d:{std::pair<int,int>{-1,0},{1,0},{0,-1},{0,1}}) {
                    int nx=at%w+d.first,ny=at/w+d.second;
                    if(nx<0||ny<0||nx>=w||ny>=h)continue;int n=ny*w+nx;
                    if(organic[n]&&!seen[n]){seen[n]=1;todo.push(n);}
                }
            }
            if(component.size()>brain.size())brain=std::move(component);
        }
        if(brain.size()<16)return out;
        int bx0=w,bx1=0,by0=h,by1=0;
        for(int at:brain){bx0=std::min(bx0,at%w);bx1=std::max(bx1,at%w);by0=std::min(by0,at/w);by1=std::max(by1,at/w);}
        // Reject a warm weapon/strut disconnected from the upper brain.
        if(by0>h/3||bx1-bx0<w/6)return out;
        for(int at:brain)out.pixels[at]=WP_BRAIN;
        // Trace exterior gray leg components upward from actual feet. The
        // organic body's sides and underside bound the search, so central
        // gun barrels and chassis never become legs just because they are low.
        std::fill(seen.begin(),seen.end(),0);std::queue<int> todo;
        auto legPixel=[&](int x,int y){if(!opaque(x,y)||y<=by1)return false;unsigned c=pixels[((size_t)y*w+x)*2];return (x<bx0||x>bx1)&&c>=80&&c<112;};
        for(int y=h-3;y<h;++y)for(int x=0;x<w;++x)if(legPixel(x,y)){seen[(size_t)y*w+x]=1;todo.push(y*w+x);}
        while(!todo.empty()) {
            int at=todo.front();todo.pop();out.pixels[at]=WP_LEGS;
            for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx) {
                int x=at%w+dx,y=at/w+dy;
                if(x<0||y<0||x>=w||y>=h||!legPixel(x,y))continue;int n=y*w+x;
                if(!seen[n]){seen[n]=1;todo.push(n);}
            }
        }
        return out;
    }
    doom_joints::Head head{};
    const char *stockName=stock?stock->name:extra?extra->name:"";
    if(out.stock&&type==MT_FATSO&&eyes.empty()&&stockName[5]>='4'&&stockName[5]<='6')return out;
    std::vector<unsigned char> unlit;
    const unsigned char *silhouette=pixels;
    if(type==MT_UNDEAD) {
        unlit.assign(pixels,pixels+(size_t)w*h*2);
        for(size_t i=0;i<(size_t)w*h;++i)if(unlit[i*2]>=160)unlit[i*2+1]=0; // muzzle flame, not skull
        silhouette=unlit.data();
    }
    bool found=doom_joints::findHead(silhouette,w,h,left,top,head);
    // Bound the silhouette estimate for replacements and narrow humanoids.
    // A weapon/shoulder silhouette wider than a plausible skull fails safe.
    int first=h;for(int y=0;y<h;++y)for(int x=0;x<w;++x)if(opaque(x,y)){first=std::min(first,y);break;}
    if(found&&(head.width>w*0.48f||(top-first-head.neck)>h*0.30f))found=false;
    if(found&&!out.stock&&std::abs(head.middle)>w*0.2f)found=false;
    float middle=head.middle+left,neck=top-head.neck,half=head.width/2;
    if(!eyes.empty()) {
        int ex0=w,ex1=0,ey0=h,ey1=0;
        for(auto e:eyes){ex0=std::min(ex0,e.first);ex1=std::max(ex1,e.first);ey0=std::min(ey0,e.second);ey1=std::max(ey1,e.second);}
        // Eye-anchored face caps keep heavy shoulders and Baron horns out.
        float pad=type==MT_FATSO?4.f:type==MT_CYBORG?3.f:2.f;
        middle=(ex0+ex1+1)*0.5f;half=(ex1-ex0+1)*0.5f+pad;
        neck=ey1+(type==MT_FATSO?10.f:type==MT_CYBORG?9.f:7.f);first=std::max(0,ey0-5);found=true;
    }
    if(out.stock&&!found&&(type==MT_KNIGHT||type==MT_BRUISER||type==MT_CYBORG)) {
        // These stock skulls sit between horns/shoulder silhouettes, which
        // can defeat the generic neck finder. Follow the uppermost organic
        // skull pixels instead, with a deliberately narrow face cap. This
        // calibrated recipe is never applied to replacement artwork.
        std::vector<int> skull;
        int firstFlesh=h;
        for(int y=0;y<h/3;++y)for(int x=0;x<w;++x) {
            if(!opaque(x,y))continue;unsigned c=pixels[((size_t)y*w+x)*2];
            bool flesh=(c>=16&&c<80)||(c>=128&&c<148);
            if(flesh&&std::abs(x-left)<=w*0.3f){firstFlesh=std::min(firstFlesh,y);}
        }
        for(int y=firstFlesh;y<std::min(h,firstFlesh+5);++y)for(int x=0;x<w;++x) {
            if(!opaque(x,y))continue;unsigned c=pixels[((size_t)y*w+x)*2];
            if(((c>=16&&c<80)||(c>=128&&c<148))&&std::abs(x-left)<=w*0.3f)skull.push_back(x);
        }
        if(skull.size()>=4) {
            std::sort(skull.begin(),skull.end());middle=skull[skull.size()/2]+0.5f;
            first=firstFlesh;neck=(float)(firstFlesh+(type==MT_CYBORG?13:14));half=type==MT_CYBORG?6.f:7.f;found=true;
        }
    }
    if(!found)return out;
    if(type==MT_UNDEAD||type==MT_VILE)half=std::min(half,w*0.15f);
    if(type==MT_KNIGHT||type==MT_BRUISER)half*=0.8f;
    for(int y=first;y<std::min(h,(int)std::ceil(neck));++y)for(int x=0;x<w;++x) {
        if(!opaque(x,y)||std::abs(x+0.5f-middle)>half)continue;
        unsigned c=pixels[((size_t)y*w+x)*2];
        if((type==MT_KNIGHT||type==MT_BRUISER||type==MT_CYBORG)&&c>=80&&c<112)continue; // gray horns/metal
        out.pixels[(size_t)y*w+x]=WP_HEAD;
    }
    return out;
}
}
#endif

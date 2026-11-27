#ifndef DOOM_MATERIAL_LIGHT_COVERAGE_H
#define DOOM_MATERIAL_LIGHT_COVERAGE_H

#include "material_light_policy.h"
#include <vector>

namespace doom_material_light {

struct CoveragePoint { float x,y; };
struct CoveredEmission {
    double weight=0,x=0,y=0;
    void add(const CoveredEmission &other) {weight+=other.weight;x+=other.x;y+=other.y;}
    CoveragePoint center() const {return {float(x/weight),float(y/weight)};}
};

inline CoveredEmission integrateWallEmission(const uint8_t *mask,int width,int height,
        float left,float top,float right,float bottom,size_t &budget) {
    if(!mask||!maskSize(width,height)||right<=left||bottom<=top)return {};
    const int x0=(int)std::floor(left),x1=(int)std::ceil(right);
    const int y0=(int)std::floor(top),y1=(int)std::ceil(bottom);
    const auto cost=uint64_t(x1-x0)*(y1-y0);
    if(cost>budget) {budget=0;return {};}
    budget-=size_t(cost);
    CoveredEmission result;
    for(int y=y0;y<y1;++y) {
        const int py=(y%height+height)%height;
        const float a=std::max(float(y),top),b=std::min(y+1.f,bottom);
        for(int x=x0;x<x1;++x) {
            const int px=(x%width+width)%width;
            const float c=std::max(float(x),left),d=std::min(x+1.f,right);
            const double w=double(mask[size_t(py)*width+px])*(d-c)*(b-a);
            result.add({w,(c+d)*.5*w,(a+b)*.5*w});
        }
    }
    return result;
}

inline void clipEdge(std::vector<CoveragePoint> &polygon,std::vector<CoveragePoint> &buffer,
                     bool xAxis,float limit,bool keepAbove) {
    if(polygon.empty())return;
    buffer.clear();
    auto distance=[&](CoveragePoint p) {float d=(xAxis?p.x:p.y)-limit;return keepAbove?d:-d;};
    auto last=polygon.back();
    float previous=distance(last);
    for(auto p:polygon) {
        float current=distance(p);
        if((previous<0)!=(current<0)) {
            float t=previous/(previous-current);
            buffer.push_back({last.x+(p.x-last.x)*t,last.y+(p.y-last.y)*t});
        }
        if(current>=0)buffer.push_back(p);
        last=p;previous=current;
    }
    polygon.swap(buffer);
}

inline void clipBox(std::vector<CoveragePoint> &polygon,std::vector<CoveragePoint> &buffer,
                    float left,float top,float right,float bottom) {
    clipEdge(polygon,buffer,true,left,true);
    clipEdge(polygon,buffer,true,right,false);
    clipEdge(polygon,buffer,false,top,true);
    clipEdge(polygon,buffer,false,bottom,false);
}

// A texel's weight is its scalar emission times the exact covered area.
// Positions are integrated over the clipped polygon, including sub-texel slivers.
inline CoveredEmission integrateEmission(const std::vector<CoveragePoint> &polygon,const uint8_t *mask,
                                          int width,int height,size_t &budget) {
    if(polygon.size()<3||!mask||!maskSize(width,height))return {};
    float left=polygon[0].x,right=left,top=polygon[0].y,bottom=top;
    for(auto p:polygon) {
        if(!std::isfinite(p.x)||!std::isfinite(p.y))return {};
        left=std::min(left,p.x);right=std::max(right,p.x);top=std::min(top,p.y);bottom=std::max(bottom,p.y);
    }
    int x0=(int)std::floor(std::clamp(left,0.f,float(width))),x1=(int)std::ceil(std::clamp(right,0.f,float(width)));
    int y0=(int)std::floor(std::clamp(top,0.f,float(height))),y1=(int)std::ceil(std::clamp(bottom,0.f,float(height)));
    size_t cost=size_t(x1-x0)*(y1-y0);
    if(cost>budget) {budget=0;return {};}
    budget-=cost;
    CoveredEmission result;
    std::vector<CoveragePoint> piece,buffer;
    for(int y=y0;y<y1;++y)for(int x=x0;x<x1;++x) {
        uint8_t emission=mask[size_t(y)*width+x];
        if(!emission)continue;
        piece=polygon;
        clipBox(piece,buffer,float(x),float(y),float(x+1),float(y+1));
        double area=0,mx=0,my=0;
        // Subtract the texel origin to keep narrow intersections stable.
        for(size_t i=0;i<piece.size();++i) {
            const auto a=piece[i],b=piece[(i+1)%piece.size()];
            const double ax=a.x-x,ay=a.y-y,bx=b.x-x,by=b.y-y;
            double cross=ax*by-ay*bx;
            area+=cross;mx+=(ax+bx)*cross;my+=(ay+by)*cross;
        }
        if(area==0)continue;
        const double w=std::abs(area)*.5*emission;
        result.add({w,(x+mx/(3*area))*w,(y+my/(3*area))*w});
    }
    return result;
}

// BSP leaves are disjoint in area. Connect only leaves sharing a nonzero
// edge segment in the texture repeat, so a point contact keeps its own light.
inline bool sharesEdge(const std::vector<CoveragePoint> &a,const std::vector<CoveragePoint> &b) {
    for(size_t i=0;i<a.size();++i) {
        const auto p=a[i],q=a[(i+1)%a.size()];
        const float dx=q.x-p.x,dy=q.y-p.y,length=std::hypot(dx,dy);
        if(length<.001f)continue;
        for(size_t j=0;j<b.size();++j) {
            const auto s=b[j],t=b[(j+1)%b.size()];
            auto distance=[&](CoveragePoint r){return std::abs(dx*(r.y-p.y)-dy*(r.x-p.x))/length;};
            if(distance(s)>.001f||distance(t)>.001f)continue;
            auto along=[&](CoveragePoint r){return (dx*(r.x-p.x)+dy*(r.y-p.y))/length;};
            const auto limits=std::minmax({along(s),along(t)});
            if(std::min(length,limits.second)-std::max(0.f,limits.first)>.001f)return true;
        }
    }
    return false;
}

template<class Point>
inline bool insideConvex(const std::vector<Point> &poly,float x,float y) {
    bool positive=false,negative=false;
    for(size_t i=0;i<poly.size();++i) {
        const auto a=poly[i],b=poly[(i+1)%poly.size()];
        const float cross=(b.x-a.x)*(y-a.y)-(b.y-a.y)*(x-a.x);
        positive|=cross>0.01f;negative|=cross<-0.01f;
    }
    return !(positive&&negative);
}

} // namespace doom_material_light
#endif

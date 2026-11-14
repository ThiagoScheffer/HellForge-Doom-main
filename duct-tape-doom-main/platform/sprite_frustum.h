#ifndef HELLFORGE_SPRITE_FRUSTUM_H
#define HELLFORGE_SPRITE_FRUSTUM_H
#include <algorithm>
#include <cmath>
namespace doom_sprite_frustum {
// Shared with the shadow renderer: changing its projection limits changes
// the filter's envelope too. No inferred collision-cylinder dimensions.
constexpr float shadowWidthMin=20,shadowWidthMax=80,shadowScaleMax=1.7f,shadowLengthMax=192;
constexpr float shadowContactLength=.65f;
inline float shadowReach(){return shadowWidthMax*shadowScaleMax*.5f+std::max(shadowLengthMax,shadowWidthMax*shadowScaleMax*shadowContactLength);}
struct Vec {double x,y,z;};
struct Bounds {Vec low,high;};
struct Camera {Vec eye,right,up,forward;double horizontal,vertical,nearPlane;};
inline double dot(Vec a,Vec b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline bool finite(Vec p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
inline bool valid(Bounds b){return finite(b.low)&&finite(b.high)&&b.low.x<=b.high.x&&b.low.y<=b.high.y&&b.low.z<=b.high.z;}
inline void include(Bounds &a,Bounds b){a.low={std::min(a.low.x,b.low.x),std::min(a.low.y,b.low.y),std::min(a.low.z,b.low.z)};a.high={std::max(a.high.x,b.high.x),std::max(a.high.y,b.high.y),std::max(a.high.z,b.high.z)};}
inline Bounds artwork(double x,double y,double z,int width,int height,int left,int top,Vec right,double floor,double ceiling,bool fitCeiling){
    double upper=z+top,lower=upper-height,shift=std::max(0.,floor-lower);
    if(fitCeiling&&ceiling-floor>=height)shift=std::min(shift,ceiling-upper);
    upper+=shift;lower+=shift;
    double l=-double(left),r=l+width;
    return {{x+std::min(right.x*l,right.x*r),y+std::min(right.y*l,right.y*r),lower},
            {x+std::max(right.x*l,right.x*r),y+std::max(right.y*l,right.y*r),upper}};
}
inline bool outside(Bounds b,const Camera &c){
    if(!valid(b)||!finite(c.eye)||!finite(c.right)||!finite(c.up)||!finite(c.forward)||
       !std::isfinite(c.horizontal)||!std::isfinite(c.vertical)||c.horizontal<=0||c.vertical<=0||!std::isfinite(c.nearPlane))return false;
    Vec centre={(b.low.x+b.high.x)*.5-c.eye.x,(b.low.y+b.high.y)*.5-c.eye.y,(b.low.z+b.high.z)*.5-c.eye.z};
    Vec half={(b.high.x-b.low.x)*.5,(b.high.y-b.low.y)*.5,(b.high.z-b.low.z)*.5};
    auto radius=[&](Vec n){return std::abs(n.x)*half.x+std::abs(n.y)*half.y+std::abs(n.z)*half.z;};
    double depth=dot(centre,c.forward),span=radius(c.forward);
    // Keep near-plane crossings, and all numerically marginal cases.
    if(depth-span<=c.nearPlane+1&&depth+span>=c.nearPlane-1)return false;
    for(int axis=0;axis<2;++axis)for(double sign:{-1.,1.}){
        Vec a=axis?c.up:c.right;double scale=axis?c.vertical:c.horizontal;
        Vec n={c.forward.x+sign*a.x*scale,c.forward.y+sign*a.y*scale,c.forward.z+sign*a.z*scale};
        double margin=std::sqrt(dot(n,n));
        if(dot(centre,n)+radius(n)<-margin)return true;
    }
    return false;
}
}
#endif

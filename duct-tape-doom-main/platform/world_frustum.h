#ifndef HELLFORGE_WORLD_FRUSTUM_H
#define HELLFORGE_WORLD_FRUSTUM_H
#include "sprite_frustum.h"
#include <array>
namespace doom_world_frustum {
using doom_sprite_frustum::Camera;
using doom_sprite_frustum::Vec;
// Project the camera's side half-spaces onto XY, eliminating height using
// nonnegative combinations. Every visible point satisfies these inequalities
// at ANY height: moving floors/doors and sky extensions need no stale Z bounds.
// The XY broad phase ignores height. Ordinary geometry can also be tested
// against the original side planes using its current bounds.
class Frustum {
    struct Plane {double x,y;};
    std::array<Plane,10> planes{};
    int count=0;
    Vec eye{};
    std::array<Vec,4> spatialPlanes{};
    int spatialCount=0;
public:
    Frustum()=default;
    explicit Frustum(const Camera &c) {
        using namespace doom_sprite_frustum;
        if(!finite(c.eye)||!finite(c.right)||!finite(c.up)||!finite(c.forward)||
           !std::isfinite(c.horizontal)||!std::isfinite(c.vertical)||c.horizontal<=0||c.vertical<=0)return;
        eye=c.eye;
        Vec sides[4];int k=0;
        for(int axis=0;axis<2;++axis)for(double sign:{-1.,1.}) {
            Vec a=axis?c.up:c.right;double scale=axis?c.vertical:c.horizontal;
            sides[k++]={c.forward.x+sign*a.x*scale,c.forward.y+sign*a.y*scale,c.forward.z+sign*a.z*scale};
        }
        for (Vec side : sides)
        {
            const double length=std::sqrt(dot(side,side));
            if (!std::isfinite(length) || length<=1e-12)
                continue;
            spatialPlanes[spatialCount++]={side.x/length,side.y/length,side.z/length};
        }
        auto add=[&](double x,double y) {
            double length=std::hypot(x,y);
            if(std::isfinite(length)&&length>1e-12&&count<(int)planes.size())planes[count++]={x/length,y/length};
        };
        for(int i=0;i<4;++i) {
            if(sides[i].z==0)add(sides[i].x,sides[i].y);
            for(int j=0;j<i;++j)if((sides[i].z<0&&sides[j].z>0)||(sides[i].z>0&&sides[j].z<0)) {
                double a=std::abs(sides[j].z),b=std::abs(sides[i].z);
                add(a*sides[i].x+b*sides[j].x,a*sides[i].y+b*sides[j].y);
            }
        }
    }
    bool outside(const std::array<float,4> &bounds) const {
        for(float v:bounds)if(!std::isfinite(v))return false;
        if(bounds[0]>bounds[2]||bounds[1]>bounds[3])return false;
        for(int i=0;i<count;++i) {
            const auto &p=planes[i];
            double x=(p.x>=0?bounds[2]:bounds[0])-eye.x,y=(p.y>=0?bounds[3]:bounds[1])-eye.y;
            // A full world unit of slack protects clipping and float roundoff.
            if(p.x*x+p.y*y < -1)return true;
        }
        return false;
    }
    bool outside3D(const std::array<float,4> &bounds, float bottom, float top) const
    {
        const doom_sprite_frustum::Bounds box{{bounds[0],bounds[1],bottom},{bounds[2],bounds[3],top}};
        if (!doom_sprite_frustum::valid(box))
            return false;
        const Vec low{box.low.x-eye.x,box.low.y-eye.y,box.low.z-eye.z};
        const Vec high{box.high.x-eye.x,box.high.y-eye.y,box.high.z-eye.z};
        for (int i=0;i<spatialCount;++i)
        {
            const Vec n=spatialPlanes[i];
            const Vec support{n.x>=0?high.x:low.x,n.y>=0?high.y:low.y,n.z>=0?high.z:low.z};
            // Match the XY broad phase's one world unit of clipping slack.
            if (doom_sprite_frustum::dot(n,support)<-1)
                return true;
        }
        return false;
    }
};
}
#endif

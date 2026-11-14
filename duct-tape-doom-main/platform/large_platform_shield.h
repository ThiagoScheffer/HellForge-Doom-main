#pragma once
#include "../../map_source.h"
#include <algorithm>
namespace lab {
// AABB workload, not collision radius or line count. 1024 x 1024 equivalent.
inline bool largePlatformFootprint(const MapRegion& r){
 if(r.edges.empty())return false;
 double x0=INFINITY,y0=INFINITY,x1=-INFINITY,y1=-INFINITY;
 for(const auto& edge:r.edges)for(auto p:{edge.a,edge.b}){
  x0=std::min(x0,double(p.x));y0=std::min(y0,double(p.y));
  x1=std::max(x1,double(p.x));y1=std::max(y1,double(p.y));
 }
 return (x1-x0)*(y1-y0)>=1024.0*1024.0;
}
inline bool floorMoverSpecial(unsigned v){
 if((v>=0x6000&&v<0x8000)||(v>=0x3000&&v<0x3800))return true;
 switch(v){
 case 5:case 7:case 8:case 9:case 10:case 14:case 15:case 18:case 19:case 20:case 21:case 22:case 23:case 24:
 case 30:case 36:case 37:case 38:case 40:case 45:case 47:case 53:case 55:case 56:case 58:case 59:
 case 60:case 62:case 64:case 65:case 66:case 67:case 68:case 69:case 70:case 71:
 case 82:case 83:case 84:case 87:case 88:case 91:case 92:case 93:case 94:case 95:case 96:case 98:
 case 100:case 101:case 102:case 119:case 120:case 121:case 122:case 123:case 127:case 128:
 case 129:case 130:case 131:case 132:case 140:case 142:case 159:case 219:case 221:case 233:return true;
 default:return false;
 }
}
}

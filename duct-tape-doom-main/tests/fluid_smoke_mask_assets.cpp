#include <cassert>
#include <initializer_list>
#include <cstdio>
#include "fluid_effect_assets.inc"
int main(){
  int edges=0, opaque=0, middle=0;
  for(int id:{13,16}){
    const auto&m=fluidMasks[id];
    assert(m.w==64&&m.h==64);
    for(int y=0;y<m.h;++y)for(int x=0;x<m.w;++x){
      const int a=m.alpha[y*m.w+x];
      if(a==0)++edges;
      if(a>0&&a<255)++middle;
      if(a>=255)++opaque;
    }
  }
  assert(edges>0 && middle>0);
  printf("PASS: existing Fluid smoke masks 13 and 16 are 64x64, soft-alpha, with %d zero and %d intermediate pixels, %d full-opacity pixels\n",edges,middle,opaque);
}

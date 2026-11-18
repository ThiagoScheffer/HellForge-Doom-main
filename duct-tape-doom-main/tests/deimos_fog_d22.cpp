#include "platform/episode_atmosphere.h"
#include <cassert>
#include <cmath>
#include <cstdio>
int main(){
  for(float storm: {0.f,.68f,.82f,1.f}) {
    float previous=-1.f;
    for(int level=0;level<=3;++level){
      float strength=doom_episode::deimosFogStrength(level,storm);
      assert(strength>=0.f&&strength<=1.f&&strength>=previous);
      previous=strength;
      float last=0.f;
      for(int distance=0;distance<=800;distance+=10) {
        float fog=doom_episode::deimosFogOpacity(float(distance),strength);
        assert(std::isfinite(fog)&&fog>=0.f&&fog<=1.f&&fog>=last-1e-6f);
        last=fog;
      }
      if(level==0)assert(doom_episode::deimosFogOpacity(800,strength)==0.f);
    }
  }
  for(int d: {50,100,200,400}){
    auto light=doom_episode::deimosFogOpacity(float(d),doom_episode::deimosFogStrength(1,.85f));
    auto heavy=doom_episode::deimosFogOpacity(float(d),doom_episode::deimosFogStrength(2,.85f));
    auto whiteout=doom_episode::deimosFogOpacity(float(d),doom_episode::deimosFogStrength(3,.85f));
    printf("at %d units: light %.3f, heavy %.3f, whiteout %.3f\n",d,light,heavy,whiteout);
    assert(light<=heavy&&heavy<=whiteout);
  }
  assert(doom_episode::select(1,true,true,true,2)==doom_episode::Profile::Deimos);
  assert(doom_episode::select(1,true,true,true,3)==doom_episode::Profile::Inferno);
  assert(doom_episode::select(0,true,true,true,2)==doom_episode::Profile::None);
  assert(doom_episode::crystalBudget(2)==480);
  puts("D2.2 control, spatial extinction, episode profile and particle budget: PASS");
}

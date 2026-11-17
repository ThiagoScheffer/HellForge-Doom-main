#include "platform/storm_rules.h"
#include <cassert>
#include <cmath>
#include <cstdio>
int main() {
  doom_storm::State s;
  int events=0,close=0,medium=0,far=0,anyFlash=0;
  for(int t=0;t<35*600;++t) {
    auto f=doom_storm::advance(s,t,35,3,true,true,0x11345u);
    assert(f.flash>=0&&f.flash<=.70f);
    if(f.flash>0)++anyFlash;
    if(f.thunder!=doom_storm::Kind::None) {
       ++events;
       if(f.thunder==doom_storm::Kind::Close)++close;
       if(f.thunder==doom_storm::Kind::Medium)++medium;
       if(f.thunder==doom_storm::Kind::Distant)++far;
    }
  }
  assert(events>10&&anyFlash>30&&far>0&&medium>0&&close>0);
  auto off=doom_storm::advance(s,35*600,35,3,false,true,0x11345u);
  assert(off.thunder==doom_storm::Kind::None&&off.flash==0&&s.nextFlash==-1);
  doom_storm::advance(s,0,35,3,true,false,0x11345u);
  for(int t=1;t<300;++t) {auto f=doom_storm::advance(s,t,35,3,true,false,0x11345u);assert(f.flash<=.19f);}
  // Same level seed produces repeatable flashes and audio events.
  doom_storm::State a,b;
  for(int t=0;t<10000;++t) {
    auto x=doom_storm::advance(a,t,35,2,true,true,4242);
    auto y=doom_storm::advance(b,t,35,2,true,true,4242);
    assert(x.thunder==y.thunder&&std::abs(x.flash-y.flash)<1e-7);
  }
  // Bolts stay fixed for each event; their visibility cannot change audio or flashes.
  doom_storm::State visible,hidden;
  int boltFrames=0;
  float azimuth=0,branchSeed=0;
  unsigned event=0;
  for(int t=0;t<35*600;++t) {
    auto x=doom_storm::advance(visible,t,35,3,true,false,4242,true);
    auto y=doom_storm::advance(hidden,t,35,3,true,false,4242,false);
    assert(x.thunder==y.thunder&&x.flash==y.flash&&y.bolt==0);
    assert(x.bolt>=0&&x.bolt<=.19f);
    if(visible.event!=event) {
      event=visible.event;azimuth=x.azimuth;branchSeed=x.boltSeed;
    }
    if(x.bolt>0) {
      ++boltFrames;
      assert(x.azimuth==azimuth&&x.boltSeed==branchSeed);
      assert(t-visible.flashTic<=6);
      assert(x.azimuth>=-3.141593f&&x.azimuth<=3.141593f);
      assert(x.boltSeed>=0&&x.boltSeed<1);
    }
  }
  assert(boltFrames>0);
  auto dry=doom_storm::advance(visible,35*600,35,3,false,false,4242,true);
  assert(dry.bolt==0&&!visible.hasBolt);
  // Reset after level time moves backwards.
  auto rewind=doom_storm::advance(a,0,35,2,true,true,4242);
  assert(a.event==0&&rewind.thunder==doom_storm::Kind::None);
  std::printf("PASS: %d thunder events (%d close, %d medium, %d distant), %d flash frames; resets and seed reproducibility\n",events,close,medium,far,anyFlash);
}

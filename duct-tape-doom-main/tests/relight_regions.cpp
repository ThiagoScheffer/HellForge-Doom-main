#include "../platform/relight_regions.h"
#include <cstdio>
#include <cstdlib>
#include <random>
#include <limits>
using doom3d::RelightRegions;
static void check(bool ok){if(!ok){std::fputs("FAIL indexed relight region differs from linear marking\n",stderr);std::exit(1);}}
int main() {
    std::mt19937 rng(39271);std::uniform_real_distribution<float> coord(-32000,32000),extent(0,2000);
    std::vector<RelightRegions::Entry> entries;
    for(int i=0;i<4000;++i) {
        float x=coord(rng),y=coord(rng),w=extent(rng),h=i%3?extent(rng):0;
        entries.push_back({{x,y,x+w,y+h},i});
    }
    entries.push_back({{0,0,128,0},4000});
    RelightRegions index;index.reset(entries);
    for(int n=0;n<10000;++n) {
        float x=coord(rng),y=coord(rng);RelightRegions::Box q{x,y,x+extent(rng),y+extent(rng)};
        if(n==0)q={128,0,128,0}; // Touching endpoints, including zero-height strips.
        if(n==1)q={-64000,-64000,64000,64000};
        if(n==2)q={0,0,std::numeric_limits<float>::quiet_NaN(),1};
        std::vector<unsigned char> expected(entries.size()),actual(entries.size());
        for(const auto &e:entries)if(RelightRegions::overlap(e.box,q))expected[e.id]|=n%2?1:2;
        index.each(q,[&](int id){actual[id]|=n%2?1:2;});check(expected==actual);
        // Repeated and overlapping light/sun requests must preserve the union.
        index.each(q,[&](int id){actual[id]|=n%2?1:2;});check(expected==actual);
    }
    index.reset();int count=0;index.each({0,0,1,1},[&](int){++count;});check(count==0);
    index.reset({{{10,10,20,20},0}});index.each({0,0,1,1},[&](int){++count;});check(count==0);
    std::puts("PASS relight index: 10000 exact linear-mask comparisons, boundaries, repeated light/sun regions, and rebuilds");
}

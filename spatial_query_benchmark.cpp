#include "map_world.h"
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <random>
using namespace lab;
int main(){
 MapRegion r;r.sector=0;r.floor=0;r.ceiling=128;
 Vec v[]={{-1024,-1024,0},{1024,-1024,0},{1024,1024,0},{-1024,1024,0}};
 for(int i=0;i<4;++i)r.edges.push_back({v[i],v[(i+1)%4],i});
 MapSource source;source.reset({r});MapWorld world;world.reset(source);
 for(int x=0;x<16;++x)for(int y=0;y<8;++y)
  if(!world.blast({-900.f+x*112,-800.f+y*224,-2},4,0,0,0))throw std::runtime_error("fixture cut failed");
 std::mt19937 gen(413);std::uniform_real_distribution<float> xy(-1800,1800),z(-16,96);
 std::vector<Vec> queries;queries.reserve(500000);
 for(int i=0;i<500000;++i)queries.push_back({xy(gen),xy(gen),z(gen)});
 size_t count[2]={0,0};double elapsed[2]={0,0};
 for(int run=0;run<6;++run){int indexed=run%2;size_t n=0;auto start=std::chrono::steady_clock::now();
  for(auto p:queries)n+=indexed?world.mayBeAffected(p,20,56):world.affected(p,20,56);
  elapsed[indexed]+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();count[indexed]=n;
 }
 if(count[0]!=count[1])return 2;
 auto stats=world.cutIndexStats();std::cout<<"cuts="<<world.cutCount()<<" cells="<<stats.cells<<" refs="<<stats.references<<" queries_per_run="<<queries.size()<<" affected="<<count[0]<<" linear_mean_ms="<<elapsed[0]/3<<" indexed_mean_ms="<<elapsed[1]/3<<'\n';
}

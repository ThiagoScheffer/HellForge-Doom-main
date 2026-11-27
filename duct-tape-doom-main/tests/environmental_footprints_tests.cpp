#include "../platform/scene3d/footprint_rules.h"
#include <cstdio>
#include <limits>
#include <vector>

using namespace doom3d::footprint;
#define CHECK(condition) do { if(!(condition)) { \
    std::fprintf(stderr,"Line %d: %s\n",__LINE__,#condition); return 1; \
} } while(false)

int main() {
    CHECK(doomFlatMaterial("FWATER1")==Material::Water);
    CHECK(doomFlatMaterial("SWATER4")==Material::Water);
    CHECK(doomFlatMaterial("WATER")==Material::Water);
    CHECK(doomFlatMaterial("BLOOD3")==Material::Blood);
    CHECK(doomFlatMaterial("MUD08")==Material::Mud&&doomFlatMaterial("MUD8")==Material::Mud);
    for(auto name:{"","FWATER5","WATERFAL","BLOODWAL","MUDSTONE","MUD09","SLIME01",
                   "SLIME09","SLIME13","NUKAGE1","LAVA1","GRASS1","DIRT1","F_SKY1"})
        CHECK(doomFlatMaterial(name)==Material::None);
    const Contact water{Material::Water,Origin::Flat},mud{Material::Mud,Origin::Flat};
    const Contact pool{Material::Blood,Origin::Pool,2};
    CHECK(lifetime(water,0)==3&&lifetime(water,1)==6&&lifetime(water,2)==9);
    CHECK(lifetime(mud,1)==25&&lifetime(pool,0)==45&&lifetime(pool,2)==45);
    CHECK(expired(water,6,6)&&expired(water,-1,6)&&!expired(pool,45,45));
    CHECK(expired(water,std::numeric_limits<float>::quiet_NaN(),6));
    CHECK(waterOpacity(0,0,6)>waterOpacity(1,0,6)&&waterOpacity(0,6,6)==0);
    CHECK(waterShine(0,3,6)==0&&pigmentCoverage(0,25)==1&&pigmentCoverage(25,25)==0);

    Tracker tracker;
    int actor=0,other=0;
    float dx,dy;
    CHECK(tracker.sample(&actor,0,0,0,0,dx,dy)&&dx==0);
    auto &trail=tracker.trail;
    CHECK(!trail.advance(water,false,true,0,35,0)&&trail.inside);
    CHECK(!tracker.sample(&actor,0,0,20,0,dx,dy));
    CHECK(tracker.sample(&actor,0,1,3,0,dx,dy)&&dx==3);
    CHECK(!trail.advance(water,false,true,1,35,3)&&trail.inside);
    CHECK(!trail.advance({},false,true,2,35,20)&&trail.start==2);
    CHECK(!trail.advance({},false,true,3,35,0));
    CHECK(!trail.advance({},false,true,4,35,10));
    CHECK(trail.advance({},false,true,5,35,10)&&trail.left);
    CHECK(!trail.advance({},false,true,6,35,1)&&trail.left&&trail.prints==0);
    CHECK(trail.advance({},false,true,7,35,19));
    trail.accepted();
    CHECK(!trail.left&&trail.prints==1&&trail.stage(7,35)==0);
    CHECK(!trail.advance({},false,false,8,35,30)&&trail.stride==0);
    trail.prints=7;
    CHECK(trail.stage(8,35)==2&&trail.advance({},false,true,71,35,20));
    trail.accepted();
    CHECK(!trail.advance({},false,true,71,35,20));
    CHECK(!trail.advance(mud,false,true,72,35,20)&&trail.carry.material==Material::Mud);
    CHECK(!trail.advance({},false,true,73,35,20));
    CHECK(trail.advance({},false,true,177,35,20));
    CHECK(!trail.advance({},false,true,178,35,20));
    CHECK(!trail.advance(pool,false,true,179,35,0)&&trail.carry.tint==2);
    CHECK(!trail.advance({},false,true,180,35,0));
    trail.prints=19;
    CHECK(trail.stage(180,35)==0&&trail.advance({},false,true,249,35,20));
    CHECK(!trail.advance({},false,true,250,35,20));
    CHECK(!trail.advance({},true,true,251,35,20)&&trail.carry.origin==Origin::None);
    CHECK(!trail.advance(water,false,true,252,35,0));
    CHECK(!trail.advance({},false,false,253,35,20)&&trail.carry.origin==Origin::None);

    trail.carry=water;
    tracker.teleported(&other);
    CHECK(trail.carry.material==Material::Water);
    tracker.teleported(&actor);
    CHECK(tracker.sample(&actor,0,2,6,0,dx,dy)&&dx==0&&trail.carry.origin==Origin::None);
    CHECK(!tracker.sample(&actor,0,3,110,0,dx,dy)&&!tracker.actor);
    CHECK(tracker.sample(&actor,0,4,12,0,dx,dy)&&dx==0);
    trail.carry=water;
    CHECK(tracker.sample(&other,0,5,20,0,dx,dy)&&dx==0&&trail.carry.origin==Origin::None);
    CHECK(tracker.sample(&other,1,6,30,0,dx,dy)&&dx==0);
    CHECK(tracker.sample(&other,1,2,40,0,dx,dy)&&dx==0);
    CHECK(tracker.sample(&other,1,10,50,0,dx,dy)&&dx==0);

    struct Print {Contact source;int id;};
    std::vector<Print> records;
    for(std::size_t i=0;i<limit;++i)CHECK(insert(records,{pool,int(i)}));
    CHECK(!insert(records,{water,100})&&records.front().id==0);
    CHECK(insert(records,{pool,101})&&records.size()==limit&&records.front().id==1);
    records.back()={water,102};
    CHECK(insert(records,{mud,103})&&records.back().id==103&&records.front().id==1);
    CHECK(insert(records,{pool,104})&&records.back().id==104&&records.front().id==1);
    std::puts("Environmental footprint rules passed.");
}

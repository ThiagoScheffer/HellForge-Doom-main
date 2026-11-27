#include "../platform/material_light_coverage.h"

#include <array>
#include <cstdio>
#include <cstdlib>

using namespace doom_material_light;

static void require(bool condition,const char *message) {
    if(!condition) {
        std::fprintf(stderr,"FAIL: %s\n",message);
        std::exit(EXIT_FAILURE);
    }
}

static bool near(double a,double b) {return std::abs(a-b)<.0001;}

int main() {
    const float infinity=std::numeric_limits<float>::infinity();
    require(effectivePower(2,0)==0&&effectivePower(2,.5f)==1&&effectivePower(2,2)==2,"current normalized power");
    require(effectivePower(infinity,1)==0&&effectivePower(1,infinity)==0&&effectivePower(1,-1)==0,"invalid power");
    require(!canEmit(0,88)&&!canEmit(1,0)&&!canEmit(infinity,88)&&!canEmit(1,infinity),"invalid emitters");
    require(sourceRadius(256,Kind::Display,false)==256&&sourceRadius(256,Kind::Display,true)==128,"display cap and Enhanced output");
    require(sourceRadius(80,Kind::Indicator,true)==80&&sourceRadius(224,Kind::Indicator,true)==88,"indicator caps");
    require(sourceRadius(224,Kind::None,true)==0&&sourceRadius(infinity,Kind::Fixture,true)==0,"inactive radius");
    for(auto kind:{Kind::Fixture,Kind::Diffuser,Kind::Fire,Kind::Liquid,Kind::Glass,Kind::Custom})
        require(sourceRadius(224,kind,true)==224,"uncapped point classes preserve legacy radius");
    require(classify(DOOM_EMISSIVE_NONE,false,false,true,false,false,1)==Kind::Custom,"unknown explicit emission");
    require(classify(DOOM_EMISSIVE_PANEL,false,false,true,false,false,1)==Kind::Display,"known override preserves semantics");
    require(classify(DOOM_EMISSIVE_LAMP,false,true,false,false,false,1)==Kind::Indicator,"key indicator");
    require(classify(DOOM_EMISSIVE_NONE,false,false,false,false,true,1)==Kind::Diffuser,"composite diffuser");
    require(classify(DOOM_EMISSIVE_LAVA,false,false,false,false,false,1)==Kind::Fire&&
        classify(DOOM_EMISSIVE_LAVA,true,false,false,false,false,1)==Kind::Liquid,"wall fire is separate from a flat pool");
    require(classify(DOOM_EMISSIVE_LAMP,false,true,true,false,false,0)==Kind::None,"zero override disables emitter");
    require(doom_emissive_kind("LITE4OFF",0)==DOOM_EMISSIVE_NONE&&doom_emissive_kind("LITE3",0)==DOOM_EMISSIVE_LAMP,
        "existing on and off classification");
    const uint8_t white[]={255,255,255};
    require(doom_emissive_pixel(doom_emissive_kind("STARTAN3",0),white,255)==0,
        "bright unrecognized artwork cannot invent emission");
    require(explicitTexel(1,0)==0&&explicitTexel(.5f,255)==127&&explicitTexel(.5f,127)==127,"mask applies coverage once");
    require(explicitTexel(infinity,255)==0&&maskSize(8193,1)==0&&maskSize(0,256)==0&&maskSize(64,64)==4096,
        "bounded alpha and image dimensions");
    std::array<uint8_t,4> mask={};
    const auto zero=explicitMaskIdentity(mask.data(),mask.size(),2,2);
    require(zero!=0&&explicitMaskIdentity(nullptr,4,2,2)==0&&explicitMaskIdentity(mask.data(),3,2,2)==0,
        "valid black distinct from missing and invalid mask");
    mask[0]=255;
    const auto bright=explicitMaskIdentity(mask.data(),mask.size(),2,2);
    require(bright!=zero&&bright==explicitMaskIdentity(mask.data(),mask.size(),2,2),"content hash stable and sensitive");

    const std::vector<CoveragePoint> strip={{-.5f,.2f},{.25f,.2f},{.25f,.6f},{-.5f,.6f}};
    size_t budget=32;
    const auto narrow=integrateEmission(strip,mask.data(),2,2,budget);
    require(near(narrow.weight,25.5)&&near(narrow.center().x,.125)&&near(narrow.center().y,.4),
        "subtexel sliver has exact positive area and centroid");
    const std::vector<CoveragePoint> dark={{1,0},{2,0},{2,1},{1,1}};
    require(integrateEmission(dark,mask.data(),2,2,budget).weight==0,"dark neighboring texel has no emitter");
    const std::vector<CoveragePoint> left={{0,0},{.5f,0},{.5f,1},{0,1}};
    const std::vector<CoveragePoint> right={{.5f,0},{1,0},{1,1},{.5f,1}};
    auto sum=integrateEmission(left,mask.data(),2,2,budget);
    sum.add(integrateEmission(right,mask.data(),2,2,budget));
    require(sharesEdge(left,right)&&!sharesEdge(left,dark)&&near(sum.weight,255)&&near(sum.center().x,.5),
        "shared BSP boundary has no duplicate area");
    auto reversed=left;
    std::reverse(reversed.begin(),reversed.end());
    require(near(integrateEmission(reversed,mask.data(),2,2,budget).weight,127.5),"winding leaves coverage unchanged");
    const auto wall=integrateWallEmission(mask.data(),2,2,-2.f,.2f,-1.75f,.6f,budget);
    require(near(wall.weight,25.5)&&near(wall.center().x,-1.875)&&near(wall.center().y,.4),
        "wall clip wraps negative UVs and integrates only visible texels");
    budget=1;
    require(integrateEmission({{0,0},{2,0},{2,2},{0,2}},mask.data(),2,2,budget).weight==0&&budget==0,
        "bounded coverage falls back on exhausted work");
    std::puts("PASS material profiles, mask identity and clipped coverage");
}

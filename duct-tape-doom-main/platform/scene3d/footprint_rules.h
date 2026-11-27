#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string_view>

namespace doom3d::footprint {
enum class Material { None, Water, Blood, Mud };
enum class Origin { None, Flat, Pool };
struct Contact {
    Material material = Material::None;
    Origin origin = Origin::None;
    int tint = 0;
};
struct Profile { int seconds, prints; float lifetime; };
constexpr Profile profile(Material material) {
    switch(material) {
    case Material::Water: return {2,8,6};
    case Material::Blood: return {2,20,45};
    case Material::Mud: return {3,12,25};
    default: return {};
    }
}
inline bool legacy(Contact c) { return c.origin==Origin::Pool; }
inline float lifetime(Contact c,int choice) {
    return profile(c.material).lifetime*(legacy(c)?1.f:choice==0?.5f:choice==2?1.5f:1.f);
}
inline bool numbered(std::string_view name,std::string_view prefix,char last,bool padded=false) {
    if(name.substr(0,prefix.size())!=prefix)return false;
    name.remove_prefix(prefix.size());
    if(padded&&name.size()==2&&name.front()=='0')name.remove_prefix(1);
    return name.size()==1&&name.front()>='1'&&name.front()<=last;
}
inline Material doomFlatMaterial(std::string_view name) {
    for(std::string_view water:{"WATER","FWATER","SWATER"})
        if(name==water||numbered(name,water,'4'))return Material::Water;
    if(name=="BLOOD"||numbered(name,"BLOOD",'3'))return Material::Blood;
    if(name=="MUD"||numbered(name,"MUD",'8',true))return Material::Mud;
    return Material::None;
}

constexpr float strideLength=20.f;
constexpr std::size_t limit=96;
struct Trail {
    Contact carry;
    bool inside=false,left=true;
    int start=-1,prints=0;
    float stride=0;

    bool advance(Contact source,bool blocked,bool grounded,int tic,int rate,float moved) {
        if(blocked) {*this={};return false;}
        if(source.origin!=Origin::None) {*this={};carry=source;inside=true;return false;}
        // Airborne exits from map flats discard the load.
        if(!grounded&&inside&&!legacy(carry)) {*this={};return false;}
        if(inside) {inside=false;start=tic;stride=0;return false;}
        if(!grounded) {stride=0;return false;}
        const auto p=profile(carry.material);
        if(start<0||tic<start||tic-start>=p.seconds*rate||prints>=p.prints)return false;
        stride+=moved;
        if(stride<strideLength||moved<.001f)return false;
        stride=std::fmod(stride,strideLength);
        return true;
    }
    int stage(int tic,int rate) const {
        const auto p=profile(carry.material);
        int age=(tic-start)*3/(p.seconds*rate);
        return legacy(carry)?age:std::max(age,prints*3/p.prints);
    }
    void accepted() { left=!left;++prints; }
};

struct Tracker {
    Trail trail;
    const void *actor=nullptr; // Identity only.
    int player=-1,lastTic=-1;
    float x=0,y=0;

    bool sample(const void *identity,int displayed,int tic,float px,float py,float &dx,float &dy) {
        dx=dy=0;
        if(actor!=identity||player!=displayed||tic<lastTic||tic-lastTic>1) {
            *this={};actor=identity;player=displayed;
        } else {
            if(tic==lastTic)return false;
            dx=px-x;dy=py-y;
        }
        lastTic=tic;x=px;y=py;
        if(dx*dx+dy*dy<=96.f*96.f)return true;
        *this={};
        return false;
    }
    void teleported(const void *identity) { if(actor==identity)*this={}; }
};

template<class Records>
bool insert(Records &records,const typename Records::value_type &print) {
    if(records.size()==limit) {
        auto victim=std::find_if(records.begin(),records.end(),[](const auto &p){return !legacy(p.source);});
        if(victim==records.end()&&!legacy(print.source))return false;
        records.erase(victim==records.end()?records.begin():victim);
    }
    records.push_back(print);
    return true;
}
inline bool expired(Contact c,float age,float duration) {
    return !std::isfinite(age)||!std::isfinite(duration)||age<0||duration<=0||
           (legacy(c)?age>duration:age>=duration);
}
inline float remaining(float age,float duration) {return std::clamp(1.f-age/duration,0.f,1.f);}
inline float waterOpacity(int stage,float age,float duration) {
    constexpr float load[]={.30f,.21f,.12f};
    return load[stage]*remaining(age,duration);
}
inline float waterShine(int stage,float age,float duration) {
    constexpr float load[]={.14f,.09f,.04f};
    return load[stage]*remaining(age,duration)*std::max(0.f,1.f-age/3.f);
}
inline float pigmentCoverage(float age,float duration) {return std::min(remaining(age,duration)/.35f,1.f);}

// These keys are separate from WAD images and the legacy blood variants.
constexpr int imageBase=-0x40000100;
inline bool blendedBatch(int key) {return key>=imageBase&&key<imageBase+3;}
}

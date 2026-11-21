/* Blood splats on floors, walls and ceilings from hits, and pools under corpses. */
#include "internal.h"
extern "C" {
#include "p_solid.h"
}

namespace doom3d {
// Blood: decals drawn in the world pass and lit like the surface under them.
// Their artwork is generated per WAD from the BLUD sprite's own palette
// indices, ordered dark to bright by how often each is used: one pool shape,
// four splat shapes, three specks, a drip run and a smear.
int bloodSourceTint=0;
int monsterBloodTint(const mobj_t *thing) {
    if(!thing)return 0;
    return thing->type==MT_HEAD?1:(thing->type==MT_BRUISER||thing->type==MT_KNIGHT)?2:0;
}
static byte bloodTintIndex(byte index,int tint) {
    if(!tint)return index;
    const byte *pal=(const byte*)W_CacheLumpName((char*)"PLAYPAL",PU_CACHE);
    static std::array<std::array<byte,256>,2> tables;
    static std::array<byte,768> previous{};
    static bool ready=false;
    if(!ready||!std::equal(previous.begin(),previous.end(),pal)){
        std::copy_n(pal,768,previous.begin());ready=true;
        for(int kind=0;kind<2;++kind)for(int i=0;i<256;++i){
            float intensity=std::max({pal[i*3],pal[i*3+1],pal[i*3+2]});
            std::array<float,3> color=kind==0?std::array<float,3>{.1f,.1f,1.f}:std::array<float,3>{.16f,1.f,.29f};
            float best=1e9f;int match=0;
            for(int j=0;j<256;++j){float d=0;for(int k=0;k<3;++k){float delta=pal[j*3+k]-intensity*color[k];d+=delta*delta;}if(d<best){best=d;match=j;}}
            tables[kind][i]=byte(match);
        }
    }
    return tables[std::clamp(tint,1,2)-1][index];
}
constexpr int bloodKeyBase=-0x40000000; // Image keys clear of walls and lumps.
constexpr int bloodPool=0,bloodSplats=4,bloodSpeckFirst=1+bloodSplats,bloodSpecks=3,bloodDrip=bloodSpeckFirst+bloodSpecks,bloodSmear=bloodDrip+1;
constexpr int bloodJet=bloodSmear+1;
static std::vector<byte> bloodShades;static bool bloodShadesLoaded=false;
const std::vector<byte> &bloodPalette() {
    if(bloodShadesLoaded)return bloodShades;
    bloodShadesLoaded=true;
    int sprite=Game_Sprite(GAME_SPRITE_BLOOD);
    if(sprite<0||sprite>=numsprites)return bloodShades;
    const byte *palette=(const byte*)W_CacheLumpName((char*)"PLAYPAL",PU_CACHE);
    std::vector<std::pair<float,byte>> pixels;
    const auto &blood=sprites[sprite];
    for(int frame=0;frame<blood.numframes;++frame) {
        const Image &image=lumpImage(firstspritelump+blood.spriteframes[frame].lump[0],false);
        for(size_t i=0;i+1<image.pixels.size();i+=2) if(image.pixels[i+1]) {
            const byte *rgb=palette+image.pixels[i]*3;
            pixels.push_back({0.299f*rgb[0]+0.587f*rgb[1]+0.114f*rgb[2],image.pixels[i]});
        }
    }
    std::sort(pixels.begin(),pixels.end());
    for(const auto &pixel:pixels)bloodShades.push_back(pixel.second);
    return bloodShades;
}
bool bloodOn() {return settings.blood&&gamestate==GS_LEVEL&&!floors.empty()&&!bloodPalette().empty();}
static byte bloodIndex(float shade) {
    const auto &shades=bloodPalette();
    return shades[std::clamp((int)std::lround(std::clamp(shade,0.0f,1.0f)*(shades.size()-1)),0,(int)shades.size()-1)];
}
// Match the existing blood decals: normal monsters use the WAD's blood
// shades, Cacodemons use blue blood, and Baron/Hell Knight use green.
// The progressive skin-damage painter needs a palette INDEX, not RGB.
byte monsterSkinBloodIndex(int type,float shade) {
    if(bloodPalette().empty())return 0;
    const int tint=type==MT_HEAD?1:(type==MT_BRUISER||type==MT_KNIGHT)?2:0;
    return bloodTintIndex(bloodIndex(shade),tint);
}
static std::array<float,3> bloodColorForTint(float shade,int tint) {
    if(bloodPalette().empty())return {0.5f,0,0};
    const byte *rgb=(const byte*)W_CacheLumpName((char*)"PLAYPAL",PU_CACHE)+bloodTintIndex(bloodIndex(shade),tint)*3;
    return {rgb[0]/255.0f,rgb[1]/255.0f,rgb[2]/255.0f};
}
std::array<float,3> bloodColor(float shade) {return bloodColorForTint(shade,bloodSourceTint);}
Image &bloodSpriteImage(int lump,int tint,int &key) {
    key=-1-lump;
    if(!tint)return lumpImage(lump,false);
    key=-15000000-lump*3-std::clamp(tint,1,2);
    auto found=images.find(key);if(found!=images.end())return found->second;
    const auto &source=lumpImage(lump,false);auto pixels=source.pixels;
    for(size_t i=0;i<pixels.size();i+=2)pixels[i]=bloodTintIndex(pixels[i],tint);
    return images.emplace(key,upload(source.width,source.height,pixels.data(),source.left,source.top)).first->second;
}
static Image &bloodImage(int variant) {
    int key=bloodKeyBase+variant;
    auto found=images.find(key);if(found!=images.end())return found->second;
    int tint=variant/32;variant%=32;
    uint32_t seed=hash32(variant*7919u+17u);
    int width=variant==bloodJet?48:variant==bloodPool?64:variant==bloodDrip?3:variant==bloodSmear?32:variant>=bloodSpeckFirst?8:32;
    int height=variant==bloodJet?12:variant==bloodDrip?40:variant==bloodSmear?16:width;
    std::vector<byte> pixels((size_t)width*height*2,0);
    auto put=[&](int x,int y,float shade) {
        if(x<0||y<0||x>=width||y>=height)return;
        size_t i=((size_t)y*width+x)*2;
        pixels[i]=bloodTintIndex(bloodIndex(shade),tint);pixels[i+1]=255;
    };
    if(variant==bloodJet) {
        for(int x=0;x<width;++x) {
            float u=(x+0.5f)/width;
            float center=height*.5f+std::sin(u*12.f)*u;
            float reach=(4.8f-3.1f*u)*(0.8f+0.25f*hashUnit(x,0,seed));
            for(int y=0;y<height;++y) {
                float coverage=std::clamp(reach-std::abs(y+0.5f-center),0.f,1.f);
                pixels[((size_t)y*width+x)*2+1]=(byte)(255*coverage);
            }
        }
    } else if(variant==bloodDrip) {
        // A run one texel wide down to a bead three wide: drawn from the
        // bottom up as it grows, so the bead leads.
        for(int y=0;y<height-4;++y)put(1,y,0.22f+0.12f*valueNoise(0,(float)y,3,seed));
        for(int y=height-4;y<height;++y)for(int x=0;x<3;++x)
            if(x==1||(y>height-4&&y<height-1))put(x,y,x==1&&y==height-3?0.6f:0.4f);
    } else if(variant==bloodSmear) {
        // Dragged blood: thick where the body was, thinning to streaks
        // along the drag, broken where the streaks run dry.
        float middle=height*0.5f;
        for(int y=0;y<height;++y)for(int x=0;x<width;++x) {
            float along=(x+0.5f)/width,reach=middle*(0.95f-0.6f*along)*(0.85f+0.3f*valueNoise((float)x,0,6,seed));
            float d=std::abs(y+0.5f-middle);
            if(d>=reach)continue;
            float streak=valueNoise((float)x*0.15f,(float)y,1.5f,seed^3u);
            if(streak<0.2f+0.55f*along*d/reach)continue;
            put(x,y,0.12f+0.35f*streak*(1-0.5f*along));
        }
    } else if(variant>=bloodSpeckFirst) {
        // A drop's splat at one texel a unit: a blob and a stray texel or two.
        float center=width*0.5f,radius=1.6f+0.6f*(variant-bloodSpeckFirst)+0.4f*hashUnit(0,0,seed);
        for(int y=0;y<height;++y)for(int x=0;x<width;++x) {
            float d=std::hypot(x+0.5f-center,y+0.5f-center)+(hashUnit(x,y,seed)-0.5f)*0.9f;
            if(d<radius)put(x,y,0.3f+0.3f*(1-d/radius));
        }
        for(int n=int(hash32(seed)%3);n>0;--n) {
            float a=hashUnit(n,1,seed)*2*doomPi,r=radius+1+hashUnit(n,2,seed);
            put((int)(center+std::cos(a)*r),(int)(center+std::sin(a)*r),0.35f);
        }
    } else {
        bool pool=variant==bloodPool;
        int size=width;float center=size*0.5f;
        float phase[3];for(int i=0;i<3;++i)phase[i]=hashUnit(i,variant,seed)*2*doomPi;
        for(int y=0;y<size;++y)for(int x=0;x<size;++x) {
            float dx=x+0.5f-center,dy=y+0.5f-center,d=std::hypot(dx,dy),a=std::atan2(dy,dx);
            float edge=pool?size*0.40f*(0.82f+0.10f*std::sin(2*a+phase[0])+0.06f*std::sin(5*a+phase[1])+0.04f*std::sin(9*a+phase[2]))
                           :size*0.30f*(0.70f+0.22f*std::sin(3*a+phase[0])+0.12f*std::sin(7*a+phase[1]));
            edge+=(valueNoise((float)x,(float)y,4,seed)-0.5f)*size*0.08f;
            if(d>=edge)continue;
            float rim=1-d/edge;
            put(x,y,pool?0.12f+0.30f*valueNoise((float)x,(float)y,6,seed^1u)+0.12f*(1-rim)
                        :0.25f+0.45f*std::pow(rim,0.6f)+0.2f*(valueNoise((float)x,(float)y,3,seed^2u)-0.5f));
        }
        // Droplets thrown past the edge.
        int drops=pool?3:6+int(hash32(seed)%5);
        for(int n=0;n<drops;++n) {
            float a=hashUnit(n,1,seed)*2*doomPi,reach=size*(pool?0.42f:0.30f+0.17f*hashUnit(n,2,seed));
            float radius=pool?1.5f+hashUnit(n,3,seed)*1.5f:0.8f+hashUnit(n,3,seed)*1.4f,shade=0.35f+0.25f*hashUnit(n,4,seed);
            float px=center+std::cos(a)*reach,py=center+std::sin(a)*reach;
            for(int y=(int)(py-radius);y<=(int)(py+radius)+1;++y)for(int x=(int)(px-radius);x<=(int)(px+radius)+1;++x)
                if(std::hypot(x+0.5f-px,y+0.5f-py)<radius)put(x,y,pool?shade*0.6f:shade);
        }
    }
    return images.emplace(key,upload(width,height,pixels.data())).first->second;
}
static int variantOf(int kind) {
    int pick=int(randomRange(0,4));
    return kind==BLOOD_SPECK?bloodSpeckFirst+pick%bloodSpecks:kind==BLOOD_SMEAR?bloodSmear:1+pick%bloodSplats;
}
struct BloodFloor { float x,y,born,size,angle,stretch;int sector,variant;bool flip,ceiling; };
struct BloodWall { int line,side,section;float along,dz,born,size,stretch;int variant;bool flip;int drips;float dripU[2],dripLength[2]; };
struct BloodPool { float x,y,born,size,angle;int sector,tint;const mobj_t *owner=nullptr; };
static std::vector<BloodFloor> bloodFloors;static std::vector<BloodWall> bloodWalls;static std::vector<BloodPool> bloodPools;
static std::vector<BloodWall> severeWallMarks;
static std::vector<BloodPool> severePools;
static std::unordered_map<const mobj_t*,bool> pooledCorpses;
static unsigned bloodLevelSerial;
namespace {
constexpr size_t severeBurstLimit=5,severeElementLimit=25,severeGlobalLimit=250;
constexpr float severeLifetime=.86f,jetTravel=.36f,jetLifetime=.58f,fallGravity=650.f;
constexpr int jetSegments=3;
constexpr int severeFragments=9;
struct SevereJet {
    float dx=0,dy=0,lift=0,travel=jetTravel;
    WallHit wall{};
    bool marked=false;
};
struct SevereFragment {
    float x=0,y=0,z=0,vx=0,vy=0,vz=0;
    float release=0,end=0,size=0;
    int sector=-1;
    bool deposited=false;
    std::array<float,3> position(float t) const {
        return {x+vx*t,y+vy*t,z+vz*t-fallGravity*t*t};
    }
};
struct SevereBurst {
    wp_severe_burst_t event{};
    float born=-1,heading=0;
    uint32_t seed=0;
    int jets=0,fragments=0;
    std::array<SevereJet,4> streams{};
    std::array<SevereFragment,severeFragments> fallout{};
    size_t elements() const {return size_t(jets*(jetSegments+1)+fragments);}
};
std::array<SevereBurst,severeBurstLimit> severeBursts{};
unsigned severeLevel=~0u;
const sector_t *severeSectorAt(float x,float y) {
    return R_PointInSubsector(fixed_t(x*FRACUNIT),fixed_t(y*FRACUNIT))->sector;
}
void planSevereFall(SevereFragment &drop) {
    drop.end=severeLifetime-drop.release;
    const auto finish=drop.position(drop.end);
    const float distance=std::hypot(finish[0]-drop.x,finish[1]-drop.y);
    // Trace the ballistic arc in short chords. A blocked fragment cannot
    // leave a pool in another room, or cross a solid-map obstruction.
    const int steps=std::max(1,int(std::ceil(distance/24.f)));
    for(int n=0;n<steps;++n) {
        const float previous=drop.end*n/steps,next=drop.end*(n+1)/steps;
        const auto a=drop.position(previous),b=drop.position(next);
        const float dx=b[0]-a[0],dy=b[1]-a[1],dz=b[2]-a[2];
        const float reach=std::hypot(dx,dy);
        WallHit hit;
        float stop=1.f;
        if(reach>.01f&&firstSolidWall(a[0],a[1],a[2],dx/reach,dy/reach,dz/reach,reach,hit))
            stop=hit.t/reach;
        if(SV_Active())stop=std::min(stop,SV_Trace(a[0],a[1],a[2],dx,dy,dz,nullptr,nullptr));
        const auto *sector=severeSectorAt(b[0],b[1]);
        const float floor=floorZ(sector);
        if(b[2]<=floor&&a[2]>floor) {
            const float contact=(a[2]-floor)/(a[2]-b[2]);
            if(contact<stop||stop==1.f) {
                drop.end=previous+(next-previous)*contact;
                const auto p=drop.position(drop.end);
                const auto *landing=severeSectorAt(p[0],p[1]);
                if(std::abs(p[2]-floorZ(landing))<=2.f)drop.sector=int(landing-sectors);
                return;
            }
        }
        if(stop<1.f||b[2]<=floor|| (sector->ceilingpic!=skyflatnum&&b[2]>=ceilZ(sector))) {
            drop.end=previous+(next-previous)*stop;
            return;
        }
    }
}
void collectSevereFall(const SevereBurst &burst,const SevereFragment &drop) {
    if(drop.sector<0||drop.sector>=numsectors)return;
    const size_t sector=size_t(drop.sector);
    if(sector<sectorMist.size()&&sectorMist[sector].liquid)return;
    const auto p=drop.position(drop.end);
    if(std::abs(p[2]-floorZ(&sectors[sector]))>3.f)return;
    for(auto &pool:severePools) {
        if(pool.sector!=drop.sector||pool.tint!=burst.event.tint||
           std::hypot(pool.x-p[0],pool.y-p[1])>14.f)continue;
        pool.size=std::min(25.f,pool.size+2.5f);
        return;
    }
    constexpr size_t poolLimit=32;
    if(severePools.size()>=poolLimit)severePools.erase(severePools.begin());
    severePools.push_back({p[0],p[1],burst.born+drop.release+drop.end,
                           7.f+drop.size*2.f,burst.heading,drop.sector,burst.event.tint});
}
void markSevereWall(const SevereBurst &burst,int index) {
    const auto &jet=burst.streams[index];
    const auto &hit=jet.wall;
    WallSpan span;
    if(!wallSpan(lines[hit.line],hit.side,hit.section,span)||span.texture<=0||span.texture==skytexture)return;
    const auto &origin=burst.event.impact;
    const float x=origin.x+jet.dx*jet.travel,y=origin.y+jet.dy*jet.travel,z=origin.z+jet.lift*jet.travel;
    const float ex=span.b.x-span.a.x,ey=span.b.y-span.a.y,len=std::hypot(ex,ey);
    if(len<1||z<span.bottom||z>span.top)return;
    const float along=std::clamp(((x-span.a.x)*ex+(y-span.a.y)*ey)/len,0.f,len);
    constexpr size_t markLimit=32;
    if(severeWallMarks.size()>=markLimit)severeWallMarks.erase(severeWallMarks.begin());
    const uint32_t hash=hash32(burst.seed+uint32_t(index));
    severeWallMarks.push_back({hit.line,hit.side,hit.section,along,z-span.anchor,
        burst.born+jet.travel,18.f+10.f*hashUnit(index,26,burst.seed),1.3f,
        1+int(hash%bloodSplats)+32*burst.event.tint,(hash&4u)!=0,0,{},{}});
}
bool severeBloodOn() {
    return settings.accelerated&&settings.severeBloodspill&&settings.bloodSpray&&
           levelSerial==r_levelserial&&totalkills<200&&bloodOn();
}
void expireSevereBlood(float now) {
    if(severeLevel!=r_levelserial||!severeBloodOn()) {
        severeLevel=r_levelserial;
        severeBursts={};
        severeWallMarks.clear();
        severePools.clear();
        return;
    }
    for(auto &burst:severeBursts) {
        if(burst.born<0)continue;
        if(now<burst.born)burst={};
        else if(now-burst.born>=severeLifetime) {
            for(auto &drop:burst.fallout)
                if(!drop.deposited)collectSevereFall(burst,drop);
            burst={};
        }
    }
}
}
void severeBloodBurst(const wp_severe_burst_t *event) {
    if(!severeBloodOn())return;
    const float now=leveltime/float(TICRATE);
    expireSevereBlood(now);
    auto slot=std::find_if(severeBursts.begin(),severeBursts.end(),
                           [](const SevereBurst &burst){return burst.born<0;});
    if(slot==severeBursts.end())return;
    const uint32_t seed=hash32(uint32_t(event->seed)^uint32_t(event->seed>>32));
    const int jets=2+int(seed%3);
    const int fragments=std::min(severeFragments,int(severeElementLimit)-jets*(jetSegments+1));
    size_t active=0;
    for(const auto &burst:severeBursts)active+=burst.elements();
    if(active+size_t(jets*(jetSegments+1)+fragments)>severeGlobalLimit)return;
    // Aim outward from the struck surface, toward the side the shot came from.
    const float heading=std::atan2(-event->directionY,-event->directionX);
    *slot={*event,now,heading,seed,jets,fragments};
    const auto &origin=event->impact;
    for(int j=0;j<jets;++j) {
        const float spread=(j-(jets-1)*.5f)*.45f;
        const float angle=heading+spread+(hashUnit(j,1,seed)-.5f)*.16f;
        const float speed=325.f+135.f*hashUnit(j,2,seed);
        auto &jet=slot->streams[j];
        const float ux=std::cos(angle),uy=std::sin(angle);
        jet.dx=ux*speed;jet.dy=uy*speed;
        jet.lift=12.f+55.f*hashUnit(j,3,seed);
        if(firstSolidWall(origin.x,origin.y,origin.z,ux,uy,jet.lift/speed,speed*jet.travel,jet.wall))
            jet.travel=jet.wall.t/speed;
        if(SV_Active()) {
            const float travel=jet.travel;
            const float solid=SV_Trace(origin.x,origin.y,origin.z,jet.dx*travel,jet.dy*travel,jet.lift*travel,nullptr,nullptr);
            if(solid<.98f) {jet.travel*=solid;jet.wall={};}
        }
    }
    for(int i=0;i<fragments;++i) {
        const auto &jet=slot->streams[i%jets];
        auto &drop=slot->fallout[i];
        // Later fragments separate farther along the flow, inheriting some
        // of the stream's speed before tumbling along their own arc.
        drop.release=.085f+.29f*hashUnit(i,9,seed);
        const float along=std::min(drop.release,jet.travel)*(.24f+.65f*hashUnit(i,10,seed));
        drop.x=origin.x+jet.dx*along;drop.y=origin.y+jet.dy*along;drop.z=origin.z+jet.lift*along;
        const float direction=std::atan2(jet.dy,jet.dx)+(hashUnit(i,11,seed)-.5f)*1.16f;
        const float speed=std::hypot(jet.dx,jet.dy)*(.20f+.35f*hashUnit(i,12,seed));
        drop.vx=std::cos(direction)*speed;drop.vy=std::sin(direction)*speed;
        drop.vz=jet.lift*.5f+(hashUnit(i,13,seed)-.5f)*100.f;
        drop.size=.95f+1.65f*hashUnit(i,14,seed);
        planSevereFall(drop);
    }
}

static void buildSevereBlood(const Uniforms &camera) {
    const float now=gameSeconds();
    expireSevereBlood(now);
    if(std::none_of(severeBursts.begin(),severeBursts.end(),
                   [](const SevereBurst &burst){return burst.born>=0;}))return;
    const auto &jetMask=bloodImage(bloodJet);
    const auto &speckMask=bloodImage(bloodSpeckFirst);
    auto &jetVertices=effectBatches[-1-(bloodKeyBase+bloodJet)];
    auto &speckVertices=effectBatches[-1-(bloodKeyBase+bloodSpeckFirst)];
    for(auto &burst:severeBursts) {
        if(burst.born<0)continue;
        const float age=now-burst.born;
        const float pressure=1.f-std::clamp((age-.17f)/(jetLifetime-.17f),0.f,1.f);
        const auto &origin=burst.event.impact;
        const auto color=bloodColorForTint(.54f,burst.event.tint);
        auto vertex=[&](const std::array<float,3> &p,const std::array<float,3> &axis,
                        float halfWidth,float u,float v,float opacity) {
            Vertex result={p[0]+axis[0]*halfWidth,p[1]+axis[1]*halfWidth,
                           p[2]+axis[2]*halfWidth,u,v,opacity,0u};
            result.red=color[0];result.green=color[1];result.blue=color[2];
            return result;
        };
        for(int j=0;j<burst.jets;++j) {
            auto &jet=burst.streams[j];
            if(!jet.marked&&jet.wall.line>=0&&age>=jet.travel) {
                markSevereWall(burst,j);
                jet.marked=true;
            }
            const float front=std::min(age+.018f,jet.travel);
            const float tail=std::max(0.f,(age-.27f)*.82f);
            if(front<=tail)continue;
            std::array<Vertex,jetSegments+1> left{},right{};
            for(int s=0;s<=jetSegments;++s) {
                const float u=s/float(jetSegments), t=tail+(front-tail)*u;
                const std::array<float,3> p={origin.x+jet.dx*t,origin.y+jet.dy*t,origin.z+jet.lift*t};
                const float vx=camera.eye[0]-p[0],vy=camera.eye[1]-p[1],vz=camera.eye[2]-p[2];
                std::array<float,3> side={jet.dy*vz-jet.lift*vy,jet.lift*vx-jet.dx*vz,jet.dx*vy-jet.dy*vx};
                const float length=std::hypot(side[0],side[1],side[2]);
                for(int axis=0;axis<3;++axis)side[axis]=length>.001f?side[axis]/length:camera.right[axis];
                const float surge=.85f+.15f*std::sin(age*47.f+j*2.1f-u*4.f);
                const float width=(1.2f+pressure*3.8f)*(1.f-.32f*u)*surge;
                const float opacity=pressure*(.95f-.25f*u)*surge;
                left[s]=vertex(p,side,-width,u*jetMask.width,0,opacity);
                right[s]=vertex(p,side,width,u*jetMask.width,float(jetMask.height),opacity);
            }
            for(int s=0;s<jetSegments;++s) {
                const float split=.25f+.12f*hashUnit(j,s,burst.seed);
                // Lose the outer sections first, then leave a gap near
                // the wound while the last piece keeps moving outward.
                if((s==2&&age>split+.08f)||(s==0&&age>split+.12f)||(s==1&&age>split+.20f))continue;
                quad(jetVertices,left[s],left[s+1],right[s+1],right[s]);
            }
        }
        for(int i=0;i<burst.fragments;++i) {
            auto &drop=burst.fallout[i];
            const float t=age-drop.release;
            if(t<=0)continue;
            if(t>=drop.end) {
                if(!drop.deposited) {collectSevereFall(burst,drop);drop.deposited=true;}
                continue;
            }
            const auto p=drop.position(t);
            const std::array<float,3> right={camera.right[0],camera.right[1],camera.right[2]};
            const float size=drop.size,elongation=1.f+.75f*std::min(1.f,t*3.f);
            auto corner=[&](float x,float y,float u,float v) {
                auto point=p;
                for(int axis=0;axis<3;++axis)point[axis]+=camera.up[axis]*y*size*elongation;
                return vertex(point,right,x*size,u,v,.86f*std::clamp((severeLifetime-age)/.16f,0.f,1.f));
            };
            quad(speckVertices,corner(-1,1,0,0),corner(1,1,float(speckMask.width),0),
                 corner(1,-1,float(speckMask.width),float(speckMask.height)),
                 corner(-1,-1,0,float(speckMask.height)));
        }
    }
}
constexpr size_t maxFloorBlood=320,maxWallBlood=160;
static void checkBloodLevel() {
    if(bloodLevelSerial==r_levelserial)return;
    bloodFloors.clear();bloodWalls.clear();bloodPools.clear();pooledCorpses.clear();bloodLevelSerial=r_levelserial;
}
void bloodFloorSplat(float x,float y,int sector,float size,float angle,float stretch,bool ceiling,float born,int kind) {
    if(!bloodOn()||sector<0||sector>=numsectors)return;
    checkBloodLevel();
    if(bloodFloors.size()>=maxFloorBlood)bloodFloors.erase(bloodFloors.begin());
    bloodFloors.push_back({x,y,born,size,angle,std::max(1.0f,stretch),sector,variantOf(kind)+32*bloodSourceTint,kind!=BLOOD_SMEAR&&randomRange(0,1)<0.5f,ceiling});
}
void bloodWallSplat(const WallHit &hit,float x,float y,float z,float size,float stretch,float born,int kind,bool drips) {
    if(!bloodOn())return;
    checkBloodLevel();
    WallSpan span;if(!wallSpan(lines[hit.line],hit.side,hit.section,span))return;
    float ex=span.b.x-span.a.x,ey=span.b.y-span.a.y,len=std::hypot(ex,ey);if(len<1)return;
    if(z<span.bottom||z>span.top)return;
    float along=std::clamp(((x-span.a.x)*ex+(y-span.a.y)*ey)/len,0.0f,len);
    if(bloodWalls.size()>=maxWallBlood)bloodWalls.erase(bloodWalls.begin());
    BloodWall wall={hit.line,hit.side,hit.section,along,z-span.anchor,born,size,std::max(1.0f,stretch),variantOf(kind)+32*bloodSourceTint,randomRange(0,1)<0.5f,0,{},{}};
    // Runs from a heavy splat: one or two, down from its lower half.
    if(drips) {
        wall.drips=1+(randomRange(0,1)<0.4f);
        for(int n=0;n<wall.drips;++n) {wall.dripU[n]=randomRange(-0.3f,0.3f)*size;wall.dripLength[n]=randomRange(8,26);}
    }
    bloodWalls.push_back(wall);
}
// Hitscan blood without the spray: splats land on the floor a little past
// the hit after their fall, and with harder hits on a wall close behind.
static void placeSplats(float x,float y,float z,float dx,float dy,int damage) {
    float now=gameSeconds();
    int count=1+(randomRange(0,30)<damage);
    for(int i=0;i<count;++i) {
        float reach=randomRange(4,36),side=randomRange(-10,10);
        float px=x+dx*reach-dy*side,py=y+dy*reach+dx*side,sx=px-x,sy=py-y,span=std::hypot(sx,sy);
        WallHit wall;
        if(span>0.5f&&firstSolidWall(x,y,z,sx/span,sy/span,0,span,wall))continue;
        const sector_t *sector=R_PointInSubsector((fixed_t)(px*FRACUNIT),(fixed_t)(py*FRACUNIT))->sector;
        float drop=z-floorZ(sector);
        if(sector->floorpic==skyflatnum||drop<0||drop>160)continue;
        float fall=(70+std::sqrt(4900+2450*drop))/1225; // Doom gravity after the 2 unit/tic hop.
        bloodFloorSplat(px,py,int(sector-sectors),randomRange(14,24)*(damage>=15?1.25f:1.0f),randomRange(0,2*doomPi),1,false,now+fall,BLOOD_SPLAT);
    }
    if(randomRange(0,1)>=std::min(0.75f,damage/16.0f))return;
    float sz=z+randomRange(-10,6);
    WallHit hit;
    if(!firstSolidWall(x,y,sz,dx,dy,0,96,hit))return;
    bloodWallSplat(hit,x+dx*hit.t,y+dy*hit.t,sz,randomRange(16,26),1,now+hit.t/400,BLOOD_SPLAT,false);
}
void bloodHit(mobj_t *target,mobj_t *shooter,fixed_t fx,fixed_t fy,fixed_t fz,fixed_t fdx,fixed_t fdy,fixed_t slope,int damage) {
    BloodSourceScope bloodSource(target);
    shotThrough(target,fx,fy,fz,fdx,fdy,damage);
    bodyHit(target,fx,fy,fz,fdx,fdy,damage);
    woundHit(target,fx,fy,fz,fdx,fdy,slope,damage,shooter);
    if(!bloodOn()||(settings.bloodSpray&&combatHeadSprayHandled(target,shooter)))return;
    checkBloodLevel();
    float x=units(fx),y=units(fy),z=units(fz),dx=units(fdx),dy=units(fdy),length=std::hypot(dx,dy);
    if(length<0.001f)return;
    dx/=length;dy/=length;
    if(settings.bloodSpray)sprayHit(target,x,y,z,dx,dy,units(slope),damage);
    else placeSplats(x,y,z,dx,dy,damage);
}
// A rotated rectangle of blood clipped to its sector's floor (or ceiling)
// polygons: size across, stretched along angle.
static void flatBlood(int variant,int sectorIndex,float cx,float cy,float size,float angle,float stretch,bool flip,bool ceiling,
                      float wet,float dark,float wash,const Uniforms &camera) {
    const sector_t &sector=sectors[sectorIndex];
    if((ceiling?sector.ceilingpic:sector.floorpic)==skyflatnum)return;
    const Image &image=bloodImage(variant);
    float h=size*0.5f,w=h*stretch*image.width/std::max(1,image.height),c=std::cos(angle),s=std::sin(angle),reach=std::hypot(w,h);
    Point across={c,s},up={-s,c};
    auto corner=[&](float u,float v){return Point{cx+across.x*u+up.x*v,cy+across.y*u+up.y*v};};
    std::vector<Point> square={corner(-w,-h),corner(w,-h),corner(w,h),corner(-w,h)};
    float lift=decalLift(cx,cy,camera),z=ceiling?ceilZ(&sector)-lift:floorZ(&sector)+lift,light=lighting(&sector);
    unsigned mode=MODE_BLOOD|sectorMode(&sector);
    auto &out=batches[bloodKeyBase+variant];
    for(int leaf:sectorFloors[sectorIndex]) {
        const auto &b=floorBounds[leaf];
        if(b[2]<cx-reach||b[0]>cx+reach||b[3]<cy-reach||b[1]>cy+reach)continue;
        auto polygon=floors[leaf];
        for(size_t n=0;n<square.size()&&!polygon.empty();++n) {
            Point a=square[n],d=square[(n+1)%square.size()];
            polygon=clip(polygon,a,{d.x-a.x,d.y-a.y},false);
        }
        auto vertex=[&](Point p) {
            float u=((p.x-cx)*across.x+(p.y-cy)*across.y+w)/(2*w),v=(h-((p.x-cx)*up.x+(p.y-cy)*up.y))/size;
            return Vertex{p.x,p.y,z,(flip?1-u:u)*image.width,v*image.height,light,mode,wet,dark,wash};
        };
        for(size_t n=1;n+1<polygon.size();++n)
            for(Point p:{polygon[0],polygon[n],polygon[n+1]})out.push_back(vertex(p));
    }
}
// Wet blood steps from glossy to matte, darkening as it dries; values move
// in quarters and eighths at tic-rate ages, never smoothly.
static void bloodAge(float age,float wetFor,float dryOver,float &wet,float &dark) {
    float fresh=std::clamp(1-(age-wetFor)/dryOver,0.0f,1.0f);
    wet=settings.bloodShine?std::ceil(fresh*4)/4:0;
    dark=1-std::round((1-fresh)*0.28f*8)/8;
}
// Rained on (under the open sky, or on the floor a broken window lets the
// rain soak), blood never dries dark: it pales over 6 seconds, its red
// fading toward a dull grey in four steps (never brighter), on the floor
// spreading as it thins (spread: up to 1.6 times as big), then the rain
// takes it texel by texel, all gone 21 seconds after it was spilled. Not a
// simulation: exaggerated, so it shows. Returns what the shader reads
// (world.frag): the pale step (0-4) times two plus how much is washed away
// (in eighths, below 1); 2 or more with the wash at 1: gone.
static bool rainedOn(int sector,float x,float y) {
    if(!settings.weather||rainNow<=0.01f)return false;
    return sectors[sector].ceilingpic==skyflatnum||rainSoaked(sector,x,y);
}
static float rainWash(bool rained,float age,float &dark,float &spread) {
    spread=1;
    if(!rained)return 0;
    float pale=std::ceil(std::clamp(age/6,0.0f,1.0f)*4);
    dark=1;spread=1+0.15f*pale;
    float wash=std::clamp(std::floor((age-6)/15*8)/8,0.0f,1.0f);
    return wash>=1?-1:pale*2+wash;
}
void buildBlood(const Uniforms &camera) {
    buildSevereBlood(camera);
    if(!settings.blood||bloodPalette().empty())return;
    checkBloodLevel();
    float now=gameSeconds();
    // Pools: corpses that came to rest, once each, sized to the body.
    for(auto &entry:pooledCorpses)entry.second=false;
    // Only remove the pool actually created for the same live corpse.
    // Other blood impacts/splats and neighboring corpse pools stay intact.
    // Keys in BloodPool are identities only: never dereference cached pointers.
    std::unordered_map<const mobj_t*,std::array<float,2>> driedOwners;
    for(int i=0;i<numsectors;++i)for(mobj_t *thing=sectors[i].thinglist;thing;thing=thing->snext) {
        if(!(thing->flags&MF_CORPSE)||(thing->flags&MF_NOBLOOD))continue;
        if(corpseDecayStage(thing)>=2) {
            // Preserve an existing no-recreate marker without allocating an
            // entry for corpses that never had a blood pool in the first place.
            auto pooled=pooledCorpses.find(thing);
            if(pooled!=pooledCorpses.end())pooled->second=true;
            driedOwners.emplace(thing,std::array<float,2>{units(thing->x),units(thing->y)});
            continue;
        }
        auto seen=pooledCorpses.find(thing);
        if(seen!=pooledCorpses.end()) {seen->second=true;continue;}
        if(thing->z>thing->floorz||thing->momx||thing->momy||sectors[i].floorpic==skyflatnum)continue;
        pooledCorpses[thing]=true;
        if(bloodPools.size()>=64)bloodPools.erase(bloodPools.begin());
        bloodPools.push_back({units(thing->x),units(thing->y),now,std::clamp(units(thing->radius)*2.4f,28.0f,64.0f),randomRange(0,2*doomPi),i,monsterBloodTint(thing),thing});
    }
    for(auto it=pooledCorpses.begin();it!=pooledCorpses.end();) it=it->second?std::next(it):pooledCorpses.erase(it);
    // Free CPU pool records and skip their per-frame geometry. Compare owner
    // identity AND original pool location to avoid mismatching nearby corpses.
    bloodPools.erase(std::remove_if(bloodPools.begin(),bloodPools.end(),[&](const BloodPool &pool) {
        auto it=driedOwners.find(pool.owner);
        return it!=driedOwners.end() &&
               std::abs(it->second[0]-pool.x)<0.01f && std::abs(it->second[1]-pool.y)<0.01f;
    }),bloodPools.end());
    for(const auto &pool:bloodPools) {
        float age=now-pool.born;
        // The pool spreads over four seconds in eight steps.
        float grow=0.3f+0.7f*std::min(1.0f,std::floor(age*2)/8);
        float wet,dark;bloodAge(age,4,6,wet,dark);
        float spread,wash=rainWash(rainedOn(pool.sector,pool.x,pool.y),age,dark,spread);
        if(wash<0)continue;
        flatBlood(bloodPool+32*pool.tint,pool.sector,pool.x,pool.y,pool.size*grow*spread,pool.angle,1,false,false,wet,dark,wash,camera);
    }
    severePools.erase(std::remove_if(severePools.begin(),severePools.end(),
        [&](const BloodPool &pool){return now<pool.born||now-pool.born>45.f;}),severePools.end());
    for(const auto &pool:severePools) {
        const float age=now-pool.born;
        if(camera.eye[2]<floorZ(&sectors[pool.sector]))continue;
        const float grow=.45f+.55f*std::min(1.f,age*1.5f);
        float wet,dark;bloodAge(age,2,6,wet,dark);
        float spread,wash=rainWash(rainedOn(pool.sector,pool.x,pool.y),age,dark,spread);
        if(wash>=0)flatBlood(bloodPool+32*pool.tint,pool.sector,pool.x,pool.y,
            pool.size*grow*spread,pool.angle,1,false,false,wet,dark,wash,camera);
    }
    bloodFloors.erase(std::remove_if(bloodFloors.begin(),bloodFloors.end(),[&](const BloodFloor &d){return now-d.born>180;}),bloodFloors.end());
    for(const auto &d:bloodFloors) {
        if(now<d.born)continue;
        if(d.ceiling?camera.eye[2]>ceilZ(&sectors[d.sector]):camera.eye[2]<floorZ(&sectors[d.sector]))continue; // Back of the flat.
        float wet,dark;bloodAge(now-d.born,0,3,wet,dark);
        float spread,wash=rainWash(!d.ceiling&&rainedOn(d.sector,d.x,d.y),now-d.born,dark,spread);
        if(wash<0)continue;
        flatBlood(d.variant,d.sector,d.x,d.y,d.size*spread,d.angle,d.stretch,d.flip,d.ceiling,d.ceiling?0:wet,dark,wash,camera);
    }
    // Wall splats stay matte; thin spray doesn't glint believably.
    bloodWalls.erase(std::remove_if(bloodWalls.begin(),bloodWalls.end(),[&](const BloodWall &d){return now-d.born>180;}),bloodWalls.end());
    severeWallMarks.erase(std::remove_if(severeWallMarks.begin(),severeWallMarks.end(),
        [&](const BloodWall &d){return now<d.born||now-d.born>45;}),severeWallMarks.end());
    for(const auto *wallMarks:{&bloodWalls,&severeWallMarks})for(const auto &d:*wallMarks) {
        if(now<d.born)continue;
        WallSpan span;if(!wallSpan(lines[d.line],d.side,d.section,span))continue;
        float ex=span.b.x-span.a.x,ey=span.b.y-span.a.y,len=std::hypot(ex,ey);
        if(len<1||ex*(camera.eye[1]-span.a.y)-ey*(camera.eye[0]-span.a.x)>0)continue; // Back of the wall.
        const Image &image=bloodImage(d.variant);
        float zc=span.anchor+d.dz,h=d.size*0.5f,hu=h*d.stretch*image.width/std::max(1,image.height);
        float u0=std::max(0.0f,d.along-hu),u1=std::min(len,d.along+hu),z0=std::max(span.bottom,zc-h),z1=std::min(span.top,zc+h);
        if(u1<=u0||z1<=z0)continue;
        float wet,dark;bloodAge(now-d.born,0,3,wet,dark);
        const line_t &line=lines[d.line];const sector_t *front=sides[line.sidenum[d.side]].sector;
        float spread,wash=rainWash(settings.weather&&rainNow>0.01f&&front->ceilingpic==skyflatnum,now-d.born,dark,spread);
        if(wash<0)continue;
        float light=lighting(front);
        if(std::abs(ey)<0.01f)light=std::max(0.1f,light-0.04f);
        if(std::abs(ex)<0.01f)light=std::min(1.0f,light+0.04f);
        float tx=ex/len,ty=ey/len,lift=decalLift(span.a.x+tx*d.along,span.a.y+ty*d.along,camera)+0.25f,nx=ty*lift,ny=-tx*lift;
        unsigned mode=MODE_BLOOD|sectorMode(front);
        auto vertex=[&](float u,float z,float s,float t) {
            Vertex v={span.a.x+tx*u+nx,span.a.y+ty*u+ny,z,s,t,light,mode,0,dark,wash};
            bakeWallCoordinates(line,d.side,u,v);
            return v;
        };
        auto texU=[&](float u){float t=(u-(d.along-hu))/(2*hu);return (d.flip?1-t:t)*image.width;};
        auto texV=[&](float z){return ((zc+h)-z)/(2*h)*image.height;};
        quad(batches[bloodKeyBase+d.variant],vertex(u0,z1,texU(u0),texV(z1)),vertex(u1,z1,texU(u1),texV(z1)),
             vertex(u1,z0,texU(u1),texV(z0)),vertex(u0,z0,texU(u0),texV(z0)));
        // Drips run down from the splat's lower half over three seconds,
        // slowing, in four-tic steps: the bead leads, the run follows.
        const Image &drip=bloodImage(bloodDrip+(d.variant/32)*32);
        for(int n=0;n<d.drips;++n) {
            float age=std::floor((now-d.born-0.3f)*TICRATE/4)*4/TICRATE;
            if(age<=0)continue;
            float grow=std::min(1.0f,age/3),length=std::round(d.dripLength[n]*(1-(1-grow)*(1-grow))),u=d.along+d.dripU[n];
            float top=zc-h*0.35f,bottom=std::max(span.bottom,top-length);
            if(length<1||top<=bottom||u<0.5f||u>len-1.5f)continue;
            float shown=std::min((float)drip.height,top-bottom);
            quad(batches[bloodKeyBase+bloodDrip+(d.variant/32)*32],vertex(u,top,0,drip.height-shown),vertex(u+drip.width,top,(float)drip.width,drip.height-shown),
                 vertex(u+drip.width,bottom,(float)drip.width,(float)drip.height),vertex(u,bottom,0,(float)drip.height));
        }
    }
    buildSpray(camera);
}
void bloodShutdown() {
    severeBursts={};severeLevel=~0u;
    severeWallMarks.clear();
    severePools.clear();
    bloodFloors.clear();bloodWalls.clear();bloodPools.clear();pooledCorpses.clear();bloodShades.clear();bloodShadesLoaded=false;
    sprayShutdown();
}
} // namespace doom3d

#ifdef DOOM_TEST_DRIVER
extern "C" int I_TestSevereWallMarks() {return int(doom3d::severeWallMarks.size());}
extern "C" int I_TestSeverePools() {return int(doom3d::severePools.size());}
extern "C" int I_TestCombatBloodWalls(int *out,int capacity) {
    int count=(int)doom3d::bloodWalls.size();
    for(int i=0;i<std::min(count,capacity);++i)out[i]=doom3d::bloodWalls[count-std::min(count,capacity)+i].line;
    return count;
}
#endif

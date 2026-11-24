/* The textured fragment path owns particles only. Doom sectors and the
   existing blood pass own all world surfaces and lasting marks. */
#include "internal.h"
#include "giblet_atlas.generated.h"
#include "../../../map_world.h"
#include <limits>

extern "C" {
#include "p_local.h"
#include "p_solid.h"
}
extern const lab::MapWorld *SV_MapWorld();

namespace doom3d {
namespace {
using namespace hellforge::giblets;

// Reserved G1 atlas key. Blood decals are at -0x40000000 and WAD images
// use their own lump-derived keys. All tints occupy bands in this image.
constexpr int atlasKey=-0x45000000;
constexpr int tintBands=3;
constexpr int candidateLimit=256, crossingLimit=64;
constexpr size_t cutLimit=256;
System giblets;
std::vector<unsigned> lineSeen;
unsigned stamp=0, mapSerial=0;
int lastTic=-1;
bool debug=false;
const char *atlasStatus="atlas_unavailable";

struct MarkBudget { uint64_t id=0; int walls=-1; };
std::array<MarkBudget,System::MaxPieces> markBudgets;
struct Metrics {
    uint64_t candidates=0, queryStops=0, collisionNS=0, wall=0, flat=0, rejected=0;
    unsigned quads=0,batches=0,atlasLoads=0,atlasHits=0;
    int maxEventWall=0;
};
Metrics metrics;

bool volumeLab() {return !SV_MapMode()&&SV_Active();}

bool uncertainSweep(Vec3 a,Vec3 b,float radius) {
    const auto *world=SV_MapWorld();
    if(!world||!world->cutCount())return false;
    // The map cut volumes may remove the original skin. Without an original
    // linedef/sector contact there is no safe owner for a lasting mark.
    if(world->cutCount()>cutLimit)return true;
    const float reach=radius+.5f*std::max(std::abs(b.x-a.x),std::abs(b.y-a.y));
    return world->affected({(a.x+b.x)*.5f,(a.y+b.y)*.5f,std::min(a.z,b.z)-radius},
                          reach,std::abs(a.z-b.z)+2*radius);
}

bool enabled() {
    return settings.accelerated&&settings.goreLevel==1&&settings.goreFragments&&
           settings.blood&&totalkills<200&&!netgame&&!demoplayback&&!demorecording&&gamestate==GS_LEVEL&&
           levelSerial==r_levelserial&&mapSerial==r_levelserial&&!volumeLab();
}
int tintBand(Tint tint) {return tint==Tint::Blue?1:tint==Tint::Green?2:0;}
Vec3 pointAt(Vec3 a,Vec3 b,float t) {
    return {a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t};
}
bool worldPoint(Vec3 p) {
    // A conservative coordinate bound keeps conversion to signed fixed point
    // representable on both classic and wide-coordinate Doom builds.
    constexpr double limit=double(std::numeric_limits<fixed_t>::max())/FRACUNIT;
    return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&
           std::abs(double(p.x))<limit&&std::abs(double(p.y))<limit;
}
const sector_t *sectorAt(Vec3 p) {
    if(!worldPoint(p))return nullptr;
    const auto *sub=R_PointInSubsector(fixed_t(p.x*FRACUNIT),fixed_t(p.y*FRACUNIT));
    return sub?sub->sector:nullptr;
}
bool liquid(int sector) {
    return sector>=0&&size_t(sector)<sectorMist.size()&&sectorMist[sector].liquid;
}

struct Sweep {
    Vec3 a,b;
    float radius;
    TraceHit hit{};
    bool found=false;
    std::array<float,crossingLimit> crossings{};
    size_t count=0;

    void accept(float t,Vec3 center,Vec3 contact,Vec3 normal,SurfaceRef ref,bool mark) {
        if(t<0||t>1||(found&&t>=hit.fraction))return;
        found=true;
        hit={center,contact,normal,t,ref,mark};
    }
    void wall(int index) {
        const line_t &line=lines[index];
        const float x=units(line.v1->x),y=units(line.v1->y);
        const float ex=units(line.v2->x)-x,ey=units(line.v2->y)-y;
        const float len=std::hypot(ex,ey);
        if(len<.001f)return;
        const float ux=ex/len,uy=ey/len;
        const float signedDistance=(a.x-x)*uy-(a.y-y)*ux;
        const int side=signedDistance<0?1:0;
        const float nx=side?-uy:uy,ny=side?ux:-ux;
        const float dx=b.x-a.x,dy=b.y-a.y,velocity=dx*nx+dy*ny;
        if(velocity>=-1e-5f)return;
        const float depth=std::abs(signedDistance);
        const float crossing=-depth/velocity;
        const auto along=[&](Vec3 p){return (p.x-x)*ux+(p.y-y)*uy;};
        if(crossing>0&&crossing<1) {
            const float c=along(pointAt(a,b,crossing));
            if(c>=0&&c<=len) {
                if(count==crossings.size()) {count++;return;}
                crossings[count++]=crossing;
            }
        }

        float t=std::max(0.f,(radius-depth)/velocity);
        if(t>1||(found&&t>=hit.fraction))return;
        Vec3 center=pointAt(a,b,t);
        float u=along(center);
        Vec3 normal={nx,ny,0};
        const bool end=u<0||u>len;
        if(end) {
            u=std::clamp(u,0.f,len);
            const float ox=a.x-(x+ux*u),oy=a.y-(y+uy*u);
            const float speed2=dx*dx+dy*dy, approach=ox*dx+oy*dy;
            const float discriminant=approach*approach-speed2*(ox*ox+oy*oy-radius*radius);
            if(approach>=0||discriminant<0)return;
            t=std::max(0.f,(-approach-std::sqrt(discriminant))/speed2);
            if(t>1||(found&&t>=hit.fraction))return;
            center=pointAt(a,b,t);
            const float vx=center.x-(x+ux*u),vy=center.y-(y+uy*u),length=std::hypot(vx,vy);
            if(length>.001f)normal={vx/length,vy/length,0};
        }
        const auto *front=side?line.backsector:line.frontsector;
        const auto *back=side?line.frontsector:line.backsector;
        if(!front){accept(t,center,center,normal,{},false);return;}
        int section=0;
        if(back) {
            const float low=units(back->floorheight),high=units(back->ceilingheight);
            if(center.z+radius>=high&&high<units(front->ceilingheight)) {
                if(front->ceilingpic==skyflatnum&&back->ceilingpic==skyflatnum) {
                    accept(t,center,center,normal,{Surface::Sky},false);return;
                }
                section=1;
            } else if(center.z-radius<=low&&low>units(front->floorheight))section=2;
            else return; // Clear portal, or its planes are owned by the adjacent sectors.
        }
        const Vec3 contact={x+ux*u,y+uy*u,center.z};
        center={contact.x+normal.x*radius,contact.y+normal.y*radius,contact.z};
        WallSpan span;
        const bool mark=!end&&wallSpan(line,side,section,span)&&span.texture>0&&span.texture<numtextures&&
                        span.texture!=skytexture&&contact.z>=span.bottom&&contact.z<=span.top;
        accept(t,center,contact,normal,{Surface::Wall,index,side,section},mark);
    }

    void planes(float first,float last) {
        const auto *sector=sectorAt(pointAt(a,b,(first+last)*.5f));
        if(!sector){accept(first,a,a,{}, {},false);return;}
        const int index=int(sector-sectors);
        const float floor=units(sector->floorheight),ceiling=units(sector->ceilingheight);
        if(ceiling-floor<2*radius){accept(first,a,a,{}, {},false);return;}
        const float dz=b.z-a.z;
        const float low=floor+radius,high=ceiling-radius;
        const float from=pointAt(a,b,first).z,to=pointAt(a,b,last).z;
        const auto plane=[&](bool top,float z,float time) {
            if(time>last)return;
            const int pic=top?sector->ceilingpic:sector->floorpic;
            const auto type=pic==skyflatnum?Surface::Sky:!top&&liquid(index)?Surface::Liquid:top?Surface::Ceiling:Surface::Floor;
            Vec3 center=pointAt(a,b,time),contact=center;
            center.z=z;contact.z=top?ceiling:floor;
            accept(time,center,contact,{0,0,top?-1.f:1.f},
                   {type,-1,-1,-1,index},pic>=0&&pic<numflats&&(type==Surface::Floor||type==Surface::Ceiling));
        };
        if(from<low||(dz<0&&to<=low))
            plane(false,low,from<=low?first:std::clamp((low-a.z)/dz,first,last));
        if(from>high||(dz>0&&to>=high))
            plane(true,high,from>=high?first:std::clamp((high-a.z)/dz,first,last));
    }
};

bool traceGiblet(Vec3 a,Vec3 b,float radius,TraceHit &out) {
    const auto stop=[&](){++metrics.queryStops;out={};return true;};
    if(!worldPoint(a)||!worldPoint(b)||!std::isfinite(radius)||radius<=0||radius>3||
       !blockmap||!blockmaplump||bmapwidth<=0||bmapheight<=0||lineSeen.size()!=size_t(numlines))return stop();
    if(uncertainSweep(a,b,radius))return stop();
    const double ox=units(bmaporgx),oy=units(bmaporgy);
    const double bx0=std::floor((std::min(a.x,b.x)-radius-ox)/MAPBLOCKUNITS);
    const double by0=std::floor((std::min(a.y,b.y)-radius-oy)/MAPBLOCKUNITS);
    const double bx1=std::floor((std::max(a.x,b.x)+radius-ox)/MAPBLOCKUNITS);
    const double by1=std::floor((std::max(a.y,b.y)+radius-oy)/MAPBLOCKUNITS);
    if(bx1<0||by1<0||bx0>=bmapwidth||by0>=bmapheight||bx1-bx0>4||by1-by0>4)return stop();
    const int x0=int(std::max(0.,bx0)),x1=int(std::min(double(bmapwidth-1),bx1));
    const int y0=int(std::max(0.,by0)),y1=int(std::min(double(bmapheight-1),by1));
    if(++stamp==0){std::fill(lineSeen.begin(),lineSeen.end(),0);++stamp;}
    Sweep sweep{a,b,radius};
    int candidates=0, entries=0;
    for(int y=y0;y<=y1;++y)
        for(int x=x0;x<=x1;++x)
            for(const int32_t *list=blockmaplump+blockmap[size_t(y)*bmapwidth+x];*list!=-1;++list) {
                if(++entries>candidateLimit*4)return stop();
                const int index=*list;
                if(index<0||index>=numlines||lineSeen[index]==stamp)continue;
                lineSeen[index]=stamp;
                if(++candidates>candidateLimit)return stop();
                ++metrics.candidates;
                sweep.wall(index);
                if(sweep.count>sweep.crossings.size())return stop();
            }
    std::sort(sweep.crossings.begin(),sweep.crossings.begin()+sweep.count);
    float first=0;
    for(size_t i=0;i<=sweep.count;++i) {
        const float last=std::min(i==sweep.count?1.f:sweep.crossings[i],sweep.hit.fraction);
        if(last>first)sweep.planes(first,last);
        if(last>=sweep.hit.fraction||last>=1.f)break;
        first=last;
    }
    if(sweep.found)out=sweep.hit;
    return sweep.found;
}

bool hostTrace(void*,Vec3 a,Vec3 b,float radius,TraceHit &out) {
    if(!debug)return traceGiblet(a,b,radius,out);
    const auto begin=SDL_GetTicksNS();
    const bool hit=traceGiblet(a,b,radius,out);
    metrics.collisionNS+=SDL_GetTicksNS()-begin;
    return hit;
}

void gibletImpact(void*,const Impact &impact) {
    auto it=std::find_if(markBudgets.begin(),markBudgets.end(),[&](const MarkBudget &entry){return entry.id==impact.eventId;});
    if(!settings.goreWallBlood||it==markBudgets.end()||it->walls<0){++metrics.rejected;return;}
    const auto &ref=impact.surface;
    const auto p=impact.contact;
    const float now=leveltime/float(TICRATE);
    // The host invokes impacts synchronously on the spray tick's thread.
    const BloodSourceScope tint(tintBand(impact.tint));
    if(ref.type==Surface::Wall) {
        WallSpan span;
        if(!it->walls||ref.line<0||ref.line>=numlines||ref.side<0||ref.side>1||
           ref.section<0||ref.section>2||!wallSpan(lines[ref.line],ref.side,ref.section,span)||
           span.texture<=0||span.texture>=numtextures||span.texture==skytexture||p.z<span.bottom||p.z>span.top) {
            ++metrics.rejected;return;
        }
        bloodWallSplat({ref.line,ref.side,ref.section,0},p.x,p.y,p.z,impact.markSize,
                       impact.cling?1.5f:1.f,now,BLOOD_SPLAT,impact.cling);
        --it->walls;++metrics.wall;++combatFeedbackStats.wallSplats;
        metrics.maxEventWall=std::max(metrics.maxEventWall,3-it->walls);
    } else if((ref.type==Surface::Floor||ref.type==Surface::Ceiling)&&ref.sector>=0&&ref.sector<numsectors) {
        const bool ceiling=ref.type==Surface::Ceiling;
        const int pic=ceiling?sectors[ref.sector].ceilingpic:sectors[ref.sector].floorpic;
        if(pic<0||pic>=numflats||pic==skyflatnum||(!ceiling&&liquid(ref.sector))){++metrics.rejected;return;}
        bloodFloorSplat(p.x,p.y,ref.sector,impact.markSize,0,1,ceiling,now,BLOOD_SPLAT);
        ++metrics.flat;
    } else ++metrics.rejected;
}

bool loadAtlas(bool uploadTexture) {
    using namespace giblet_art;
    SDL_Surface *source=SDL_LoadPNG_IO(SDL_IOFromConstMem(png,sizeof(png)),true);
    if(!source)return false;
    SDL_Surface *rgba=SDL_ConvertSurface(source,SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(source);
    if(!rgba)return false;
    const int pal=W_CheckNumForName((char*)"PLAYPAL");
    if(rgba->w!=width||rgba->h!=height||pal<0||W_LumpLength(pal)<768) {
        SDL_DestroySurface(rgba);return false;
    }
    const auto *palette=(const byte*)W_CacheLumpNum(pal,PU_CACHE);
    std::unordered_map<unsigned,std::array<byte,tintBands>> colors;
    std::vector<byte> packed(size_t(width)*height*tintBands*2);
    for(int y=0;y<height;++y)for(int x=0;x<width;++x) {
        const auto *rgb=(const byte*)rgba->pixels+y*rgba->pitch+x*4;
        if(!rgb[3])continue;
        const unsigned key=unsigned(rgb[0])<<16 | unsigned(rgb[1])<<8 | rgb[2];
        auto [it,added]=colors.try_emplace(key);
        if(added) {
            int best=INT_MAX,index=0;
            for(int i=0;i<256;++i) {
                const int dr=int(rgb[0])-palette[i*3],dg=int(rgb[1])-palette[i*3+1],db=int(rgb[2])-palette[i*3+2];
                const int error=dr*dr+dg*dg+db*db;
                if(error<best){best=error;index=i;}
            }
            it->second={byte(index),bloodTintIndex(byte(index),1),bloodTintIndex(byte(index),2)};
        }
        for(int band=0;band<tintBands;++band) {
            const size_t out=(size_t(band*height+y)*width+x)*2;
            packed[out]=it->second[band];packed[out+1]=rgb[3];
        }
    }
    SDL_DestroySurface(rgba);
    Image image;
    image.width=width;image.height=height*tintBands;image.opaque=false;
    if(uploadTexture) {
        image.texture=gpuCreateTexture(GpuFormat::RG8,image.width,image.height,packed.data(),width*2,1);
        if(!image.texture)return false;
    }
    // World lighting samples the same indexed pixels on the CPU.
    image.pixels=std::move(packed);
    images.emplace(atlasKey,std::move(image));
    ++metrics.atlasLoads;
    return true;
}

const giblet_art::Frame &frameFor(const Visual &v) {
    constexpr int pairs[4][3]={{2,3,8},{0,1,-1},{4,5,-1},{6,7,9}};
    constexpr int sizes[]={3,2,2,3};
    const unsigned kind=unsigned(v.kind);
    return giblet_art::frames[pairs[kind][v.variant%sizes[kind]]*2+int(v.resting)];
}
void drawVisual(void *context,const Visual &v) {
    const auto &camera=*static_cast<const Uniforms*>(context);
    const auto *sector=v.resting&&v.surface.sector>=0&&v.surface.sector<numsectors?
        &sectors[v.surface.sector]:sectorAt(v.center);
    if(!sector)return;
    if(v.resting&&(sector->floorpic==skyflatnum||liquid(v.surface.sector)))return;
    const auto &frame=frameFor(v);
    const float size=v.size*frame.scale/float(std::max(frame.w,frame.h));
    const float w=frame.w*size,h=frame.h*size,c=std::cos(v.rotation),s=std::sin(v.rotation);
    const Vec3 center=v.resting?Vec3{v.contact.x,v.contact.y,floorZ(sector)+.14f}:v.center;
    if(v.resting&&uncertainSweep(center,center,v.size))return;
    const float light=lighting(sector);
    const int band=tintBand(v.tint)*giblet_art::height;
    const auto vertex=[&](float u,float t) {
        const float a=(u-frame.pivotX)*w,b=(frame.pivotY-t)*h;
        const float x=a*c-b*s,y=a*s+b*c;
        const Vec3 p=v.resting?Vec3{center.x+x,center.y+y,center.z}:
            Vec3{center.x+x*camera.right[0]+y*camera.up[0],center.y+x*camera.right[1]+y*camera.up[1],
                 center.z+x*camera.right[2]+y*camera.up[2]};
        return Vertex{p.x,p.y,p.z,frame.x+u*frame.w,band+frame.y+t*frame.h,light,MODE_SPRITE};
    };
    // Both sprite poses use the same transparent, depth-tested texture batch.
    quad(batches[atlasKey],vertex(0,0),vertex(1,0),vertex(1,1),vertex(0,1));
    ++metrics.quads;
}
}

void clearGiblets() {
    giblets.clear();markBudgets={};lastTic=-1;
}
static void prepareGibletLevel(bool uploadTexture) {
    clearGiblets();lineSeen.clear();stamp=0;metrics={};
    images.erase(atlasKey);
    mapSerial=r_levelserial;debug=M_CheckParm((char*)"-goredebug")!=0;
    if(volumeLab()) {atlasStatus="solid_geometry";return;}
    if(!blockmap||!blockmaplump||numlines<=0) {atlasStatus="missing_blockmap";return;}
    lineSeen.resize(numlines);
    atlasStatus=loadAtlas(uploadTexture)?"textured":"atlas_unavailable";
}
void prepareGiblets() {prepareGibletLevel(true);}
int activeGiblets() {return int(giblets.activeCount());}
int emitGiblets(const Burst &request,int wallBudget,GoreBurstReport &report) {
    if(!enabled()||!images.count(atlasKey)) {
        report.path=volumeLab()?"solid_geometry":!images.count(atlasKey)?
            (std::strcmp(atlasStatus,"textured")==0?"atlas_unavailable":atlasStatus):"feature_off";
        return 0;
    }
    if(uncertainSweep(request.origin,request.origin,3)) {report.path="edited_geometry";return 0;}
    ++metrics.atlasHits;
    const auto budget=std::find_if(markBudgets.begin(),markBudgets.end(),[](const MarkBudget &entry){
        return !entry.id||!giblets.hasActiveEvent(entry.id);
    });
    if(budget==markBudgets.end()) {report.path="event_budget";return 0;}
    const int count=giblets.emit(request);
    if(count) {
        *budget={request.eventId,wallBudget};
        if(wallBudget>=0)metrics.maxEventWall=std::max(metrics.maxEventWall,3-wallBudget);
    }
    report.path=count?"textured":"piece_budget";
    return count;
}
void tickGiblets(size_t bloodDrops,int proceduralFragments) {
    if(!enabled()||!images.count(atlasKey)) {if(activeGiblets())clearGiblets();return;}
    if(paused||menuactive||lastTic==leveltime)return;
    lastTic=leveltime;
    const Host host={nullptr,hostTrace,gibletImpact};
    giblets.advance(1.f/TICRATE,host);
    if(!debug)return;
    if(leveltime%TICRATE||!giblets.stats().spawned)return;
    const auto &stats=giblets.stats();
    std::fprintf(stderr,"GORE G1 totals: heavy=%d drops=%zu spawned=%u traceCalls=%u candidates=%llu limited=%llu collisionUs=%llu impacts=%u rejected=%llu wall=%llu flat=%llu maxEventWall=%d atlasLoads=%u atlasHits=%u quads=%u batches=%u\n",
        activeGiblets()+proceduralFragments,bloodDrops,stats.spawned,stats.traces,
        (unsigned long long)metrics.candidates,(unsigned long long)metrics.queryStops,
        (unsigned long long)(metrics.collisionNS/1000),stats.emittedImpacts,
        (unsigned long long)(metrics.rejected+stats.suppressedImpacts),(unsigned long long)metrics.wall,
        (unsigned long long)metrics.flat,metrics.maxEventWall,metrics.atlasLoads,metrics.atlasHits,metrics.quads,metrics.batches);
}
void drawGiblets(const Uniforms &camera) {
    if(!enabled()||!activeGiblets()||!images.count(atlasKey))return;
    const auto before=metrics.quads;
    giblets.forEachVisual(drawVisual,const_cast<Uniforms*>(&camera));
    metrics.batches+=metrics.quads!=before;
}

#ifdef DOOM_TEST_DRIVER
bool testPrepareGiblets() {
    prepareGibletLevel(false);
    const auto found=images.find(atlasKey);
    if(found==images.end())return false;
    const auto &image=found->second;
    const auto &frame=giblet_art::frames[8]; // GIB4A0, a viscera pair.
    const auto *palette=(const byte*)W_CacheLumpName((char*)"PLAYPAL",PU_CACHE);
    int blue=0,red=0;
    for(int y=frame.y;y<frame.y+frame.h;++y)for(int x=frame.x;x<frame.x+frame.w;++x) {
        for(int band=0;band<2;++band) {
            const size_t at=(size_t(y+band*giblet_art::height)*image.width+x)*2;
            if(!image.pixels[at+1])continue;
            const auto *rgb=palette+image.pixels[at]*3;
            if(band)blue+=rgb[2]>rgb[0]&&rgb[2]>rgb[1];
            else red+=rgb[0]>rgb[2]&&rgb[0]>rgb[1];
        }
    }
    return blue>0&&red>0;
}
bool testGibletTrace(Vec3 a,Vec3 b,float radius,TraceHit &hit) {
    if(lineSeen.size()!=size_t(numlines))lineSeen.resize(numlines);
    return traceGiblet(a,b,radius,hit);
}
int testGibletQuads(const Uniforms &camera,bool blue,bool grounded) {
    auto &batch=batches[atlasKey];
    batch.clear();drawGiblets(camera);
    int count=0;
    const float low=blue?float(giblet_art::height):0,high=low+giblet_art::height;
    for(size_t i=0;i+5<batch.size();i+=6) {
        const auto &a=batch[i];
        if(a.v<low||a.v>=high)continue;
        const bool flat=std::all_of(batch.begin()+i,batch.begin()+i+6,[&](const Vertex &b){return a.z==b.z;});
        if(flat==grounded)++count;
    }
    return count;
}
unsigned testGibletTraceCount() {return giblets.stats().traces;}
#endif
}

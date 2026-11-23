#include "internal.h"
#include "../../../volume.h"

namespace doom3d {
void drawHellfireGlow(const Uniforms &camera,lab::Vec at,float width,float height,
                     float roll,const std::array<float,3> &color,float alpha,bool additive,bool floorAligned);

namespace {
constexpr int oilBatch=100000239,flameBatch=100000240;
constexpr int maskSize=32;
constexpr size_t maxFloorVertices=768;
using Color=std::array<float,3>;

void prepareMasks() {
    if(images.count(-1-oilBatch)&&images.count(-1-flameBatch))return;
    std::array<byte,maskSize*maskSize*2> oil{},flame{};
    for(int y=0;y<maskSize;++y)for(int x=0;x<maskSize;++x) {
        const float row=(y+.5f)/maskSize;
        const float across=2.f*(x+.5f)/maskSize-1.f;
        const float grain=hashUnit(x,y,0x570f14u);
        const float wander=.15f*std::sin(row*10.f)+.12f*std::sin(row*23.f+1.f);
        const float width=.10f+.71f*row;
        const float body=std::clamp((width-std::abs(across-wander))/.3f,0.f,1.f);
        const float vertical=std::min(row*5.f,(1.f-row)*9.f);
        const size_t i=2u*(y*maskSize+x)+1;
        oil[i]=byte(150.f+65.f*grain+28.f*std::sin(12.f*across+3.f*std::sin(row*17.f)));
        flame[i]=byte(255.f*body*std::clamp(vertical,0.f,1.f)*(.82f+.18f*grain));
    }
    if(!images.count(-1-oilBatch))images.emplace(-1-oilBatch,upload(maskSize,maskSize,oil.data()));
    if(!images.count(-1-flameBatch))images.emplace(-1-flameBatch,upload(maskSize,maskSize,flame.data()));
}

struct Surface {
    p_oil_pool_view_t view;
    std::vector<Point> triangles;
    bool used=true;
};
std::vector<Surface> surfaces;
unsigned oilLevel=~0u;

const Surface &oilSurface(const p_oil_pool_view_t &pool) {
    for(auto &surface:surfaces)
        if(surface.view.x==pool.x&&surface.view.y==pool.y&&surface.view.radius==pool.radius&&
           surface.view.sector==pool.sector) {
            surface.used=true;
            return surface;
        }
    auto &surface=surfaces.emplace_back(Surface{pool,{},true});
    const Point center={units(pool.x),units(pool.y)};
    const float radius=units(pool.radius);
    std::array<Point,OIL_SPOKES> rim;
    for(int i=0;i<OIL_SPOKES;++i) {
        const float a=2.f*doomPi*i/OIL_SPOKES,r=radius*pool.reach[i]/255.f;
        rim[i]={center.x+std::cos(a)*r,center.y+std::sin(a)*r};
    }
    for(int leaf:sectorFloors[pool.sector]) {
        const auto &bounds=floorBounds[leaf];
        if(bounds[2]<center.x-radius||bounds[0]>center.x+radius||
           bounds[3]<center.y-radius||bounds[1]>center.y+radius)continue;
        for(int n=0;n<OIL_SPOKES;++n) {
            const std::array<Point,3> corners={center,rim[n],rim[(n+1)%OIL_SPOKES]};
            if(pool.reach[n]==0||pool.reach[(n+1)%OIL_SPOKES]==0)continue;
            auto polygon=floors[leaf];
            for(int k=0;k<3&&!polygon.empty();++k) {
                const auto a=corners[k],b=corners[(k+1)%3];
                polygon=clip(polygon,a,{b.x-a.x,b.y-a.y},false);
            }
            for(size_t k=1;k+1<polygon.size();++k) {
                if(surface.triangles.size()>=maxFloorVertices)return surface;
                surface.triangles.insert(surface.triangles.end(),{polygon[0],polygon[k],polygon[k+1]});
            }
        }
    }
    return surface;
}

void oilFilm(const p_oil_pool_view_t &pool,const Uniforms &camera,float opacity,float flicker) {
    const auto &sector=sectors[pool.sector];
    const float x=units(pool.x),y=units(pool.y),r=units(pool.radius);
    const float z=floorZ(&sector)+decalLift(x,y,camera)+.025f;
    const float light=.60f+.40f*lighting(&sector);
    auto &vertices=decalBatches[oilBatch];
    for(Point p:oilSurface(pool).triangles) {
        const float dx=(p.x-x)/r,dy=(p.y-y)/r;
        const float line=.5f+.5f*std::sin(17.f*dx+23.f*dy);
        const float glint=std::pow(line,5.f)*flicker;
        vertices.push_back({p.x,p.y,z,(dx*.5f+.5f)*maskSize,(dy*.5f+.5f)*maskSize,
                            opacity,0u,(.055f+.115f*glint)*light,
                            (.032f+.060f*glint)*light,(.018f+.026f*glint)*light});
    }
}

void tongue(const Uniforms &camera,float x,float y,float z,float width,float height,float alpha,float lean,
            const Color &tint,bool additive=false) {
    auto v=[&](float u,float t) {
        const float r=(2.f*u-1.f)*width+(1.f-t)*lean;
        return Vertex{x+camera.right[0]*r,y+camera.right[1]*r,z+(1.f-t)*height,
                      u*maskSize,t*maskSize,alpha,0u,tint[0],tint[1],tint[2]};
    };
    auto &batch=additive?effectAddBatches[flameBatch]:effectBatches[flameBatch];
    quad(batch,v(0,0),v(1,0),v(1,1),v(0,1));
}

void burst(const p_oil_pool_view_t &pool,const Uniforms &camera,float x,float y,float z,
           float age,uint32_t seed,bool small) {
    const float scale=small?.32f:1.f;
    const float life=small?.19f:.48f;
    if(age>=life)return;
    const float phase=age/life;
    const float fade=1.f-phase;
    if(!small) {
        // The animated BEXP sprite is the fireball. This faint glow ties it
        // to the igniting oil without covering the changing sprite frames.
        drawHellfireGlow(camera,{x,y,z+6.f},10.f+phase*12.f,5.f+phase*5.f,0.f,
                         {1.f,.45f,.058f},fade*.42f,true,false);
        drawHellfireGlow(camera,{x,y,z+15.f+phase*17.f},7.f+phase*11.f,5.f+phase*11.f,.2f,
                         {.16f,.105f,.078f},phase*fade*.66f,false,false);
    }
    const int sparks=small?2:9;
    for(int i=0;i<sparks;++i) {
        const float angle=2.f*doomPi*hashUnit(i,1,seed);
        const float vx=std::cos(angle),vy=std::sin(angle);
        const float distance=(14.f+26.f*hashUnit(i,2,seed))*phase*scale;
        const float height=scale*(10.f+54.f*phase-59.f*phase*phase);
        const float px=x+vx*distance,py=y+vy*distance;
        const auto *sector=R_PointInSubsector(fixed_t(px*FRACUNIT),fixed_t(py*FRACUNIT))->sector;
        if(sector!=&sectors[pool.sector]||z+height>=ceilZ(sector))continue;
        WallHit wall;
        if(distance>.01f&&firstSolidWall(x,y,z+6.f*scale,vx,vy,(height-6.f*scale)/distance,distance,wall))continue;
        const bool drop=i>=6;
        drawHellfireGlow(camera,{px,py,z+height},drop?1.4f:.60f,drop?1.5f:.95f,angle,
                         drop?Color{.11f,.048f,.022f}:Color{1.f,.61f,.10f},fade*.88f,!drop,false);
    }
}

void leakSpray(const p_oil_pool_view_t &pool,const Uniforms &camera,float x,float y,float z,
               float age,uint32_t seed) {
    constexpr float sprayLife=.46f,dripLife=1.5f;
    if(age>=dripLife)return;
    const auto *sector=&sectors[pool.sector];
    const float yaw=pool.leakAngle*(2.f*doomPi/4294967296.f);
    // Four separate short arcs from the struck side of the can, then a
    // pair of heavier drops down its face into the floor spill.
    for(int n=0;n<6;++n) {
        const bool spray=n<4;
        const float angle=yaw+(hashUnit(n,31,seed)-.5f)*(spray?1.25f:.36f);
        const float phase=spray?(age-n*.017f)/sprayLife:std::fmod(age*2.3f+(n-4)*.53f,1.f);
        if(phase<0.f||phase>=1.f)continue;
        const float reach=spray?.50f+.38f*hashUnit(n,32,seed):.13f;
        const float d=13.f+phase*reach*units(pool.radius);
        const float dx=std::cos(angle),dy=std::sin(angle);
        const float px=x+dx*d,py=y+dy*d;
        const float h=11.f*(1.f-phase*phase)+(spray?phase*(1.f-phase)*16.f:0.f);
        if(z+h+.9f>=ceilZ(sector)||
           R_PointInSubsector(fixed_t(px*FRACUNIT),fixed_t(py*FRACUNIT))->sector!=sector)continue;
        WallHit wall;
        if(firstSolidWall(x,y,z+11.f,dx,dy,(h-11.f)/d,d,wall))continue;
        const float fade=spray?1.f-phase:1.f-age/dripLife;
        drawHellfireGlow(camera,{px,py,z+h},spray?1.65f:1.05f,spray?1.3f:1.8f,
                         angle,{.09f,.042f,.019f},.95f*fade,false,false);
        drawHellfireGlow(camera,{px,py,z+h+.28f},.64f,.45f,0.f,
                         {.52f,.25f,.09f},.55f*fade,true,false);
    }
}
}

void buildIncendiary(const Uniforms &camera) {
    if(oilLevel!=r_levelserial) {
        surfaces.clear();
        oilLevel=r_levelserial;
    }
    const int count=P_IncendiaryPoolCount();
    if(!count||floors.empty()) {
        surfaces.clear();
        return;
    }
    for(auto &surface:surfaces)surface.used=false;
    prepareMasks();
    for(int i=0;i<count;++i) {
        p_oil_pool_view_t pool{};
        if(!P_IncendiaryPoolGet(i,&pool)||pool.sector<0||pool.sector>=(int)sectorFloors.size())continue;
        const auto &sector=sectors[pool.sector];
        const float x=units(pool.x),y=units(pool.y),z=floorZ(&sector);
        const float clearance=ceilZ(&sector)-z;
        if(sector.floorpic==skyflatnum||clearance<4.f||std::hypot(x-camera.eye[0],y-camera.eye[1])>1800.f)continue;
        const uint32_t seed=hash32(uint32_t(pool.x)^(uint32_t(pool.y)<<1));
        const float age=float(pool.age)/TICRATE;
        const float fade=std::clamp(pool.remaining/70.f,0.f,1.f);
        const bool leak=pool.kind==OIL_LEAK;
        const float flicker=.70f+.16f*std::sin(age*16.f+hashUnit(i,3,seed)*5.f);
        oilFilm(pool,camera,.91f*fade,leak?.39f:flicker*fade);
        if(leak||pool.kind==OIL_RUPTURE)burst(pool,camera,x,y,z,age,seed,leak);
        if(leak) {
            leakSpray(pool,camera,x,y,z,age,seed);
            continue;
        }
        const int flames=pool.kind==OIL_RUPTURE?6:3;
        for(int n=0;n<flames;++n) {
            const int spoke=(n*OIL_SPOKES/flames+int(seed%7u))%OIL_SPOKES;
            const float a=2.f*doomPi*spoke/OIL_SPOKES;
            const float r=units(pool.radius)*pool.reach[spoke]/255.f*(.24f+.42f*hashUnit(n,4,seed));
            const float px=x+std::cos(a)*r,py=y+std::sin(a)*r;
            if(R_PointInSubsector(fixed_t(px*FRACUNIT),fixed_t(py*FRACUNIT))->sector!=&sector)continue;
            const float flick=.63f+.37f*std::sin(age*(11.f+n)+hashUnit(n,5,seed)*6.f);
            const float h=std::min(2.f+fade*(5.f+9.f*flick),clearance-1.f);
            const float lean=1.3f*flick-.7f;
            tongue(camera,px,py,z+.4f,2.7f+2.3f*fade,h,fade*(.44f+.25f*flick),lean,{.87f,.205f,.024f});
            tongue(camera,px,py,z+.4f,1.3f+1.5f*fade,h*.72f,fade*(.51f+.39f*flick),lean*.64f,
                   {1.f,.70f,.14f},true);
            if(n%3==0) {
                const float rise=std::fmod(age*.77f+hashUnit(n,6,seed),1.f);
                if(5.f+rise*17.f<clearance)
                    drawHellfireGlow(camera,{px,py,z+5.f+rise*17.f},2.f+rise*3.f,3.f+rise*4.f,rise*.5f,
                                     {.15f,.102f,.082f},fade*(1.f-rise)*.24f,false,false);
            }
        }
    }
    surfaces.erase(std::remove_if(surfaces.begin(),surfaces.end(),[](const Surface &s){return !s.used;}),surfaces.end());
}
} // namespace doom3d

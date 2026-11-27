/* Light from glowing wall and floor textures, grouped into tiles that
   see each other. */
#include "internal.h"
#include "../material_light_coverage.h"
#include <tuple>
extern "C" {
}

namespace doom3d {
std::vector<SurfaceLight> surfaceLights;
static std::vector<SurfaceLight> switchLights;
static std::unordered_map<SurfaceLightKey,int,SurfaceLightKeyHash> surfaceGroups; // Lookup only; tile grouping order is explicit.
float surfaceEye[3];
SurfaceLightSelection surfaceSelection;
uint64_t surfaceLightTime;
struct FlatLightSample { Point point; int u,v,component=0;float fraction=1; };
static std::map<std::tuple<int,int,bool>,std::vector<FlatLightSample>> flatLightSamples;
static constexpr size_t flatCoverageLimit=4*1024*1024;
static size_t flatCoverageBudget=flatCoverageLimit;
static bool clippedWallLimitReported=false;
static std::vector<std::array<int,3>> wallMaterials;
static std::vector<std::array<int,2>> flatMaterials;
#ifdef DOOM_TEST_DRIVER
struct MaterialAudit {
    size_t surfaces=0,logical=0,off=0,clipped=0,destroyed=0;
};
static std::map<const Image *,MaterialAudit> auditedMaterials;
static bool auditMaterials=false;
static unsigned auditedLevel=~0u;
struct MaterialAuditFrame {
    ~MaterialAuditFrame() {
        if(!auditMaterials)return;
        size_t areaPieces=0,live=0,fog=0;
        for(const auto &area:bakeAreas)areaPieces+=area.pieces.size();
        for(unsigned i=0;i<flashes.count;++i) {
            fog+=fogOnly[i];
            live+=!fogOnly[i]&&lightKinds[i]==I_RENDER3D_LIGHT_SURFACE;
        }
        std::map<std::pair<bool,std::string>,std::pair<const Image *,MaterialAudit>> ordered;
        for(const auto &[key,image]:images) {
            const auto found=auditedMaterials.find(&image);
            if(found==auditedMaterials.end())continue;
            char name[9]={};
            if(key>=0)R_TextureName(key,name);
            else memcpy(name,lumpinfo[-1-key].name,8);
            ordered[{key<0,name}]={&image,found->second};
        }
        for(const auto &[name,entry]:ordered) {
            const auto &[image,counts]=entry;
            int rule=Game_GlowKind(name.second.c_str(),name.first);
            if(rule==DOOM_EMISSIVE_NONE&&!image->switchIndicator&&!image->emissionWeight&&!image->explicitEmissionHash)continue;
            fprintf(stderr,"Material audit %s %s kind %u coverage %.6f surfaces %zu logical %zu off %zu clipped %zu destroyed %zu\n",
                name.first?"flat":"wall",name.second.c_str(),unsigned(image->lightMaterial),image->emissionCoverage,
                counts.surfaces,counts.logical,counts.off,counts.clipped,counts.destroyed);
        }
        fprintf(stderr,"Material audit totals preset %d bake_only %d logical %zu switch %zu static %zu areas %zu pieces %zu slots %zu live %zu fog %zu\n",
            effectiveLightingModel(settings),bakeOnlyActive,surfaceLights.size(),switchLights.size(),bakeSources.size(),
            bakeAreas.size(),areaPieces,surfaceSelection.slots.size(),live,fog);
        auditMaterials=false;auditedLevel=r_levelserial;
    }
};
#endif
// Pixel integration depends only on the translated material and its clipped
// texture coordinates. Retain one entry per wall side/span, bounded by map size.
struct WallEmissionIntegral {
    int texture=-1;uint64_t mask=0;std::array<float,4> bounds{};
    double weight=0,centerU=0,centerV=0;
};
static std::vector<std::array<WallEmissionIntegral,8>> wallEmissionIntegrals;
struct WallPointSample {int u,v;float atU,atV,extent,coverage;};
struct WallPoints {
    int texture=-1;uint64_t mask=0;std::array<float,4> bounds{};
    std::vector<WallPointSample> points;
};
static std::unordered_map<size_t,WallPoints> wallPointSamples;
static std::vector<SurfaceLight> mergeInputs,mergeResult;
static std::vector<size_t> mergeMembership;
static std::vector<float> mergeWeights;
static int mergeLightingModel=doom_lighting_model::Enhanced;
static bool sameSurface(const SurfaceLight &a,const SurfaceLight &b,bool shine=true) {
    return a.key==b.key&&a.x==b.x&&a.y==b.y&&a.z==b.z&&a.radius==b.radius&&a.strength==b.strength&&
        a.color==b.color&&a.facing==b.facing&&a.extent==b.extent&&a.group==b.group&&a.flickerSector==b.flickerSector&&
        a.radiusLimit==b.radiusLimit&&(!shine||a.shine==b.shine);
}


// A wall light's normal from its merge group (facing 2+ the normal's
// octant), for doom_panel_emission; flat lights have none.
std::array<float,2> panelNormal(const SurfaceLight &light) {
    if(light.facing<2)return {0,0};
    float angle=(light.facing-2)*(float)(M_PI/4);
    return {std::cos(angle),std::sin(angle)};
}
// Visibility groups of emissive tiles (SurfaceLight::group), per level: in
// each merge cell, a tile joins the first group whose first tile it sees
// along a bake ray, else starts its own. Cells are regrouped whenever a tile
// first appears (a moving door reshapes its wall pieces); the bake map's
// heights at that moment decide.
static bool tileSees(const SurfaceLight &a,const SurfaceLight &b) {
    float d[3]={b.x-a.x,b.y-a.y,b.z-a.z},length=std::sqrt(d[0]*d[0]+d[1]*d[1]+d[2]*d[2]);
    if(length<1)return true;
    for(float &c:d)c/=length;
    int sector=(int)(R_PointInSubsector((fixed_t)(a.x*FRACUNIT),(fixed_t)(a.y*FRACUNIT))->sector-sectors);
    return traceRay(bakeMap,sector,a.x,a.y,a.z,d,length-0.5f,SkyCeiling::open);
}
static void groupSurfaceLights(std::vector<SurfaceLight> &tiles) {
    if(bakeMap.sectors.size()!=(size_t)numsectors)return;
    std::set<SurfaceLightKey> stale;
    for(auto &tile:tiles) {
        auto known=surfaceGroups.find(tile.key);
        if(known!=surfaceGroups.end())tile.group=known->second;
        else {tile.group=0;stale.insert(surfaceCell(tile));}
    }
    if(stale.empty())return;
    std::map<SurfaceLightKey,std::vector<SurfaceLight*>> cells;
    for(auto &tile:tiles) {
        SurfaceLight plain=tile;plain.group=0;
        SurfaceLightKey cell=surfaceCell(plain);
        if(stale.count(cell))cells[cell].push_back(&tile);
    }
    for(auto &entry:cells) {
        auto &members=entry.second;
        std::sort(members.begin(),members.end(),[](const SurfaceLight *a,const SurfaceLight *b){return a->key<b->key;});
        std::vector<const SurfaceLight*> seeds;
        std::vector<std::array<float,4>> sums; // Each group's weighted position sum and weight.
        auto centerOf=[](const SurfaceLight &like,const std::array<float,4> &sum) {
            SurfaceLight center=like;center.x=sum[0]/sum[3];center.y=sum[1]/sum[3];center.z=sum[2]/sum[3];
            return center;
        };
        // A tile joins the first group whose first tile it sees; strict, it
        // also needs the group's center with it added in view of both.
        auto seed=[&](const std::vector<SurfaceLight*> &tiles,size_t first,bool strict) {
            for(SurfaceLight *tile:tiles) {
                float w=std::max(tile->strength,0.001f);
                std::array<float,4> own={tile->x*w,tile->y*w,tile->z*w,w};
                int group=-1;
                for(size_t n=first;n<seeds.size()&&group<0;++n) {
                    if(!tileSees(*tile,*seeds[n]))continue;
                    if(strict) {
                        std::array<float,4> with=sums[n];for(int c=0;c<4;++c)with[c]+=own[c];
                        SurfaceLight center=centerOf(*tile,with);
                        if(!tileSees(*tile,center)||!tileSees(*seeds[n],center))continue;
                    }
                    group=(int)n;
                }
                if(group<0) {group=(int)seeds.size();seeds.push_back(tile);sums.push_back({});}
                for(int c=0;c<4;++c)sums[group][c]+=own[c];
                tile->group=group;
            }
        };
        seed(members,0,false);
        // The merged light stands at its group's weighted center (as in
        // mergeSurfaceLights); tiles that cannot see it, around a corner of
        // an odd-shaped room or pool, regroup strictly among themselves, and
        // any still cut off stand alone.
        for(int pass=0;pass<6;++pass) {
            std::vector<SurfaceLight*> cut;
            for(SurfaceLight *tile:members)
                if(!tileSees(*tile,centerOf(*tile,sums[tile->group])))cut.push_back(tile);
            if(cut.empty())break;
            for(SurfaceLight *tile:cut) {
                float w=std::max(tile->strength,0.001f);
                auto &sum=sums[tile->group];sum[0]-=tile->x*w;sum[1]-=tile->y*w;sum[2]-=tile->z*w;sum[3]-=w;
            }
            if(pass<3)seed(cut,seeds.size(),true);
            else for(SurfaceLight *tile:cut) {
                float w=std::max(tile->strength,0.001f);
                tile->group=(int)seeds.size();seeds.push_back(tile);sums.push_back({tile->x*w,tile->y*w,tile->z*w,w});
            }
        }
        for(const SurfaceLight *tile:members)surfaceGroups[tile->key]=tile->group;
    }
}
SurfaceLightKey surfaceCellOf(const SurfaceLight &tile) {
    SurfaceLight grouped=tile;
    auto known=surfaceGroups.find(tile.key);
    grouped.group=known!=surfaceGroups.end()?known->second:0;
    return surfaceCell(grouped);
}
// Merged emissive lights, kept apart where walls divide their tiles.
std::vector<SurfaceLight> mergedSurfaceLights(std::vector<SurfaceLight> tiles) {
    const int model=effectiveLightingModel(settings);
    if(mergeLightingModel!=model) {
        // Visibility groups depend on the strength-weighted center.
        mergeLightingModel=model;
        surfaceGroups.clear();mergeResult.clear();surfaceSelection.clear();
    }
    bool hit=tiles.size()==mergeInputs.size()&&!mergeResult.empty();
    if(hit)for(size_t i=0;i<tiles.size();++i)if(!sameSurface(tiles[i],mergeInputs[i],false)){hit=false;break;}
    if(!hit) {
        mergeInputs=tiles;
        groupSurfaceLights(tiles);mergeResult=mergeSurfaceLights<true>(tiles);
        mergeMembership.resize(tiles.size());mergeWeights.assign(mergeResult.size(),0);
        std::unordered_map<SurfaceLightKey,size_t,SurfaceLightKeyHash> indices;indices.reserve(mergeResult.size());
        for(size_t i=0;i<mergeResult.size();++i)indices.emplace(mergeResult[i].key,i);
        for(size_t i=0;i<tiles.size();++i) {
            auto key=surfaceCell(tiles[i]);
            size_t group=indices.at(key);mergeMembership[i]=group;
            mergeWeights[group]+=std::max(tiles[i].strength,0.001f);
        }
    } else {
        // Flicker is live. Sum in the original tile order with the same float
        // arithmetic; only unchanged positions/grouping/extent are reused.
        for(auto &light:mergeResult)light.shine=0;
        for(size_t i=0;i<tiles.size();++i)mergeResult[mergeMembership[i]].shine+=tiles[i].shine*std::max(tiles[i].strength,0.001f);
        for(size_t i=0;i<mergeResult.size();++i)mergeResult[i].shine/=mergeWeights[i];
    }
    if(M_CheckParm((char*)"-verifysurfacecache")) {
        groupSurfaceLights(tiles);auto fresh=mergeSurfaceLights(tiles);
        if(fresh.size()!=mergeResult.size())I_Error((char*)"Cached surface light count differs");
        for(size_t i=0;i<fresh.size();++i)if(!sameSurface(fresh[i],mergeResult[i]))I_Error((char*)"Cached surface light merge differs");
    }
    return mergeResult;
}
// How strongly a sector's glowing textures light, from its light level:
// the mapper's stand-in for the strength of its lamps. The old 35% minimum
// made a lamp in a very dim sector almost half strength even at level 144.
// Keep a small 8% local contribution so emissive fixtures are visible in
// dark rooms without letting them flood neighboring metal. Approximate
// strengths: 96: 8%, 144: 25%, 160: 38%, 192: 76%, 208+: 100%.
// Flickering sectors
// count at their peak (their light thinker's maxlight), read once per level
// with the low they drop to, so a save loaded mid-flicker lights like a
// fresh start.
struct LampRange { int low,peak; bool flickers,smooth,lamps; };
static std::vector<LampRange> lampRanges;
// A glowing texture other than a screen: a lamp the sector's swing is for.
static bool isLamp(const Image &image) {return image.emissionWeight>0&&image.screens.empty();}
// Whether a flickering sector shows a lamp texture: on its floor or
// ceiling, or on the wall sections of its sides as the map starts.
static void findLamps() {
    for(int i=0;i<numsectors;++i)for(int ceiling=0;ceiling<2;++ceiling) {
        int flat=ceiling?sectors[i].ceilingpic:sectors[i].floorpic;
        if(lampRanges[i].flickers&&flat!=skyflatnum&&isLamp(lumpImage(firstflat+flattranslation[flat],true)))lampRanges[i].lamps=true;
    }
    for(int i=0;i<numlines;++i)for(int side=0;side<2;++side) {
        if(lines[i].sidenum[side]<0)continue;
        const side_t &s=sides[lines[i].sidenum[side]];
        int index=int(s.sector-sectors);
        if(!lampRanges[index].flickers||lampRanges[index].lamps)continue;
        const sector_t *back=lines[i].sidenum[side^1]>=0?sides[lines[i].sidenum[side^1]].sector:nullptr;
        int shown[3]={s.midtexture,
                      back&&back->ceilingheight<s.sector->ceilingheight?s.toptexture:0,
                      back&&back->floorheight>s.sector->floorheight?s.bottomtexture:0};
        for(int tex:shown)if(tex>0&&isLamp(wallImage(texturetranslation[tex])))lampRanges[index].lamps=true;
    }
}
static void readLampRanges() {
    if(lampRanges.size()==(size_t)numsectors)return;
    lampRanges.resize(numsectors);
    for(int i=0;i<numsectors;++i)lampRanges[i]={sectors[i].lightlevel,sectors[i].lightlevel,false,false,false};
    Game_LightSwings([](int sector,int low,int high,int smooth,void*) {
        if(sector<0||sector>=numsectors)return;
        LampRange &range=lampRanges[sector];
        range={low,std::max(range.peak,high),true,smooth!=0,false};
    },nullptr);
    findLamps();
}
static std::array<float,3> emissionDensity={};
// Count emitting texels over actual map surface area, rather than texture
// dimensions: a small repeating texture can cover a whole cavern.
void measureEmissionDensity() {
    double total=0,sum[3]={};
    auto add=[&](const Image &image,double area) {
        if(area<=0)return;
        total+=area;
        for(int c=0;c<3;++c)sum[c]+=area*image.emissionCoverage*image.emissionColor[c];
    };
    for(size_t i=0;i<floors.size();++i) {
        const auto &poly=floors[i];double area=0;
        for(size_t n=0;n<poly.size();++n) {const auto &a=poly[n],&b=poly[(n+1)%poly.size()];area+=(double)a.x*b.y-(double)b.x*a.y;}
        const auto &s=*subsectors[i].sector;
        for(int pic:{s.floorpic,s.ceilingpic})if(pic!=skyflatnum)add(lumpImage(firstflat+flattranslation[pic],true),std::abs(area)*0.5);
    }
    for(int i=0;i<numlines;++i)for(int side=0;side<2;++side) {
        const auto &line=lines[i];if(line.sidenum[side]<0)continue;
        const auto &s=sides[line.sidenum[side]];const auto *other=side?line.frontsector:line.backsector;
        float floor=units(s.sector->floorheight),ceiling=units(s.sector->ceilingheight);
        double width=std::hypot(units(line.v2->x)-units(line.v1->x),units(line.v2->y)-units(line.v1->y));
        auto wall=[&](int pic,float height){if(pic>0&&height>0)add(wallImage(texturetranslation[pic]),width*height);};
        if(!other)wall(s.midtexture,ceiling-floor);
        else {
            wall(s.bottomtexture,std::min(ceiling,units(other->floorheight))-floor);
            if(s.sector->ceilingpic!=skyflatnum||other->ceilingpic!=skyflatnum)wall(s.toptexture,ceiling-std::max(floor,units(other->ceilingheight)));
            if(s.midtexture>0)wall(s.midtexture,std::min({ceiling,units(other->ceilingheight),floor+(float)wallImage(texturetranslation[s.midtexture]).height})-std::max(floor,units(other->floorheight)));
        }
    }
    for(int c=0;c<3;++c)emissionDensity[c]=total>0?(float)(sum[c]/total):0;
    fprintf(stderr,"3D emissive coverage: RGB %.3f %.3f %.3f of map surface area.\n",emissionDensity[0],emissionDensity[1],emissionDensity[2]);
}
float materialEmissionGain(const Image &image) {
    // Use the fixture's hue: red-dominated maps do not dim blue key lamps.
    return emissionCoverageGain(emissionDensity.data(),image.emissionColor.data(),image.liquidEmitter);
}
static float lampLevel(int sector) {
    readLampRanges();
    if(sector<0||sector>=numsectors)return 1;
    // The material's emissive texels are lit separately in world.frag;
    // this factor controls the light cast into the environment, not the texels.
    return doom_lighting_model::fixtureOutput(lampRanges[sector].peak,effectiveLightingModel(settings));
}
// Lamps in a flickering sector follow it. Blinking and strobing sectors
// switch them, as a shot-out lamp sputters: out while the sector stands in
// the lower half of its swing, their glowing texels dead (addBrokenWall,
// addBrokenFlat). MAP02's strips strobing between 128 and 144 stay out and
// flash on for 5 tics in 40. Glowing and fire-flickering sectors fade them
// with their swing instead, glow and all (lampGlow): E1M2's armor alcove
// (sector 28) pulses. Their baked light follows by flicker group
// (assignLampGroups), and the sector itself keeps its low (lampLightLevel).
// The swing is the lamps' and not the room's power: screens beside them
// stay on. A sector whose only glowing textures are screens was drawn for
// them, so there the screens are what flickers.
bool lampFlickers(int sector) {
    readLampRanges();
    return sector>=0&&sector<numsectors&&lampRanges[sector].flickers;
}
bool lampFlickers(int sector,const Image &image) {
    return lampFlickers(sector)&&(image.screens.empty()||!lampRanges[sector].lamps);
}
float lampShine(int sector) {
    if(!lampFlickers(sector))return 1;
    const LampRange &range=lampRanges[sector];
    int now=sectors[sector].lightlevel;
    if(range.smooth)return std::clamp((now-range.low)/(float)(range.peak-range.low),0.0f,1.0f);
    return now*2<range.low+range.peak?0.0f:1.0f;
}
bool lampOut(int sector,const Image &image) {return lampFlickers(sector,image)&&!lampRanges[sector].smooth&&lampShine(sector)<=0;}
int lampLightLevel(const sector_t *sector) {
    int index=int(sector-sectors);
    if(!lampGroupSector(index))return sector->lightlevel;
    return std::min<int>(sector->lightlevel,lampRanges[index].low);
}
void lampGlow(std::vector<Vertex> &out,size_t first,const sector_t *sector,const Image &image) {
    int index=int(sector-sectors);
    float shine=materialEmissionGain(image);
    if(lampFlickers(index,image)&&lampRanges[index].smooth)shine*=lampShine(index);
    if(shine>=1)return;
    unsigned alpha=(unsigned)std::lround(shine*255);
    for(size_t n=first;n<out.size();++n)out[n].lampTint=(out[n].lampTint&0xFFFFFFu)|alpha<<24;
}
static void surfaceLight(const SurfaceLightKey &key,int sector,float x,float y,float z,const Image &image,int facing,float radius=224,float extent=0,
                         float crowd=1,float coverage=1) {
    const bool sourceDriven=sourceDrivenLighting(settings);
    if(!settings.emissive||image.emissionWeight==0)return;
    if(sourceDriven?fixtureShotOut(key):tileDark(key)) {
#ifdef DOOM_TEST_DRIVER
        if(auditMaterials)++auditedMaterials[&image].destroyed;
#endif
        return;
    }
    // Lamps too dim to light stay listed (strength 0), and so do lamps
    // switched off: shots still find them.
    SurfaceLight light={key,x,y,z,radius,1.65f*image.emissionWeight*lampLevel(sector)*crowd*coverage,image.emissionColor,facing,extent};
    if(image.switchIndicator) {
        // Switches change state dynamically and must not enter the static
        // bake. Unlike the old fixed 1.2 light, their contribution follows
        // the actual lit area, sector brightness, and emitter crowding.
        // Keep a compact halo without painting nearby walls a solid color.
        light.radius=88;
        light.strength=std::min(.30f,1.15f*image.emissionWeight*
            (.25f+.75f*lampLevel(sector))*crowd*coverage);
        light.extent=0;
        switchLights.push_back(light);
#ifdef DOOM_TEST_DRIVER
        if(auditMaterials)++auditedMaterials[&image].logical;
#endif
        return;
    }
    if(sourceDriven) {
        light.radius=doom_material_light::sourceRadius(radius,image.lightMaterial,true);
        if(light.radius<radius)light.radiusLimit=light.radius;
        if(!doom_material_light::canEmit(light.strength,light.radius))return;
    }
    if(lampFlickers(sector,image)) {light.flickerSector=sector;light.shine=lampShine(sector);}
    surfaceLights.push_back(light);
#ifdef DOOM_TEST_DRIVER
    if(auditMaterials) {
        auto &counts=auditedMaterials[&image];
        ++counts.logical;counts.off+=light.shine<=0;counts.clipped+=coverage<1;
    }
#endif
}
static void materialReplaced() {
    lightBakeKey=-1;
    surfaceGroups.clear();mergeResult.clear();surfaceSelection.clear();
}
// Raw material IDs change on a permanent replacement. An animation advances
// only the translation, and its live copy is reconciled every frame.
void trackWallLightMaterial(int side,const side_t &surface) {
    if(!sourceDrivenLighting(settings))return;
    if(wallMaterials.size()!=(size_t)numsides)wallMaterials.assign(numsides,{-1,-1,-1});
    auto &previous=wallMaterials[side];
    const std::array<int,3> current={surface.midtexture,surface.toptexture,surface.bottomtexture};
    if(previous==current)return;
    auto isStaticEmitter=[](int tex) {
        if(tex<=0)return false;
        auto emitting=[](const Image &image) {return image.emissionWeight>0&&!image.switchIndicator;};
        return emitting(wallImage(tex))||emitting(wallImage(texturetranslation[tex]));
    };
    if(previous[0]>=0)
        for(size_t i=0;i<current.size();++i)if(previous[i]!=current[i]&&(isStaticEmitter(previous[i])||isStaticEmitter(current[i]))) {
            materialReplaced();
            break;
        }
    previous=current;
}

static void addClippedFlatSamples(int sector,const Image &image,const std::map<std::pair<int,int>,Point> &whole,
                                  std::vector<FlatLightSample> &result) {
    using namespace doom_material_light;
    struct Piece {int leaf;std::vector<CoveragePoint> footprint;CoveredEmission emission;};
    std::map<std::pair<int,int>,std::vector<Piece>> repeats;
    std::vector<CoveragePoint> scratch;
    const auto limitMessage=[] {fprintf(stderr,"3D material lighting: clipped flat coverage budget reached.\n");};
    for(int leaf:sectorFloors[sector]) {
        const auto &polygon=floors[leaf];
        if(polygon.size()<3)continue;
        const auto &bounds=floorBounds[leaf];
        const int u0=(int)std::floor(bounds[0]/image.width),u1=(int)std::ceil(bounds[2]/image.width);
        const int v0=(int)std::floor(-bounds[3]/image.height),v1=(int)std::ceil(-bounds[1]/image.height);
        for(int u=u0;u<u1;++u)for(int v=v0;v<v1;++v) {
            if(whole.count({u,v}))continue;
            std::vector<CoveragePoint> footprint;
            footprint.reserve(polygon.size());
            for(auto p:polygon)footprint.push_back({p.x-u*float(image.width),-p.y-v*float(image.height)});
            clipBox(footprint,scratch,0,0,float(image.width),float(image.height));
            const auto emission=integrateEmission(footprint,image.emission.data(),image.width,image.height,flatCoverageBudget);
            if(!flatCoverageBudget) {limitMessage();return;}
            if(emission.weight>0)repeats[{u,v}].push_back({leaf,std::move(footprint),emission});
        }
    }
    const double nominal=image.emissionCoverage*image.width*image.height*255.0;
    for(auto &[tile,pieces]:repeats) {
        std::sort(pieces.begin(),pieces.end(),[](const Piece &a,const Piece &b){return a.leaf<b.leaf;});
        std::vector<size_t> parents(pieces.size());
        for(size_t i=0;i<pieces.size();++i)parents[i]=i;
        auto root=[&](size_t i) {while(parents[i]!=i)i=parents[i];return i;};
        const uint64_t comparisons=uint64_t(pieces.size())*(pieces.size()-1)/2;
        if(comparisons>=flatCoverageBudget) {flatCoverageBudget=0;limitMessage();return;}
        flatCoverageBudget-=size_t(comparisons);
        for(size_t i=0;i<pieces.size();++i)for(size_t j=0;j<i;++j)if(sharesEdge(pieces[i].footprint,pieces[j].footprint)) {
            const auto a=root(i),b=root(j);
            parents[std::max(a,b)]=std::min(a,b);
        }
        std::map<size_t,std::vector<const Piece *>> components;
        for(size_t i=0;i<pieces.size();++i)components[root(i)].push_back(&pieces[i]);
        const auto [u,v]=tile;
        for(const auto &[owner,members]:components) {
            CoveredEmission sum;
            const Piece *largest=members.front();
            for(const auto *piece:members) {
                sum.add(piece->emission);
                if(piece->emission.weight>largest->emission.weight)largest=piece;
            }
            auto center=sum.center();
            if(std::none_of(members.begin(),members.end(),[&](const Piece *piece) {
                return insideConvex(piece->footprint,center.x,center.y);
            }))center=largest->emission.center();
            result.push_back({{center.x+u*image.width,-center.y-v*image.height},u,v,
                components.size()==1?0:pieces[owner].leaf+2,float(std::min(1.0,sum.weight/nominal))});
        }
    }
}

static const std::vector<FlatLightSample> &flatSamples(int sectorIndex,int lump,const Image &image) {
    const bool clipped=sourceDrivenLighting(settings)&&image.lightMaterial!=doom_material_light::Kind::Liquid&&
        image.emissionCoverage<.4f;
    auto key=std::make_tuple(sectorIndex,lump,clipped);
    auto cached=flatLightSamples.find(key);
    if(cached!=flatLightSamples.end()) return cached->second;
    std::map<std::pair<int,int>,Point> samples;
    // Sample the texture's emission center on its world-aligned repeat grid.
    // BSP polygons only test membership; their centers never place lights.
    for(int leaf:sectorFloors[sectorIndex]) {
        const auto &poly=floors[leaf];if(poly.size()<3) continue;
        const auto &bounds=floorBounds[leaf];
        int firstU=(int)std::ceil((bounds[0]-image.emissionU)/image.width);
        int lastU=(int)std::floor((bounds[2]-image.emissionU)/image.width);
        int firstV=(int)std::ceil((-bounds[3]-image.emissionV)/image.height);
        int lastV=(int)std::floor((-bounds[1]-image.emissionV)/image.height);
        for(int u=firstU;u<=lastU;++u) for(int v=firstV;v<=lastV;++v) {
            Point point={image.emissionU+u*image.width,-image.emissionV-v*image.height};
            if(doom_material_light::insideConvex(poly,point.x,point.y))samples[{u,v}]=point;
        }
    }
    std::vector<FlatLightSample> result;
    for(const auto &sample:samples) result.push_back({sample.second,sample.first.first,sample.first.second});
    if(clipped&&flatCoverageBudget)addClippedFlatSamples(sectorIndex,image,samples,result);
    return flatLightSamples.emplace(key,std::move(result)).first->second;
}
void collectFlatLights() {
    switchLights.clear();
#ifdef DOOM_TEST_DRIVER
    static const bool audit=M_CheckParm((char*)"-materiallightaudit")!=0;
    auditMaterials=audit&&leveltime>=12&&auditedLevel!=r_levelserial&&bakedLightsActive;
    if(auditMaterials)auditedMaterials.clear();
#endif
    if(!settings.emissive) return;
    const bool track=sourceDrivenLighting(settings);
    if(track&&flatMaterials.size()!=(size_t)numsectors)flatMaterials.assign(numsectors,{-1,-1});
    for(int i=0;i<numsectors;++i) for(int ceiling=0;ceiling<2;++ceiling) {
        const sector_t &sector=sectors[i];int flat=ceiling?sector.ceilingpic:sector.floorpic;
        if(track) {
            auto &previous=flatMaterials[i][ceiling];
            if(previous>=0&&previous!=flat) {
                auto emitting=[](int pic) {
                    return pic!=skyflatnum&&(lumpImage(firstflat+pic,true).emissionWeight>0||
                        lumpImage(firstflat+flattranslation[pic],true).emissionWeight>0);
                };
                if(emitting(previous)||emitting(flat))materialReplaced();
            }
            previous=flat;
        }
        if(flat==skyflatnum)continue;
        int lump=firstflat+flattranslation[flat];const Image &image=lumpImage(lump,true);
#ifdef DOOM_TEST_DRIVER
        if(auditMaterials)++auditedMaterials[&image].surfaces;
#endif
        if(image.emissionWeight==0)continue;
        float z=sectorHeight(&sector,ceiling)+(ceiling?-8:8);
        for(const auto &sample:flatSamples(i,lump,image))
            surfaceLight({0,i,ceiling,sample.u,sample.v,sample.component},i,sample.point.x,sample.point.y,z,image,ceiling,256,0,1,sample.fraction);
    }
}
void collectSurfaceLights() {
#ifdef DOOM_TEST_DRIVER
    MaterialAuditFrame report;
#endif
    uint64_t now=SDL_GetTicksNS();
    float seconds=surfaceLightTime?(now-surfaceLightTime)/1e9f:1.0f/60;
    surfaceLightTime=now;
    // Switches change texture independently of sector lighting. Keep their
    // small light live, including bake-only mode, so neither state is baked in.
    lightKind=I_RENDER3D_LIGHT_SURFACE;
    if(settings.emissive) {
        std::vector<std::pair<float,size_t>> nearby;
        for(size_t i=0;i<switchLights.size();++i) {
            const auto &p=switchLights[i];
            float distance=std::hypot(std::hypot(p.x-surfaceEye[0],p.y-surfaceEye[1]),p.z-surfaceEye[2]);
            if(distance<1024)nearby.emplace_back(distance,i);
        }
        std::sort(nearby.begin(),nearby.end());
        // Bound the combined strength as well as each individual switch:
        // eight clustered indicators must not flood a small control room.
        float switchBudget=.9f;
        for(size_t i=0;i<std::min(size_t(8),nearby.size())&&flashes.count<maxLights&&switchBudget>0;++i) {
            const auto &p=switchLights[nearby[i].second];auto n=panelNormal(p);
            float fade=std::clamp((1024-nearby[i].first)/256.0f,0.0f,1.0f);
            float strength=std::min(p.strength*fade,switchBudget);
            if(strength<.01f)continue;
            appendLight(p.x,p.y,p.z,p.radius,strength,p.color,nullptr,0.35f,true,0,{0,n[0],n[1],0});
            switchBudget-=strength;
        }
    }
    lightKind=bakeOnlyActive?I_RENDER3D_LIGHT_FOG:I_RENDER3D_LIGHT_SURFACE;
    if(bakeOnlyActive) {
        // The bake holds these lights; the nearest static lights stay as
        // fog-only lights so the glow around torches and pools remains.
        surfaceSelection.clear();
        if(!settings.fog)return;
        const bool sourceDriven=sourceDrivenLighting(settings);
        if(settings.emissive)for(const auto &light:mergedSurfaceLights(surfaceLights)) {
            float strength=sourceDriven?doom_material_light::effectivePower(light.strength,light.shine):light.strength;
            if(sourceDriven&&strength<=0)continue;
            staticFog.push_back({light.x,light.y,light.z,light.radius,strength,light.color});
        }
        std::vector<std::pair<float,size_t>> scored;
        for(size_t n=0;n<staticFog.size();++n) {
            const StaticFog &f=staticFog[n];
            if(sourceDriven&&!doom_material_light::canEmit(f.strength,f.radius))continue;
            float distance=std::hypot(f.x-surfaceEye[0],f.y-surfaceEye[1]);
            if(distance<=f.radius+512)scored.emplace_back(f.strength/(1+distance/128),n);
        }
        std::sort(scored.begin(),scored.end(),[](const auto &a,const auto &b){return a.first>b.first;});
        for(size_t n=0;n<std::min(size_t(4),scored.size())&&flashes.count<maxLights;++n) {
            const StaticFog &f=staticFog[scored[n].second];
            const auto before=flashes.count;
            appendLight(f.x,f.y,f.z,f.radius,f.strength,f.color,nullptr,0.75f,false);
            if(flashes.count>before)fogOnly[before]=true;
        }
        return;
    }
    if(!settings.emissive) {surfaceSelection.clear();return;}
    surfaceSelection.capacity=settings.fewerSurfaceLights?8:24;
    bool trace=M_CheckParm((char*)"-profilespikes")!=0;
    uint64_t t0=trace?SDL_GetTicksNS():0;
    std::vector<SurfaceLight> lit=mergedSurfaceLights(surfaceLights);
    uint64_t t1=trace?SDL_GetTicksNS():0;
    const bool sourceDriven=sourceDrivenLighting(settings);
    if(!sourceDriven)lit.erase(std::remove_if(lit.begin(),lit.end(),[](const SurfaceLight &l){return l.strength<=0;}),lit.end());
    surfaceSelection.update(lit,surfaceEye,seconds,sourceDriven);
    uint64_t t2=trace?SDL_GetTicksNS():0;
    // Surface lights reach a short way, but walls still stop them: the bake
    // subtracts only their flat share, so their gloss would otherwise show on
    // shiny surfaces through walls. The Performance switch skips the tests.
    // A flickering lamp's copy dims and goes out with its baked light.
    for(const auto &slot:surfaceSelection.slots) {
        const auto &p=slot.light;
        const float strength=sourceDriven?doom_material_light::effectivePower(p.strength,p.shine)*slot.gain:
            p.strength*slot.gain*p.shine;
        if(p.shine<=0||(sourceDriven&&strength<=0))continue;
        auto n=panelNormal(p);
        appendLight(p.x,p.y,p.z,p.radius,strength,p.color,nullptr,0.35f,!settings.unoccludedSurfaceLights,
                    bakedLightsActive?1.0f:0,{std::min(p.extent,p.radius),n[0],n[1],0});
    }
    if(trace){auto end=SDL_GetTicksNS();if(end-t0>2000000)fprintf(stderr,"Surface spike tic %d: merge %.3f select %.3f blockers %.3f ms\n",gametic,(t1-t0)/1e6,(t2-t1)/1e6,(end-t2)/1e6);}
}

// How much glowing wall is round a point, as glowing area (texels that glow, in square units) within
// about 384 units, from the map as it starts. A lamp strip or a fire wall's few lines is a fixture and
// lights in full; SIGIL lines whole rooms with fire and red rock (FIREWALA, ROCKRED1), and every 256-unit
// cell of it was a lamp of its own, flooding the room in red. Past glowCrowdRef the wall lights dim by
// the square root, so a room of glow lights about as a room twice its glow would, not N times; the
// walls' own glow, drawn on them, stays.
constexpr float glowCrowdCell=64,glowCrowdRadius=384,glowCrowdRef=32000;
static std::vector<float> glowCrowd;
static unsigned glowCrowdLevel=~0u;
static float glowCrowdX,glowCrowdY;
static int glowCrowdW,glowCrowdH;
static void buildGlowCrowding() {
    glowCrowdLevel=r_levelserial;
    float minX=1e30f,minY=1e30f,maxX=-1e30f,maxY=-1e30f;
    for(int i=0;i<numvertexes;++i) {
        float vx=units(vertexes[i].x),vy=units(vertexes[i].y);
        minX=std::min(minX,vx);minY=std::min(minY,vy);maxX=std::max(maxX,vx);maxY=std::max(maxY,vy);
    }
    if(numvertexes==0)minX=minY=maxX=maxY=0;
    glowCrowdX=minX;glowCrowdY=minY;
    glowCrowdW=(int)((maxX-minX)/glowCrowdCell)+2;glowCrowdH=(int)((maxY-minY)/glowCrowdCell)+2;
    std::vector<float> area((size_t)glowCrowdW*glowCrowdH,0);
    auto add=[&](const line_t &line,int tex,float height) {
        if(tex<=0||height<=0)return;
        const Image &image=wallImage(tex);
        if(image.emissionWeight<=0)return;
        float ax=units(line.v1->x),ay=units(line.v1->y),bx=units(line.v2->x),by=units(line.v2->y),len=std::hypot(bx-ax,by-ay);
        int steps=std::max(1,(int)std::ceil(len/32));
        float each=image.emissionCoverage*len*height/steps;
        for(int k=0;k<steps;++k) {
            float t=(k+0.5f)/steps;
            int cx=(int)((ax+(bx-ax)*t-glowCrowdX)/glowCrowdCell),cy=(int)((ay+(by-ay)*t-glowCrowdY)/glowCrowdCell);
            area[(size_t)cy*glowCrowdW+cx]+=each;
        }
    };
    for(int i=0;i<numlines;++i)for(int side=0;side<2;++side) {
        const line_t &line=lines[i];
        if(line.sidenum[side]<0)continue;
        const side_t &s=sides[line.sidenum[side]];
        const sector_t *front=s.sector,*back=line.sidenum[side^1]>=0?sides[line.sidenum[side^1]].sector:nullptr;
        float floor=units(front->floorheight),ceiling=units(front->ceilingheight);
        if(!back) {add(line,s.midtexture,ceiling-floor);continue;}
        float backFloor=units(back->floorheight),backCeiling=units(back->ceilingheight);
        if(!(front->ceilingpic==skyflatnum&&back->ceilingpic==skyflatnum))add(line,s.toptexture,ceiling-backCeiling);
        add(line,s.bottomtexture,backFloor-floor);
        if(s.midtexture>0)add(line,s.midtexture,std::min((float)wallImage(s.midtexture).height,std::min(ceiling,backCeiling)-std::max(floor,backFloor)));
    }
    // A tent round each cell, out to glowCrowdRadius, in x and then y.
    int reach=(int)(glowCrowdRadius/glowCrowdCell);
    std::vector<float> across(area.size(),0);glowCrowd.assign(area.size(),0);
    for(int y=0;y<glowCrowdH;++y)for(int x=0;x<glowCrowdW;++x) {
        float sum=0;
        for(int d=-reach;d<=reach;++d)if(x+d>=0&&x+d<glowCrowdW)sum+=area[(size_t)y*glowCrowdW+x+d]*(1-std::abs(d)/(reach+1.0f));
        across[(size_t)y*glowCrowdW+x]=sum;
    }
    for(int y=0;y<glowCrowdH;++y)for(int x=0;x<glowCrowdW;++x) {
        float sum=0;
        for(int d=-reach;d<=reach;++d)if(y+d>=0&&y+d<glowCrowdH)sum+=across[(size_t)(y+d)*glowCrowdW+x]*(1-std::abs(d)/(reach+1.0f));
        glowCrowd[(size_t)y*glowCrowdW+x]=sum;
    }
}
// The strength a wall tile's light keeps where glow crowds (1: a fixture, in full).
static float glowCrowding(float x,float y) {
    if(glowCrowdLevel!=r_levelserial)buildGlowCrowding();
    float fx=(x-glowCrowdX)/glowCrowdCell-0.5f,fy=(y-glowCrowdY)/glowCrowdCell-0.5f;
    int x0=std::clamp((int)std::floor(fx),0,glowCrowdW-1),y0=std::clamp((int)std::floor(fy),0,glowCrowdH-1);
    int x1=std::min(x0+1,glowCrowdW-1),y1=std::min(y0+1,glowCrowdH-1);
    float tx=std::clamp(fx-x0,0.0f,1.0f),ty=std::clamp(fy-y0,0.0f,1.0f);
    auto cell=[&](int cx,int cy){return glowCrowd[(size_t)cy*glowCrowdW+cx];};
    float crowd=(cell(x0,y0)*(1-tx)+cell(x1,y0)*tx)*(1-ty)+(cell(x0,y1)*(1-tx)+cell(x1,y1)*tx)*ty;
    return crowd>glowCrowdRef?std::sqrt(glowCrowdRef/crowd):1.0f;
}

static const std::vector<WallPointSample> &clippedWallPoints(size_t key,int texture,const Image &image,
        const std::array<float,4> &bounds,int firstU,int lastU,int firstV,int lastV) {
    auto &cached=wallPointSamples[key];
    if(cached.texture==texture&&cached.mask==image.explicitEmissionHash&&cached.bounds==bounds)return cached.points;
    cached={texture,image.explicitEmissionHash,bounds,{}};
    const auto [left,length,top,bottom]=bounds;
    const float right=left+length;
    const int w=image.width,h=image.height,stepU=std::max(1,64/w),stepV=std::max(1,64/h);
    const bool single=lastU<firstU||lastV<firstV;
    if(single) {lastU=firstU;lastV=firstV;}
    const double fullWeight=image.emissionCoverage*w*h*255.0*stepU*stepV;
    size_t budget=256*1024;
    for(int u=firstU;u<=lastU;u+=stepU)for(int v=firstV;v<=lastV;v+=stepV) {
        const float a=u==firstU?left:float(u*w),b=u+stepU>lastU?right:float((u+stepU)*w);
        const float c=v==firstV?top:float(v*h),d=v+stepV>lastV?bottom:float((v+stepV)*h);
        const bool complete=a==u*w&&b==(u+stepU)*w&&c==v*h&&d==(v+stepV)*h;
        if(complete) {
            const float atV=image.emissionV+v*h;
            cached.points.push_back({u,v,image.emissionU+u*w,atV,
                std::max(0.f,std::min({h*stepV*.5f,atV-top,bottom-atV})),1.f});
        } else if(budget) {
            const auto emission=doom_material_light::integrateWallEmission(image.emission.data(),w,h,a,c,b,d,budget);
            if(emission.weight>0) {
                const auto p=emission.center();
                cached.points.push_back({u,v,p.x,p.y,std::max(0.f,std::min(p.y-c,d-p.y)),
                    float(std::min(1.0,emission.weight/fullWeight))});
            }
        }
    }
    if(!budget&&!clippedWallLimitReported) {
        fprintf(stderr,"3D material lighting: clipped wall coverage budget reached.\n");
        clippedWallLimitReported=true;
    }
    return cached.points;
}

void wallLights(const line_t &line,int side,Point a,Point b,float bottom,float top,int tex,float anchor,int span) {
    if(!settings.emissive||tex<=0||top<=bottom)return;
    const Image &image=wallImage(texturetranslation[tex]);
#ifdef DOOM_TEST_DRIVER
    if(auditMaterials)++auditedMaterials[&image].surfaces;
#endif
    float u=units(sides[line.sidenum[side]].textureoffset),len=std::hypot(b.x-a.x,b.y-a.y);
    if(image.emissionWeight<=0||len<=0)return;
    float nx=(b.y-a.y)/len,ny=-(b.x-a.x)/len;
    int facing=2+((int)std::lround(std::atan2(ny,nx)/(float)(M_PI/4))+8)%8;
    int firstU=(int)std::ceil((u-image.emissionU)/image.width);
    int lastU=(int)std::floor((u+len-image.emissionU)/image.width);
    int firstV=(int)std::ceil((anchor-top-image.emissionV)/image.height);
    int lastV=(int)std::floor((anchor-bottom-image.emissionV)/image.height);
    int stepV=std::max(1,64/image.height);
    if(sourceDrivenLighting(settings)&&!image.emission.empty()) {
        const auto &points=clippedWallPoints(size_t(&line-lines)*8+side*4+span,texturetranslation[tex],image,
            {u,len,anchor-top,anchor-bottom},firstU,lastU,firstV,lastV);
        for(const auto &point:points) {
            const float t=std::clamp((point.atU-u)/len,0.f,1.f);
            const float x=a.x+(b.x-a.x)*t+nx*DOOM_PANEL_OFFSET,y=a.y+(b.y-a.y)*t+ny*DOOM_PANEL_OFFSET;
            surfaceLight({1,int(&line-lines),side,point.u,point.v,span},int(sides[line.sidenum[side]].sector-sectors),
                x,y,anchor-point.atV,image,facing,224,point.extent,glowCrowding(x,y),point.coverage);
        }
        return;
    }
    // A clipped strip may contain glowing pixels without containing the whole
    // texture's emission centroid. Place a light at the visible glow instead.
    if((firstU>lastU||firstV>lastV)&&!image.emission.empty()){
        if(wallEmissionIntegrals.size()!=size_t(numlines))wallEmissionIntegrals.resize(numlines);
        auto &cached=wallEmissionIntegrals[&line-lines][side*4+span];
        std::array<float,4> bounds={u,len,anchor-top,anchor-bottom};
        int material=texturetranslation[tex];
        bool hit=cached.texture==material&&cached.mask==image.explicitEmissionHash&&cached.bounds==bounds;
        if(!hit||M_CheckParm((char*)"-verifysurfacecache")) {
            double weight=0,centerU=0,centerV=0;
            for(int v=(int)std::floor(anchor-top);v<(int)std::ceil(anchor-bottom);++v)
                for(int uu=(int)std::floor(u);uu<(int)std::ceil(u+len);++uu){
                    int px=(uu%image.width+image.width)%image.width,py=(v%image.height+image.height)%image.height;
                    float cover=std::max(0.f,std::min(u+len,uu+1.f)-std::max(u,float(uu)))
                        *std::max(0.f,std::min(anchor-bottom,v+1.f)-std::max(anchor-top,float(v)));
                    double w=image.emission[py*image.width+px]*cover;
                    weight+=w;centerU+=(uu+.5)*w;centerV+=(v+.5)*w;
                }

            if(hit&&(cached.weight!=weight||cached.centerU!=centerU||cached.centerV!=centerV))
                I_Error((char*)"Cached wall emission differs from pixel integration");
            cached={material,image.explicitEmissionHash,bounds,weight,centerU,centerV};
        }
        double weight=cached.weight,centerU=cached.centerU,centerV=cached.centerV;
        if(weight>0){
            float t=std::clamp(float(centerU/weight-u)/len,0.f,1.f),z=std::clamp(float(anchor-centerV/weight),bottom,top);
            float px=a.x+(b.x-a.x)*t+nx*DOOM_PANEL_OFFSET;
            float py=a.y+(b.y-a.y)*t+ny*DOOM_PANEL_OFFSET;
            surfaceLight({1,(int)(&line-lines),side,firstU,firstV,span},int(sides[line.sidenum[side]].sector-sectors),
                px,py,z,image,facing,224,std::min(top-z,z-bottom),glowCrowding(px,py));
        }
        return;
    }
    for(int tileU=firstU;tileU<=lastU;tileU+=std::max(1,64/image.width)) {
        float along=image.emissionU+tileU*image.width-u,t=along/len;
        for(int tileV=firstV;tileV<=lastV;tileV+=stepV) {
            // Each sample stands for the rows of tiles it skips, within the wall piece.
            float z=anchor-image.emissionV-tileV*image.height;
            float extent=std::max(0.0f,std::min({image.height*stepV*0.5f,top-z,z-bottom}));
            float x=a.x+(b.x-a.x)*t+nx*DOOM_PANEL_OFFSET,y=a.y+(b.y-a.y)*t+ny*DOOM_PANEL_OFFSET;

            surfaceLight({1,(int)(&line-lines),side,tileU,tileV,span},int(sides[line.sidenum[side]].sector-sectors),
                x,y,z,image,facing,224,extent,glowCrowding(x,y));
        }
    }
}
void clearSurfaceLights() {
    switchLights.clear();
    wallEmissionIntegrals.clear();wallPointSamples.clear();mergeInputs.clear();mergeResult.clear();mergeMembership.clear();mergeWeights.clear();
    surfaceLights.clear();surfaceSelection.clear();flatLightSamples.clear();surfaceGroups.clear();lampRanges.clear();surfaceLightTime=0;
    flatCoverageBudget=flatCoverageLimit;clippedWallLimitReported=false;wallMaterials.clear();flatMaterials.clear();
}
} // namespace doom3d

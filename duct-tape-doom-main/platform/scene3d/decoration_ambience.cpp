/* Cosmetic scavenger ambience: independent of gameplay actors and combat RNG.
   The manager remembers at most 256 nearby eligible bodies, picks the closest
   12 active emitters, and draws particles ONLY from the current live thing
   iteration (no dereference of cached pointers after removal or resurrection).
   Future decoration effects should extend BodyKind and the two renderers below. */
#include "internal.h"
#include "../decoration_ambience_rules.h"
#include <cstdint>
#include <unordered_set>

namespace doom3d {
namespace {
using doom_decor::BodyKind;
struct Emitter {
    BodyKind kind=BodyKind::None;
    int firstTic=0,type=-1;
    float x=0,y=0,z=0;
    uint32_t seed=0;
};
static std::unordered_map<const mobj_t*,Emitter> known;
static std::vector<const mobj_t*> selected;
static unsigned currentLevel=~0u;
static int lastScan=-1;
static unsigned fliesDrawn=0,wormsDrawn=0,critterBlocksDrawn=0;

// Draw primitive budget. This extra ceiling prevents a map containing several
// exotic species from multiplying the number of vertex blocks per creature.
bool reserveCritter(unsigned blocks) {
    if(wormsDrawn>=doom_decor::wormLimit ||
       critterBlocksDrawn+blocks>doom_decor::critterBlockLimit)return false;
    ++wormsDrawn;critterBlocksDrawn+=blocks;return true;
}

// Two overlapping screen-facing blocks approximate a rounded cross section:
// the wide, shallow midsection and narrower, full-height cap remove the
// square corners of a single billboard. Every block is charged to the
// existing critter geometry budget; no new shaders or textures.
constexpr unsigned wormBlocksPerSegment=2;
void roundedWormSegment(const Uniforms &camera,float x,float y,float z,
                        float width,float tall,const std::array<float,3> &color,float alpha) {
    pixelBlock(particleVertices,camera,x,y,z,width,tall*.66f,
               color,false,nullptr,true,nullptr,alpha);
    // Small lighting shift gives the center a rounder cross-section.
    const std::array<float,3> center={color[0]*1.035f,color[1]*1.035f,color[2]*1.035f};
    // Nudge the cap toward the camera to prevent coplanar depth rejection.
    float dx=camera.eye[0]-x,dy=camera.eye[1]-y,dz=camera.eye[2]-z;
    float inv=1.f/std::sqrt(dx*dx+dy*dy+dz*dz+1e-4f);
    constexpr float capOffset=.055f;
    pixelBlock(particleVertices,camera,x+dx*inv*capOffset,y+dy*inv*capOffset,
               z+dz*inv*capOffset,width*.70f,tall,
               center,false,nullptr,true,nullptr,alpha);
}

// Explicit vanilla biological types from info.c. Nonorganic, unknown and
// PWAD-defined decorations are deliberately not inferred from sprite pixels.
BodyKind bodyKind(const mobj_t &thing) {
    switch(thing.type) {
    // Original placed dead bodies, gibs and flattened remains.
    case MT_MISC61:case MT_MISC62:case MT_MISC63:case MT_MISC64:
    case MT_MISC65:case MT_MISC66:case MT_MISC67:case MT_MISC68:
    case MT_MISC69:case MT_MISC71:case MT_MISC84:
        return BodyKind::Remains;
    // Heads/bodies on stakes and impalement decorations.
    case MT_MISC70:case MT_MISC72:case MT_MISC73:
    case MT_MISC74:case MT_MISC75:
        return BodyKind::Impaled;
    // Both solid and pass-through hanging bodies, including twitching meat.
    case MT_MISC51:case MT_MISC52:case MT_MISC53:case MT_MISC54:
    case MT_MISC55:case MT_MISC56:case MT_MISC57:case MT_MISC58:
    case MT_MISC59:case MT_MISC60:case MT_MISC78:case MT_MISC79:
    case MT_MISC80:case MT_MISC81:case MT_MISC82:case MT_MISC83:
        return BodyKind::Hanging;
    default:break;
    }
    if(thing.player||!thing.info||thing.health>0||!(thing.flags&MF_CORPSE)
       ||!(thing.info->flags&MF_COUNTKILL))return BodyKind::None;
    return BodyKind::Corpse;
}
// Map the existing DOOM mobj types, not sprite names: the actor may be using
// an animation frame from a custom WAD, but no sprite pixel analysis is needed.
// Only conventional monster corpses get specialized scavengers.
doom_decor::CorpseProfile corpseProfile(const Emitter &e) {
    using doom_decor::CorpseProfile;
    if(e.kind!=BodyKind::Corpse)return CorpseProfile::Generic;
    switch(e.type) {
    case MT_FATSO:case MT_BRUISER:return CorpseProfile::LargeRedWorms;
    case MT_UNDEAD:return CorpseProfile::BoneSpiders;
    case MT_VILE:return CorpseProfile::FireSlug;
    case MT_HEAD:return CorpseProfile::BlueJelly;
    default:return CorpseProfile::Generic;
    }
}
uint32_t thingSeed(const mobj_t &thing) {
    // Stable cosmetic seed for the map object position and class; never P_Random.
    uint32_t x=uint32_t(thing.x),y=uint32_t(thing.y),z=uint32_t(thing.z);
    return doom_decor::hash(uint32_t(thing.type)*0x9e3779b9u ^
        doom_decor::hash(x)^doom_decor::hash(y+0x715177u)^doom_decor::hash(z+0x614d22u));
}
bool settled(const mobj_t &thing, BodyKind kind) {
    if(kind!=BodyKind::Corpse)return true;
    if(thing.z>thing.floorz+4*FRACUNIT)return false;
    return std::abs(thing.momx)<=FRACUNIT&&std::abs(thing.momy)<=FRACUNIT&&
           std::abs(thing.momz)<=FRACUNIT;
}
float distance2(const Emitter &e,const Uniforms &camera) {
    float dx=e.x-camera.eye[0],dy=e.y-camera.eye[1];return dx*dx+dy*dy;
}
void scan(const Uniforms &camera) {
    std::unordered_set<const mobj_t*> present;
    present.reserve(known.size()+16);
    const float track2=doom_decor::trackRadius*doom_decor::trackRadius;
    for(int sector=0;sector<numsectors;++sector)
        for(const mobj_t *thing=sectors[sector].thinglist;thing;thing=thing->snext) {
            BodyKind kind=bodyKind(*thing);
            if(kind==BodyKind::None)continue;
            const float x=units(thing->x),y=units(thing->y);
            float dx=x-camera.eye[0],dy=y-camera.eye[1];
            if(dx*dx+dy*dy>track2)continue;
            auto it=known.find(thing);
            if(it==known.end()) {
                if(known.size()>=doom_decor::trackedLimit)continue;
                Emitter e;
                e.kind=kind;e.type=thing->type;
                e.firstTic=kind==BodyKind::Corpse?leveltime:0;
                e.seed=thingSeed(*thing);e.x=x;e.y=y;e.z=units(thing->z);
                it=known.emplace(thing,e).first;
            } else if(it->second.type!=thing->type||it->second.kind!=kind) {
                // The allocator might reuse an address. Never inherit old timing.
                it->second={kind,kind==BodyKind::Corpse?leveltime:0,thing->type,
                            x,y,units(thing->z),thingSeed(*thing)};
            }
            it->second.x=x;it->second.y=y;it->second.z=units(thing->z);
            present.insert(thing);
        }
    for(auto it=known.begin();it!=known.end();) {
        if(!present.count(it->first))it=known.erase(it);
        else ++it;
    }
}
bool fliesReady(const Emitter &e,int preset) {
    return doom_decor::allowsFlies(corpseProfile(e))&&doom_decor::flies(e.seed)&&
           leveltime-e.firstTic>=doom_decor::flyDelayTics(e.seed,preset,TICRATE);
}
bool crittersReady(const Emitter &e,int preset) {
    if(!doom_decor::wormSurface(e.kind))return false;
    auto profile=corpseProfile(e);
    bool selected=profile==doom_decor::CorpseProfile::Generic?
        doom_decor::worms(e.seed):doom_decor::specialCreature(e.seed,profile);
    return selected && leveltime-e.firstTic>=doom_decor::wormDelayTics(e.seed,preset,TICRATE);
}
} // namespace

void decorationAmbienceReset() {
    known.clear();selected.clear();lastScan=-1;currentLevel=~0u;
}

void decorationAmbienceBegin(const Uniforms &camera) {
    selected.clear();fliesDrawn=wormsDrawn=critterBlocksDrawn=0;
    if(currentLevel!=r_levelserial) {decorationAmbienceReset();currentLevel=r_levelserial;}
    if(camera.effects[2]!=0||
       !doom_decor::active(unsigned(std::max(0,totalkills)),settings.decorationAmbience,settings.decorationInsects)) {
        if(totalkills>=int(doom_decor::monsterLimit))known.clear();
        return;
    }
    if(lastScan<0||leveltime<lastScan||leveltime-lastScan>=TICRATE) {
        scan(camera);lastScan=leveltime;
    }
    std::vector<std::pair<float,const mobj_t*>> closest;
    closest.reserve(known.size());
    float radius2=doom_decor::effectRadius*doom_decor::effectRadius;
    for(const auto &entry:known) {
        const Emitter &e=entry.second;
        float d2=distance2(e,camera);
        if(d2>radius2||(!fliesReady(e,settings.decorationDelay)&&
          !(settings.decorationInsects>=2&&crittersReady(e,settings.decorationDelay))))continue;
        closest.push_back({d2,entry.first});
    }
    std::sort(closest.begin(),closest.end(),[](const auto &a,const auto &b){return a.first<b.first;});
    for(size_t i=0;i<std::min(size_t(doom_decor::emitterLimit),closest.size());++i)
        selected.push_back(closest[i].second);
}

void decorationAmbienceThing(const mobj_t *thing,const Uniforms &camera) {
    // 'thing' is supplied by the *current* geometry iteration. Cached keys
    // are compared only; stale pointers are never dereferenced.
    if(selected.empty()||!thing||
       std::find(selected.begin(),selected.end(),thing)==selected.end())return;
    auto found=known.find(thing);if(found==known.end())return;
    const Emitter &e=found->second;
    if(bodyKind(*thing)!=e.kind||!settled(*thing,e.kind))return;
    bool fly=fliesReady(e,settings.decorationDelay);
    bool worm=settings.decorationInsects>=2&&crittersReady(e,settings.decorationDelay);
    if(!fly&&!worm)return;
    float x=units(thing->x),y=units(thing->y),z=units(thing->z);
    float floor=units(thing->floorz);
    float height=std::clamp(units(thing->height),8.f,96.f);
    float radius=std::clamp(units(thing->radius),8.f,40.f);
    float t=(leveltime+renderFraction)/float(TICRATE);
    float light=thing->subsector&&thing->subsector->sector?lighting(thing->subsector->sector):0.5f;
    light=std::clamp(light,0.12f,1.f);
    const std::array<float,3> flyColor={.10f+.07f*light,.09f+.06f*light,.075f+.055f*light};
    // A nearly black head and a warm flesh-colored body, both shaded by
    // sector light. Keep these cosmetic colors independent of gameplay RNG.
    const std::array<float,3> wormHeadColor={.025f+.045f*light,.023f+.035f*light,.023f+.030f*light};
    const std::array<float,3> wormSkinColor={.48f+.35f*light,.30f+.28f*light,.25f+.23f*light};
    if(fly) {
        int count=3+int(doom_decor::hash(e.seed^0x732ac9u)%3u);
        for(int n=0;n<count&&fliesDrawn<doom_decor::flyLimit;++n,++fliesDrawn) {
            float phase=6.2831853f*doom_decor::unit(e.seed+uint32_t(n)*971u);
            float speed=1.1f+doom_decor::unit(e.seed+uint32_t(n)*13u)*1.5f;
            float a=phase+t*speed*(n%2?-1.f:1.f);
            float orbit=std::min(radius+5.f,8.f+doom_decor::unit(e.seed+uint32_t(n)*241u)*12.f);
            float fx=x+std::cos(a)*orbit,fy=y+std::sin(a)*orbit;
            float center=e.kind==BodyKind::Hanging?z+height*0.46f:
                         e.kind==BodyKind::Impaled?z+height*0.68f:floor+std::min(16.f,height*.5f);
            float fz=center+4.f*std::sin(t*3.3f+phase)+4.f*doom_decor::unit(e.seed+uint32_t(n)*7u);
            pixelBlock(particleVertices,camera,fx,fy,fz,.62f,1.1f,flyColor,false,nullptr,true,nullptr,.78f);
        }
    }
    if(worm && z<=floor+4.f) {
        const auto profile=corpseProfile(e);
        using doom_decor::CorpseProfile;
        if(profile==CorpseProfile::Generic) {
        // Distinct worms must be visible above the depth-tested floor and
        // outside the centre of a collapsed sprite. The old 0.48-unit-tall,
        // floor-hugging resting quads were effectively sub-pixel at typical
        // combat distances (even when a worm was selected).
        int count=1+int(doom_decor::hash(e.seed^0xb912ed11u)%3u);
        for(int n=0;n<count;++n) {
            if(!reserveCritter(5*wormBlocksPerSegment))break;
            float phase=doom_decor::unit(e.seed+uint32_t(n)*103u)*6.2831853f;
            float a=phase+.24f*std::sin(t*.75f+phase);
            float orbit=radius*.7f;
            float sx=x+std::cos(phase)*orbit+2.2f*std::sin(t*.43f+phase);
            float sy=y+std::sin(phase)*orbit+2.2f*std::cos(t*.39f+phase);
            // Five connected, slightly tapering segments form one worm.
            // The leading (part 0) segment is a small black head, followed
            // by alternating pink/beige skin tones. Every variation is stable
            // per worm and segment, so it never flickers during animation.
            float skinVariation=.92f+.13f*doom_decor::unit(e.seed+uint32_t(n)*4099u+0x13572u);
            for(int part=0;part<5;++part) {
                float along=(part-2)*1.65f;
                float wave=.65f*std::sin(t*2.f+part*1.1f+phase);
                float px=sx+std::cos(a)*along-std::sin(a)*wave;
                float py=sy+std::sin(a)*along+std::cos(a)*wave;
                float pz=floor+1.8f+.20f*std::sin(t*1.3f+part*.8f+phase);
                float taper=part==0?.78f:part==4?.73f:part==3?.89f:1.f;
                float band=skinVariation*(part%2==0?.92f:1.06f);
                std::array<float,3> color=part==0?wormHeadColor:
                    std::array<float,3>{wormSkinColor[0]*band,wormSkinColor[1]*band,wormSkinColor[2]*band};
                roundedWormSegment(camera,px,py,pz,2.4f*taper,1.5f*taper,color,.94f);
            }
        }
        } else if(profile==CorpseProfile::LargeRedWorms) {
            // Mancubus and Baron: 1-3 dark maroon earthworms, about 1.6x the
            // normal worm's overall footprint (six rounded segments).
            // The muted red is closer to DOOM's dark blood tones than the
            // earlier bright saturated red. No extra GPU textures are needed.
            constexpr int segments=6;
            int count=1+int(doom_decor::hash(e.seed^0xc472a1u)%3u);
            for(int n=0;n<count;++n) {
                if(!reserveCritter(segments*wormBlocksPerSegment))break;
                float phase=6.2831853f*doom_decor::unit(e.seed+uint32_t(n)*131u);
                float a=phase+.18f*std::sin(t*.60f+phase);
                float orbit=radius*.74f;
                float sx=x+std::cos(phase)*orbit+2.3f*std::sin(t*.35f+phase);
                float sy=y+std::sin(phase)*orbit+2.3f*std::cos(t*.31f+phase);
                float tint=.90f+.16f*doom_decor::unit(e.seed+uint32_t(n)*1511u+0x491d17u);
                for(int part=0;part<segments;++part) {
                    float along=(part-2.5f)*2.25f;
                    float wave=.88f*std::sin(t*1.65f+part*.78f+phase);
                    float px=sx+std::cos(a)*along-std::sin(a)*wave;
                    float py=sy+std::sin(a)*along+std::cos(a)*wave;
                    float pz=floor+2.25f+.22f*std::sin(t*1.15f+part*.75f+phase);
                    float taper=part==0?.72f:part==segments-1?.64f:
                                part==1?.89f:part==segments-2?.85f:1.f;
                    float band=tint*(part%2?.92f:1.05f);
                    const std::array<float,3> tone={
                        (.30f+.22f*light)*band,
                        (.040f+.061f*light)*band,
                        (.032f+.049f*light)*band};
                    roundedWormSegment(camera,px,py,pz,3.85f*taper,2.4f*taper,tone,.94f);
                }
            }
        } else if(profile==CorpseProfile::BoneSpiders) {
            // Revenant: a bone-colored abdomen and eight dark leg tips.
            // Deliberately no generic flies or worms.
            int count=1+int(doom_decor::hash(e.seed^0x791f4u)%2u);
            for(int n=0;n<count;++n) {
                if(!reserveCritter(10))break;
                float phase=6.2831853f*doom_decor::unit(e.seed+uint32_t(n)*911u);
                float a=phase+.55f*std::sin(t*1.9f+phase);
                float sx=x+std::cos(phase)*radius*.72f+3.0f*std::sin(t*1.55f+phase);
                float sy=y+std::sin(phase)*radius*.72f+3.0f*std::cos(t*1.40f+phase);
                float dx=std::cos(a),dy=std::sin(a),sideX=-dy,sideY=dx;
                const std::array<float,3> bone={.53f+.32f*light,.49f+.29f*light,.40f+.22f*light};
                const std::array<float,3> leg={.045f+.045f*light,.034f+.039f*light,.032f+.027f*light};
                pixelBlock(particleVertices,camera,sx-dx*.6f,sy-dy*.6f,floor+2.3f,
                           2.6f,1.7f,bone,false,nullptr,true,nullptr,.96f);
                pixelBlock(particleVertices,camera,sx+dx*1.3f,sy+dy*1.3f,floor+2.1f,
                           1.6f,1.4f,bone,false,nullptr,true,nullptr,.96f);
                for(int legIndex=0;legIndex<8;++legIndex) {
                    int pair=legIndex/2,side=(legIndex%2)?1:-1;
                    float back=(float(pair)-1.5f)*1.55f;
                    float step=.45f*std::sin(t*5.5f+pair*1.1f+phase+side);
                    float px=sx+dx*back+sideX*(side*(2.6f+.5f*pair));
                    float py=sy+dy*back+sideY*(side*(2.6f+.5f*pair));
                    pixelBlock(particleVertices,camera,px,py,floor+1.55f+step,
                               .92f,.95f,leg,false,nullptr,true,nullptr,.97f);
                }
            }
        } else if(profile==CorpseProfile::FireSlug) {
            // Arch-vile: a fiery nudibranch silhouette (slender body, animated
            // paired side frills). Palette color only: no dynamic light source.
            int count=1+int(doom_decor::hash(e.seed^0x961ac2u)%2u);
            for(int n=0;n<count;++n) {
                if(!reserveCritter(14))break;
                float phase=6.2831853f*doom_decor::unit(e.seed+uint32_t(n)*277u);
                float a=phase+.20f*std::sin(t*.75f+phase);
                float sx=x+std::cos(phase)*radius*.66f+1.6f*std::sin(t*.42f+phase);
                float sy=y+std::sin(phase)*radius*.66f+1.6f*std::cos(t*.38f+phase);
                float dx=std::cos(a),dy=std::sin(a),sideX=-dy,sideY=dx;
                for(int part=0;part<6;++part) {
                    float u=float(part)-2.5f;
                    float wave=.65f*std::sin(t*1.75f+part*.9f+phase);
                    float px=sx+dx*(u*1.8f)+sideX*wave;
                    float py=sy+dy*(u*1.8f)+sideY*wave;
                    float taper=part==0?.68f:part==5?.66f:1.f;
                    const std::array<float,3> fire=part%2?
                        std::array<float,3>{.89f,.31f+.20f*light,.045f}:
                        std::array<float,3>{.64f+.20f*light,.105f+.12f*light,.028f};
                    pixelBlock(particleVertices,camera,px,py,floor+2.0f+.14f*wave,
                               3.5f*taper,1.7f*taper,fire,false,nullptr,true,nullptr,.94f);
                }
                for(int fringe=0;fringe<8;++fringe) {
                    int part=fringe/2+1,side=fringe%2?1:-1;
                    float u=float(part)-2.5f;
                    float sway=.8f*std::sin(t*2.35f+fringe*1.35f+phase);
                    float px=sx+dx*(u*1.8f)+sideX*(side*(2.4f+sway));
                    float py=sy+dy*(u*1.8f)+sideY*(side*(2.4f+sway));
                    const std::array<float,3> tip={.98f,.53f+.23f*light,.065f};
                    pixelBlock(particleVertices,camera,px,py,floor+2.6f+.25f*sway,
                               1.25f,1.7f,tip,false,nullptr,true,nullptr,.88f);
                }
            }
        } else if(profile==CorpseProfile::BlueJelly) {
            // Cacodemon: deeper blue (blue-blood-like) jelly and tassels.
            // Keep subtle pulsation but avoid the former bright cyan look.
            int count=1;
            for(int n=0;n<count;++n) {
                if(!reserveCritter(10))break;
                float phase=6.2831853f*doom_decor::unit(e.seed+uint32_t(n)*421u);
                float sx=x+std::cos(phase)*radius*.68f+1.4f*std::sin(t*.31f+phase);
                float sy=y+std::sin(phase)*radius*.68f+1.4f*std::cos(t*.33f+phase);
                float pulse=.91f+.09f*std::sin(t*2.0f+phase);
                const std::array<float,3> rim={.034f,.19f*pulse,.49f*pulse};
                const std::array<float,3> core={.052f,.32f*pulse,.68f*pulse};
                pixelBlock(particleVertices,camera,sx,sy,floor+3.0f,
                           4.7f,2.8f,rim,false,nullptr,true,nullptr,.77f);
                pixelBlock(particleVertices,camera,sx,sy,floor+3.6f,
                           2.9f,2.1f,core,false,nullptr,true,nullptr,.82f);
                for(int tail=0;tail<8;++tail) {
                    float a=phase+6.2831853f*float(tail)/8.f;
                    float sway=.7f*std::sin(t*2.3f+tail*.85f+phase);
                    float distance=3.0f+(tail%2?1.6f:.5f)+sway;
                    float px=sx+std::cos(a)*distance,py=sy+std::sin(a)*distance;
                    const std::array<float,3> tassel=tail%2?
                        std::array<float,3>{.030f,.22f*pulse,.51f*pulse}:
                        std::array<float,3>{.042f,.31f*pulse,.62f*pulse};
                    pixelBlock(particleVertices,camera,px,py,floor+1.6f+.25f*sway,
                               1.15f,1.6f,tassel,false,nullptr,true,nullptr,.76f);
                }
            }
        }
    }
}
} // namespace doom3d

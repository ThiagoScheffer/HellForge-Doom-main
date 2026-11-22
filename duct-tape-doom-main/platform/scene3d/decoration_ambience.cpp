/* Cosmetic scavenger ambience: independent of gameplay actors and combat RNG.
   The manager remembers at most 256 nearby eligible bodies, picks the closest
   12 active emitters, and draws particles ONLY from the current live thing
   iteration (no dereference of cached pointers after removal or resurrection). */
#include "internal.h"
#include "../decoration_ambience_rules.h"
#include "../corpse_decomposition.h"
#include <cstdint>
#include <unordered_set>

namespace doom3d {
struct DecorationArt {
    int sprite,frame;
    uint32_t signature;
    float anchorX;
    int detailX,detailY;
};

namespace {
using doom_decor::BodyKind;
enum class HorrorDecor : uint8_t {
    None,Twitching,DryHanging,BloodyHanging,DeadImpaled,LivingImpaled,SkullStake,HeartPillar
};
HorrorDecor horrorDecor(int type) {
    switch(type) {
    case MT_MISC51:case MT_MISC60:return HorrorDecor::Twitching;
    case MT_MISC52:case MT_MISC53:case MT_MISC54:case MT_MISC55:
    case MT_MISC56:case MT_MISC57:case MT_MISC58:case MT_MISC59:return HorrorDecor::DryHanging;
    case MT_MISC78:case MT_MISC79:case MT_MISC80:case MT_MISC81:
    case MT_MISC82:case MT_MISC83:return HorrorDecor::BloodyHanging;
    case MT_MISC74:return HorrorDecor::DeadImpaled;
    case MT_MISC75:return HorrorDecor::LivingImpaled;
    case MT_MISC70:case MT_MISC72:return HorrorDecor::SkullStake;
    case MT_MISC37:return HorrorDecor::HeartPillar;
    default:return HorrorDecor::None;
    }
}
bool matchesSprite(HorrorDecor kind,int sprite) {
    switch(kind) {
    case HorrorDecor::Twitching:return sprite==SPR_GOR1;
    case HorrorDecor::DryHanging:return sprite>=SPR_GOR2&&sprite<=SPR_GOR5;
    case HorrorDecor::BloodyHanging:return sprite>=SPR_HDB1&&sprite<=SPR_HDB6;
    case HorrorDecor::DeadImpaled:return sprite==SPR_POL1;
    case HorrorDecor::LivingImpaled:return sprite==SPR_POL6;
    case HorrorDecor::SkullStake:return sprite==SPR_POL2||sprite==SPR_POL4;
    case HorrorDecor::HeartPillar:return sprite==SPR_COL5;
    default:return false;
    }
}
bool hangs(int sprite) {
    return (sprite>=SPR_GOR1&&sprite<=SPR_GOR5)||(sprite>=SPR_HDB1&&sprite<=SPR_HDB6);
}
// Stock Doom and Doom II patch fingerprints. Coordinates identify the chain
// and an opaque flesh/edge pixel in each frame, independent of collision size.
constexpr DecorationArt artwork[]={
    {SPR_GOR1,0,0x06854e3c, 9.5f,14,65},
    {SPR_GOR1,1,0x7d88d796,10.5f,16,65},
    {SPR_GOR1,2,0x1d9c4818, 9.5f,13,65},
    {SPR_GOR2,0,0xa6dec738,19.5f,31,79},
    {SPR_GOR3,0,0x0880c6bb,16.5f,10,73},
    {SPR_GOR4,0,0xc7552c58, 7.5f,10,63},
    {SPR_GOR5,0,0xfec6683a, 4.5f, 8,49},
    {SPR_HDB1,0,0x550f80af,11.0f,15,72},
    {SPR_HDB2,0,0x497879a9,11.0f,14,65},
    {SPR_HDB3,0,0x2e6a2ce6,11.0f,10,61},
    {SPR_HDB4,0,0xa635ded7,11.0f,10,60},
    {SPR_HDB5,0,0x75c93d55,11.0f, 7,51},
    {SPR_HDB6,0,0xde8a01a4,11.0f, 6,58},
    {SPR_POL1,0,0x6cd3eac9, 0.0f,11,29},
    {SPR_POL2,0,0xcc102c87, 0.0f,24,21},
    {SPR_POL4,0,0x0beef910, 0.0f,22,10},
    {SPR_POL6,0,0x3964768b, 0.0f,23,57},
    {SPR_POL6,1,0x2e8d07dc, 0.0f,25,57},
    {SPR_COL5,1,0xd3c0fc3f, 0.0f,22, 5}
};
struct ArtCheck { int lump=-1;bool valid=false; };
std::array<ArtCheck,std::size(artwork)> checkedArt;
struct DecorationCritter { float x,y,z,reach,age,light;bool spider,detaches; };
std::array<DecorationCritter,doom_decor::emitterLimit> decorationCritters;
size_t decorationCritterCount=0;

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
uint32_t thingSeed(const mobj_t &thing,bool stableHeight=false) {
    // Stable cosmetic seed for the map object position and class; never P_Random.
    uint32_t x=uint32_t(thing.x),y=uint32_t(thing.y),z=stableHeight?0u:uint32_t(thing.z);
    return doom_decor::hash(uint32_t(thing.type)*0x9e3779b9u ^
        doom_decor::hash(x)^doom_decor::hash(y+0x715177u)^doom_decor::hash(z+0x614d22u));
}
bool detailAge(const Emitter &e,int preset,float fraction,float &age,int &event) {
    const auto kind=horrorDecor(e.type);
    if(kind!=HorrorDecor::DryHanging&&kind!=HorrorDecor::SkullStake)return false;
    const bool spider=kind==HorrorDecor::SkullStake;
    const int period=int((spider?30:22)+doom_decor::hash(e.seed^0x76dbu)%20);
    const int tics=int(period*TICRATE*doom_decor::delayScale(preset));
    const int offset=int(doom_decor::hash(e.seed^0x7227u)%unsigned(tics));
    const int shifted=leveltime%tics+offset;
    event=leveltime/tics+shifted/tics;
    age=(shifted%tics+fraction)/TICRATE;
    return age<(spider?5.4f:3.4f);
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
            // Only *selected* aging corpses lose scavengers from Stage 2.
            // The live sector iteration owns the actor; never dereference a
            // pointer cached in known/selected to determine its decay stage.
            if(kind==BodyKind::Corpse && corpseDecayStage(thing)>=2)continue;
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

bool horrorDecorEnabled() {
    return settings.accelerated&&gamestate==GS_LEVEL&&
        doom_decor::horrorActive(unsigned(std::max(0,totalkills)),settings.decorationAmbience);
}

const DecorationArt *decorationArt(const mobj_t &thing,int lump) {
    if(!horrorDecorEnabled()||!matchesSprite(horrorDecor(thing.type),thing.sprite))return nullptr;
    const int frame=thing.frame&FF_FRAMEMASK;
    for(size_t n=0;n<std::size(artwork);++n) {
        const auto &art=artwork[n];
        if(art.sprite!=thing.sprite||art.frame!=frame)continue;
        ArtCheck &check=checkedArt[n];
        if(check.lump!=lump) {
            check={lump,false};
            const int size=W_LumpLength(lump);
            if(size<8||size>2048)return nullptr;
            const auto *bytes=static_cast<const byte*>(W_CacheLumpNum(lump,PU_CACHE));
            uint32_t signature=2166136261u;
            for(int i=0;i<size;++i)signature=(signature^bytes[i])*16777619u;
            check.valid=signature==art.signature;
        }
        return check.valid?&art:nullptr;
    }
    return nullptr;
}

float decorationPivot(const DecorationArt &art,const Image &image,bool flip) {
    return (flip?image.width-art.anchorX:art.anchorX)-image.left;
}

float decorationRoll(const mobj_t &thing,const DecorationArt &art,float fraction,
                     float rx,float ry,float top,float ceiling,float &pivotDown) {
    if(!hangs(art.sprite)||!(thing.flags&MF_SPAWNCEILING))return 0;
    // Some pass-through variants already extend into the ceiling. Keep the
    // pivot at the visible intersection so the chain doesn't slide across it.
    pivotDown=std::max(0.f,top-ceiling);
    const uint32_t seed=thingSeed(thing,true);
    const int duration=(5+doom_decor::hash(seed^0x246u)%4)*TICRATE;
    const int cycle=duration+(3+doom_decor::hash(seed^0x622u)%8)*TICRATE;
    const float phase=doom_decor::unit(seed^0x7bb4u)*2*doomPi;
    auto ageAt=[&](uint32_t salt,int period) {
        const float age=doom_decor::eventAge(leveltime,seed^salt,period)+fraction;
        return age<0?age+period:age;
    };
    const float age=ageAt(0x551u,cycle);
    float degrees=0;
    if(age<duration) {
        const float u=age/duration,envelope=std::sin(float(doomPi)*u);
        const float amplitude=.65f+1.25f*doom_decor::unit(seed^0x2443u);
        degrees=amplitude*envelope*envelope*std::sin(float(2*doomPi)*u+phase);
    }
    if(art.sprite==SPR_GOR1||(art.sprite>=SPR_HDB1&&art.sprite<=SPR_HDB6)) {
        const int twitchPeriod=(12+doom_decor::hash(seed^0x54f3u)%14)*TICRATE;
        const float twitch=ageAt(0x2871u,twitchPeriod);
        if(twitch<21) {
            const float u=twitch/21.f,envelope=std::sin(float(doomPi)*u);
            degrees+=1.25f*envelope*envelope*std::sin(float(4*doomPi)*u+phase);
        }
    }
    // Project one world-space sway direction into each billboard's basis.
    return std::clamp(degrees,-3.f,3.f)*float(doomPi/180)*
        (rx*std::cos(phase)+ry*std::sin(phase));
}

Image &decorationStain(const DecorationArt &art,Image &image,int &key) {
    if(art.sprite<SPR_HDB1||art.sprite>SPR_HDB6||!bloodOn())return image;
    // One shared variant per verified HDB patch, released with the map images.
    const int stainKey=-1600000010-(art.sprite-SPR_HDB1);
    auto found=images.find(stainKey);
    if(found!=images.end()) {key=stainKey;return found->second;}
    auto pixels=image.pixels;
    for(int n=0;n<3;++n) {
        const int length=5+2*n;
        for(int row=0;row<length;++row) {
            int x=art.detailX+(n-1)*3+(row>4&&n==2),y=art.detailY-length+row+n;
            if(x<0||y<0||x>=image.width||y>=image.height)continue;
            size_t offset=2*(y*image.width+x);
            if(pixels[offset+1])
                pixels[offset]=monsterSkinBloodIndex(MT_MISC78,.10f+.13f*(1.f-float(row)/length));
        }
    }
    auto normals=spriteNormals(image.width,image.height,image.pixels.data());
    key=stainKey;
    return images.emplace(key,upload(image.width,image.height,pixels.data(),image.left,image.top,
                                     nullptr,normals.data())).first->second;
}

void decorationBloodTick(const mobj_t &thing,float eyeX,float eyeY,float rx,float ry,unsigned &budget) {
    int min=0,range=0;
    const auto kind=horrorDecor(thing.type);
    switch(kind) {
    case HorrorDecor::Twitching:min=3;range=4;break;
    case HorrorDecor::BloodyHanging:min=2;range=4;break;
    case HorrorDecor::DeadImpaled:min=5;range=6;break;
    case HorrorDecor::LivingImpaled:min=2;range=3;break;
    case HorrorDecor::HeartPillar:
        if(thing.state!=&states[S_HEARTCOL2]||thing.tics!=states[S_HEARTCOL2].tics)return;
        break;
    default:return;
    }
    if(!thing.subsector||!matchesSprite(kind,thing.sprite)||
       thing.sprite<0||thing.sprite>=numsprites||thing.health<=0)return;
    const float x=units(thing.x),y=units(thing.y);
    const float dx=x-eyeX,dy=y-eyeY;
    if(dx*dx+dy*dy>doom_decor::effectRadius*doom_decor::effectRadius)return;
    const uint32_t seed=thingSeed(thing,true);
    if(kind==HorrorDecor::HeartPillar) {
        const int pulse=states[S_HEARTCOL].tics+states[S_HEARTCOL2].tics;
        if(pulse<=0||doom_decor::eventAge(leveltime/pulse,seed^0x227au,5+doom_decor::hash(seed)%5))return;
    } else {
        const int period=(min+doom_decor::hash(seed^0x6366u)%range)*TICRATE;
        if(doom_decor::eventAge(leveltime,seed^0xe512u,period))return;
    }
    const auto &sprite=sprites[thing.sprite];
    const int frame=thing.frame&FF_FRAMEMASK;
    if(frame>=sprite.numframes||sprite.spriteframes[frame].rotate||voxelMonster(thing))return;
    const auto &sf=sprite.spriteframes[frame];
    const int lump=firstspritelump+sf.lump[0];
    const DecorationArt *art=decorationArt(thing,lump);
    if(!art)return;
    const Image &image=lumpImage(lump,false);
    const float top=decorationSpriteTop(thing,image);
    float down=art->detailY+.5f;
    float across=(sf.flip[0]?image.width-art->detailX-.5f:art->detailX+.5f)-image.left;
    float pivotDown=0;
    const float roll=decorationRoll(thing,*art,0,rx,ry,top,units(thing.ceilingz),pivotDown);
    const float pivot=decorationPivot(*art,image,sf.flip[0]);
    doom_decor::SpriteRoll{pivot,pivotDown,std::sin(roll),std::cos(roll)}.apply(across,down);
    --budget;
    decorationBloodDrop(thing,x+rx*across+ry*.45f,y+ry*across-rx*.45f,
                        top-down,doom_decor::hash(seed^uint32_t(leveltime)));
}

// Separate map-wide cosmetic age registry: unlike local fly emitters, its
// timestamps do not reset when the player moves more than 768 units away.
// Contains no owned mobj pointers: keys are compared, never dereferenced.
namespace {
struct DecayRecord { int firstSeen=0,type=-1; };
std::unordered_map<const mobj_t*,DecayRecord> decayBodies;
unsigned decayLevel=~0u;
int lastDecayScan=-1;
int lastDecayDiagnostic=-1;
// Optional command-line diagnostics. -decaytest also selects all eligible
// small corpses, only for a reproducible visual test; the default stays 40%.
bool decayDiagnostics() {return M_CheckParm((char*)"-decaydebug")!=0;}
bool decayTestAll() {return M_CheckParm((char*)"-decaytest")!=0;}

// Only normal, terminal death frames of common small monsters. Excludes
// x-deaths, transient death animations, decorations, and boss-sized actors.
bool normalDecayCorpse(const mobj_t &thing) {
    if(thing.player||thing.health>0||!thing.info||!thing.state||
       !(thing.flags&MF_CORPSE)||!(thing.info->flags&MF_COUNTKILL))return false;
    switch(thing.type) {
    case MT_POSSESSED:return thing.state==&states[S_POSS_DIE5];
    case MT_SHOTGUY:return thing.state==&states[S_SPOS_DIE5];
    case MT_TROOP:return thing.state==&states[S_TROO_DIE5];
    case MT_SERGEANT:return thing.state==&states[S_SARG_DIE6];
    // Additional organic corpses: only the actual terminal stock death state.
    // Never recolor transitional frames, gib deaths or non-stock actor types.
    case MT_FATSO:return thing.state==&states[S_FATT_DIE10];
    case MT_HEAD:return thing.state==&states[S_HEAD_DIE6];
    case MT_KNIGHT:return thing.state==&states[S_BOS2_DIE7];
    case MT_BRUISER:return thing.state==&states[S_BOSS_DIE7];
    default:return false;
    }
}
} // namespace

void corpseDecayReset() {
    decayBodies.clear();decayLevel=~0u;lastDecayScan=-1;lastDecayDiagnostic=-1;
}

void corpseDecayTick() {
    if(decayLevel!=r_levelserial){corpseDecayReset();decayLevel=r_levelserial;}
    if(!settings.corpseDecomposition||!settings.accelerated||
       totalkills>=int(doom_decay::monsterLimit)) {
        if(!decayBodies.empty())decayBodies.clear();
        if(decayDiagnostics() && (lastDecayDiagnostic<0||leveltime-lastDecayDiagnostic>=10*TICRATE)) {
            lastDecayDiagnostic=leveltime;
            std::fprintf(stderr,"HELLFORGE DECAY: INACTIVE enabled=%d accelerated=%d monsters=%d (limit=%u)\n",
                         settings.corpseDecomposition,settings.accelerated,totalkills,doom_decay::monsterLimit);
        }
        return;
    }
    if(lastDecayScan>=0&&leveltime>=lastDecayScan&&leveltime-lastDecayScan<TICRATE)return;
    lastDecayScan=leveltime;
    std::unordered_set<const mobj_t*> current;
    current.reserve(decayBodies.size()+16);
    unsigned eligible=0,selectedCount=0;
    for(int s=0;s<numsectors;++s)for(const mobj_t *thing=sectors[s].thinglist;thing;thing=thing->snext) {
        if(!normalDecayCorpse(*thing))continue;
        ++eligible;
        // Normal: same deterministic 40% selection as flies. -decaytest
        // temporarily selects all eligible bodies to make tests reproducible.
        auto found=decayBodies.find(thing);
        if(found==decayBodies.end()&&!decayTestAll()&&!doom_decor::flies(thingSeed(*thing)))continue;
        ++selectedCount;
        current.insert(thing);
        if(found==decayBodies.end()) {
            if(decayBodies.size()>=doom_decay::trackedLimit)continue;
            decayBodies.emplace(thing,DecayRecord{leveltime,thing->type});
        } else if(found->second.type!=thing->type) {
            found->second={leveltime,thing->type};
        }
    }
    for(auto it=decayBodies.begin();it!=decayBodies.end();) {
        if(!current.count(it->first))it=decayBodies.erase(it);
        else ++it;
    }
    if(decayDiagnostics() && (lastDecayDiagnostic<0||leveltime-lastDecayDiagnostic>=10*TICRATE)) {
        lastDecayDiagnostic=leveltime;
        unsigned ready=0;
        int oldest=0;
        const int delay=settings.corpseDecayDelay;
        const int min=(delay==2||delay==5||delay==8||delay==10)?delay:5;
        for(const auto &pair:decayBodies) {
            const int age=leveltime-pair.second.firstSeen;
            oldest=std::max(oldest,age);
            if(age>=min*60*TICRATE)++ready;
        }
        std::fprintf(stderr,"HELLFORGE DECAY: TRACK enabled=1 delay=%d min monsters=%d eligible=%u selected=%u tracked=%zu ready=%u oldest=%.1f sec testall=%d\n",
                     min,totalkills,eligible,selectedCount,decayBodies.size(),ready,
                     double(oldest)/TICRATE,int(decayTestAll()));
    }
}

int corpseDecayStage(const mobj_t *thing) {
    if(!thing||!settings.corpseDecomposition||!settings.accelerated||
       totalkills>=int(doom_decay::monsterLimit)||!normalDecayCorpse(*thing)||
       decayLevel!=r_levelserial)return 0;
    auto it=decayBodies.find(thing);
    if(it==decayBodies.end()||it->second.type!=thing->type)return 0;
    // Compute the same thresholds locally so this translation unit does not
    // depend on a doom_decay::stage declaration in the installed helper header.
    // Keep the four presets and +45/+120-second stages aligned with the rules.
    const int ageTics=leveltime-it->second.firstSeen;
    if(ageTics<0)return 0;
    const int configured=settings.corpseDecayDelay;
    const int minutes=(configured==2||configured==5||configured==8||configured==10)?configured:5;
    const std::int64_t age=ageTics;
    const std::int64_t start=std::int64_t(minutes)*60*TICRATE;
    if(age<start)return 0;
    if(age<start+std::int64_t(45)*TICRATE)return 1;
    if(age<start+std::int64_t(120)*TICRATE)return 2;
    return 3;
}

void decorationAmbienceReset() {
    known.clear();selected.clear();lastScan=-1;currentLevel=~0u;
    checkedArt={};decorationCritterCount=0;
}

void decorationAmbienceBegin(const Uniforms &camera) {
    selected.clear();fliesDrawn=wormsDrawn=critterBlocksDrawn=0;decorationCritterCount=0;
    if(currentLevel!=r_levelserial) {decorationAmbienceReset();currentLevel=r_levelserial;}
    if(camera.effects[2]!=0||!horrorDecorEnabled()||settings.decorationInsects<=0) {
        if(totalkills>=int(doom_decor::monsterLimit))known.clear();
        return;
    }
    if(lastScan<0||leveltime<lastScan||leveltime-lastScan>=TICRATE) {
        scan(camera);lastScan=leveltime;
    }
    std::vector<std::pair<float,const mobj_t*>> closest;
    closest.reserve(known.size());
    std::vector<std::pair<float,const mobj_t*>> detailOnly;
    float radius2=doom_decor::effectRadius*doom_decor::effectRadius;
    for(const auto &entry:known) {
        const Emitter &e=entry.second;
        float d2=distance2(e,camera);
        if(d2>radius2)continue;
        if(fliesReady(e,settings.decorationDelay)||
           (settings.decorationInsects>=2&&crittersReady(e,settings.decorationDelay)))
            closest.push_back({d2,entry.first});
        else if(settings.decorationInsects>=2) {
            float age;int event;
            if(detailAge(e,settings.decorationDelay,renderFraction,age,event))
                detailOnly.push_back({d2,entry.first});
        }
    }
    auto selectClosest=[&](auto &candidates) {
        std::sort(candidates.begin(),candidates.end(),[](const auto &a,const auto &b){return a.first<b.first;});
        for(size_t i=0;i<candidates.size()&&selected.size()<doom_decor::emitterLimit;++i)
            selected.push_back(candidates[i].second);
    };
    selectClosest(closest);
    if(selected.size()<doom_decor::emitterLimit)selectClosest(detailOnly);
}

void queueDecorationCritter(const mobj_t &thing,const DecorationArt &art,const Image &image,
                            float x,float y,float top,float rx,float ry,float roll,float pivotDown,bool flip) {
    if(settings.decorationInsects<2||!thing.subsector||decorationCritterCount>=decorationCritters.size()||
       std::find(selected.begin(),selected.end(),&thing)==selected.end())return;
    const auto found=known.find(&thing);
    if(found==known.end()||found->second.type!=thing.type)return;
    float age;int event;
    if(!detailAge(found->second,settings.decorationDelay,renderFraction,age,event))return;
    const bool spider=horrorDecor(thing.type)==HorrorDecor::SkullStake;
    const bool detaches=!spider&&doom_decor::hash(found->second.seed^uint32_t(event)^0x1fd9u)%3u==0;
    if(detaches&&age>3.f)
        roll=decorationRoll(thing,art,renderFraction-(age-3.f)*TICRATE,rx,ry,top,units(thing.ceilingz),pivotDown);
    float across=(flip?image.width-art.detailX-.5f:art.detailX+.5f)-image.left;
    float down=art.detailY+.5f;
    const float pivot=decorationPivot(art,image,flip);
    doom_decor::SpriteRoll{pivot,pivotDown,std::sin(roll),std::cos(roll)}.apply(across,down);
    const float z=top-down;
    const float reach=std::min(spider?13.f:23.f,z-units(thing.floorz)-1.f);
    if(reach<2||z>units(thing.ceilingz))return;
    const float light=std::clamp(lighting(thing.subsector->sector),.12f,1.f);
    decorationCritters[decorationCritterCount++]={x+rx*across+ry*.6f,y+ry*across-rx*.6f,z,reach,age,light,spider,detaches};
}

void drawDecorationCritters(const Uniforms &camera) {
    unsigned larvae=0,spiders=0;
    auto block=[&](float x,float y,float z,float w,float h,const std::array<float,3> &color,float alpha) {
        pixelBlock(particleVertices,camera,x,y,z,w,h/w,color,false,nullptr,true,nullptr,alpha);
    };
    for(size_t i=0;i<decorationCritterCount;++i) {
        const auto &c=decorationCritters[i];
        if(c.spider) {
            if(spiders>=doom_decor::maxSpiders||!reserveCritter(9))continue;
            ++spiders;
            const float descent=c.reach*std::clamp(std::min(c.age/2.f,(5.4f-c.age)/2.4f),0.f,1.f);
            const float z=c.z-descent;
            const std::array<float,3> body={.08f+.10f*c.light,.047f+.06f*c.light,.035f+.046f*c.light};
            if(descent>.15f) {
                const float sx=camera.right[0]*.13f,sy=camera.right[1]*.13f;
                Vertex a={c.x-sx,c.y-sy,c.z,0,0,.62f,0};
                a.red=.32f*c.light;a.green=.28f*c.light;a.blue=.24f*c.light;
                Vertex b=a,d=a,e=a;
                b.x+=2*sx;b.y+=2*sy;d=b;d.z=z;e.z=z;
                quad(particleVertices,a,b,d,e);
            }
            block(c.x,c.y,z,1.1f,.85f,body,.95f);
            for(int side:{-1,1})for(int leg=0;leg<3;++leg) {
                const float offset=side*(.66f+.2f*leg);
                const float step=.12f*std::sin(c.age*8.f+leg+side);
                block(c.x+camera.right[0]*offset,c.y+camera.right[1]*offset,
                      z+.24f-.24f*leg+step,.44f,.25f,body,.9f);
            }
            block(c.x,c.y,z-.43f,.66f,.48f,body,.95f);
        } else {
            const float fall=c.detaches?std::max(0.f,c.age-3.f):0.f;
            const float drop=260.f*fall*fall;
            if(drop>=c.reach||larvae>=doom_decor::maxLarvae||!reserveCritter(3*wormBlocksPerSegment))continue;
            ++larvae;
            const std::array<float,3> pale={.53f+.31f*c.light,.44f+.28f*c.light,.28f+.22f*c.light};
            const float alpha=.92f*std::clamp(std::min(c.age*5.f,(3.4f-c.age)*5.f),0.f,1.f);
            for(int segment=0;segment<3;++segment) {
                const float phase=c.age*4.f-segment*.9f;
                const float offset=.42f*std::sin(phase)+.28f*std::sin(c.age*1.15f);
                const float z=c.z-drop-segment*.45f+.15f*std::sin(phase);
                roundedWormSegment(camera,c.x+camera.right[0]*offset,c.y+camera.right[1]*offset,
                                   z,.8f-.09f*segment,.64f,pale,alpha);
            }
        }
    }
}

void decorationAmbienceThing(const mobj_t *thing,const Uniforms &camera) {
    // 'thing' is supplied by the *current* geometry iteration. Cached keys
    // are compared only; stale pointers are never dereferenced.
    if(selected.empty()||!thing||
       std::find(selected.begin(),selected.end(),thing)==selected.end())return;
    auto found=known.find(thing);if(found==known.end())return;
    const Emitter &e=found->second;
    if(bodyKind(*thing)!=e.kind||!settled(*thing,e.kind))return;
    // Stage 2 is already dry enough to stop sustaining flies, worms and the
    // exotic corpse critters. No gameplay entities or GPU resources are spawned.
    if(e.kind==BodyKind::Corpse && corpseDecayStage(thing)>=2)return;
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

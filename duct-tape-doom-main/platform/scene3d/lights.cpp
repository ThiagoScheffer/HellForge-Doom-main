/* Dynamic lights collected each frame: muzzle flashes, projectiles,
   decorations, barrels and door spill, packed for the GPU. */
#include "internal.h"
#include "frame_workers.h"
extern "C" {
#include "p_local.h"
}

namespace doom3d {
std::array<const mobj_t*,maxLights> lightSources={};
#ifdef DOOM_TEST_DRIVER
extern "C" int I_TestKeyLight(mobj_t* thing){return std::find(lightSources.begin(),lightSources.end(),thing)!=lightSources.end();}
#endif

std::array<bool,maxLights> fogOnly={};

std::array<uint8_t,maxLights> lightKinds={};
uint8_t lightKind=0;
struct FlashPulse { float x,y,z,radius,strength; int tic; const mobj_t *source; std::array<float,3> color; };
static std::vector<FlashPulse> flashPulses;
static unsigned flashLevelSerial;
static std::unordered_map<unsigned,std::array<float,3>> glowColors;
static std::array<float,3> frameGlow(int sprite,int frame) {
    frame&=FF_FRAMEMASK;
    if(sprite<0||sprite>=numsprites||frame>=sprites[sprite].numframes) return {1,1,1};
    unsigned key=(unsigned(sprite)<<16)|unsigned(frame);
    auto cached=glowColors.find(key);if(cached!=glowColors.end()) return cached->second;
    const auto &sf=sprites[sprite].spriteframes[frame];
    std::array<float,3> color={1,1,1};float weight=-1;
    for(int n=0;n<(sf.rotate?8:1);++n) {
        const Image &image=lumpImage(firstspritelump+sf.lump[n],false);
        if(image.glowWeight>weight) {weight=image.glowWeight;color=image.glow;}
    }
    glowColors.emplace(key,color);return color;
}
void muzzleFlash(mobj_t *source,int weapon,int projectile) {
    if(!source||gamestate!=GS_LEVEL) return;
    if(flashLevelSerial!=r_levelserial) {flashPulses.clear();flashLevelSerial=r_levelserial;}
    float radius=384,strength=1.9f;int flashState=-1;
    Game_MuzzleFlash(weapon,&radius,&strength,&flashState);
    float z=source->player?units(source->player->viewz)-8:units(source->z)+units(source->height)*0.65f;
    int sprite=source->sprite,frame=source->frame;
    if(projectile>=0&&projectile<NUMMOBJTYPES) {
        const auto &state=states[mobjinfo[projectile].spawnstate];sprite=state.sprite;frame=state.frame;
    } else if(source->player&&flashState>=0) {
        const auto &state=states[flashState];sprite=state.sprite;frame=state.frame;
    }
    FlashPulse pulse={units(source->x),units(source->y),z,radius,strength,leveltime,source,frameGlow(sprite,frame)};
    for(auto &old:flashPulses) if(old.source==source) {old=pulse;return;}
    if(flashPulses.size()==8) flashPulses.erase(flashPulses.begin());
    flashPulses.push_back(pulse);
}


// The map's lines by 128-unit cell, built once a level (lines don't move), so
// a light asks only the lines near it instead of the whole map.
std::vector<int> linesAround(float x,float y,float reach) {
    constexpr float cell=128;
    static std::vector<std::vector<int>> cells;
    static std::vector<unsigned> seen;
    static unsigned gridLevel=~0u,stamp=0;
    static float originX,originY;
    static int across,down;
    if(gridLevel!=r_levelserial) {
        gridLevel=r_levelserial;
        float minX=1e30f,minY=1e30f,maxX=-1e30f,maxY=-1e30f;
        for(int i=0;i<numvertexes;++i) {
            float vx=units(vertexes[i].x),vy=units(vertexes[i].y);
            minX=std::min(minX,vx);minY=std::min(minY,vy);maxX=std::max(maxX,vx);maxY=std::max(maxY,vy);
        }
        if(numvertexes==0)minX=minY=maxX=maxY=0;
        originX=minX;originY=minY;
        across=(int)((maxX-minX)/cell)+1;down=(int)((maxY-minY)/cell)+1;
        cells.assign((size_t)across*down,{});seen.assign(numlines,0);stamp=0;
        for(int i=0;i<numlines;++i) {
            float ax=units(lines[i].v1->x),ay=units(lines[i].v1->y),bx=units(lines[i].v2->x),by=units(lines[i].v2->y);
            int x0=(int)((std::min(ax,bx)-originX)/cell),x1=(int)((std::max(ax,bx)-originX)/cell);
            int y0=(int)((std::min(ay,by)-originY)/cell),y1=(int)((std::max(ay,by)-originY)/cell);
            for(int cy=y0;cy<=y1;++cy)for(int cx=x0;cx<=x1;++cx)cells[(size_t)cy*across+cx].push_back(i);
        }
    }
    std::vector<int> near;
    if(++stamp==0) {std::fill(seen.begin(),seen.end(),0);stamp=1;}
    int x0=std::max(0,(int)std::floor((x-reach-originX)/cell)),x1=std::min(across-1,(int)std::floor((x+reach-originX)/cell));
    int y0=std::max(0,(int)std::floor((y-reach-originY)/cell)),y1=std::min(down-1,(int)std::floor((y+reach-originY)/cell));
    for(int cy=y0;cy<=y1;++cy)for(int cx=x0;cx<=x1;++cx)
        for(int i:cells[(size_t)cy*across+cx])if(seen[i]!=stamp) {seen[i]=stamp;near.push_back(i);}
    std::sort(near.begin(),near.end());   // in map order, as the whole-map scan met them
    return near;
}

// A short flash where a shot sparks off machinery or breaks a lamp
// (shot_lights.cpp); a new one replaces the last.
void sparkFlash(float x,float y,float z,float radius,float strength,const std::array<float,3> &color) {
    if(gamestate!=GS_LEVEL) return;
    if(flashLevelSerial!=r_levelserial) {flashPulses.clear();flashLevelSerial=r_levelserial;}
    FlashPulse pulse={x,y,z,radius,strength,leveltime,nullptr,color};
    for(auto &old:flashPulses) if(!old.source) {old=pulse;return;}
    if(flashPulses.size()==8) flashPulses.erase(flashPulses.begin());
    flashPulses.push_back(pulse);
}


// Shadow walls kept between frames. A light's walls (relevant and not hidden)
// depend only on its position, reach and cone and on the heights and skies of
// the sectors along the lines around it, so a fixed light's list stays good
// until one of those sectors changes. With it the GPU words built from the
// list (buildLightWords). When full, the least recently used entry goes, so
// passing projectiles never push out the lamps that shine every frame.
struct BlockerKey {
    uint32_t bits[8];
    bool operator==(const BlockerKey &other) const {return !memcmp(bits,other.bits,sizeof(bits));}
};
struct BlockerKeyHash {
    size_t operator()(const BlockerKey &key) const {
        size_t hash=0;
        for(uint32_t bits:key.bits)hash=(hash^bits)*0x100000001b3ull;
        return hash;
    }
};
struct CachedBlockers {
    std::vector<LightBlocker> blockers;
    std::vector<int> sectors; // Whose heights and skies decided them.
    uint64_t built=0,used=0;
    std::vector<std::array<uint32_t,4>> words; // buildLightWords' copy, with sliced and sliceCost.
    bool wordsReady=false,sliced=false;
    float sliceCost=0;
};
static constexpr size_t blockerCacheSize=256;
static std::unordered_map<BlockerKey,CachedBlockers,BlockerKeyHash> blockerCache;
static std::array<CachedBlockers*,maxLights> lightCache={}; // Each of this frame's lights' entry, or none.
static std::vector<std::array<float,3>> sectorSeen; // Floor, ceiling, sky.
static std::vector<uint64_t> sectorChanged;
static uint64_t lightFrame;
static unsigned blockerLevel;
// Once per frame, before lights are added: which sectors moved or changed sky.
static void noteSectorChanges() {
    ++lightFrame;lightCache={};
    if(blockerLevel!=r_levelserial||sectorSeen.size()!=(size_t)numsectors) {
        blockerCache.clear();blockerLevel=r_levelserial;
        sectorSeen.assign(numsectors,{NAN,NAN,NAN});sectorChanged.assign(numsectors,0);
    }
    for(int i=0;i<numsectors;++i) {
        std::array<float,3> now={floorZ(&sectors[i]),ceilZ(&sectors[i]),sectors[i].ceilingpic==skyflatnum?1.0f:0.0f};
        if(now!=sectorSeen[i]) {sectorSeen[i]=now;sectorChanged[i]=lightFrame;}
    }
}
static bool blockersCurrent(const CachedBlockers &entry) {
    for(int sector:entry.sectors) if(sectorChanged[sector]>entry.built) return false;
    return true;
}
// A slot for a new entry: the stale one under key, a free one, or the least
// recently used one not in use this frame; none when every entry is.
static CachedBlockers *blockerSlot(const BlockerKey &key) {
    auto found=blockerCache.find(key);
    if(found!=blockerCache.end()) return &found->second;
    if(blockerCache.size()>=blockerCacheSize) {
        auto oldest=blockerCache.end();
        for(auto it=blockerCache.begin();it!=blockerCache.end();++it)
            if(it->second.used<lightFrame&&(oldest==blockerCache.end()||it->second.used<oldest->second.used)) oldest=it;
        if(oldest==blockerCache.end()) return nullptr;
        blockerCache.erase(oldest);
    }
    return &blockerCache[key];
}


// Directionality: 0 lights every surface alike, 1 is full Lambert facing.
// Unoccluded lights skip wall blockers, so shading them costs no ray tests.
// baked: the part of this light already in the bake maps, which world surfaces
// subtract so they only gain its flicker and detail (0 for dynamic-only lights).
// spot: a spotlight's unit axis and cone cosine (direction); without a cone,
// x is a glowing panel's half-height, which softens its wall shadows, and a
// negative w is minus a lamp's radius (lampSpot), which softens all of them.
void appendLight(float x,float y,float z,float radius,float strength,const std::array<float,3> &color,const mobj_t *source,float directionality,bool occluded,float baked,
                 const std::array<float,4> &spot) {
    if(flashes.count>=maxLights) return;
    ProfileTimer timer{profile.frame.lightsMs};
    lightSources[flashes.count]=source;fogOnly[flashes.count]=false;lightKinds[flashes.count]=lightKind;
    Flash &f=flashes.lights[flashes.count++];
    f={{x,y,z,radius},strength,(unsigned)lightBlockers.size(),0,baked,{color[0],color[1],color[2],directionality},{spot[0],spot[1],spot[2],spot[3]}};
    if(!occluded) return;
    if(float lamp=doom_flash_lamp(&f);lamp>0) {
        // The lamp's points keep clear of the floor and ceiling and of every
        // wall that can stop a ray, or rays from them would start inside one.
        // They lie up to lamp*0.433 above or below the center and lamp*0.612 across.
        const sector_t *own=R_PointInSubsector((fixed_t)(x*FRACUNIT),(fixed_t)(y*FRACUNIT))->sector;
        lamp=std::min(lamp,(std::min(z-floorZ(own),ceilZ(own)-z)-1)/0.433f);
        for(int i:linesAround(x,y,lamp*0.6124f+1)) {
            if(lamp<=0) break;
            const line_t &line=lines[i];
            float ax=units(line.v1->x),ay=units(line.v1->y),bx=units(line.v2->x),by=units(line.v2->y),reach=lamp*0.6124f+1;
            if(std::max(ax,bx)<x-reach||std::min(ax,bx)>x+reach||std::max(ay,by)<y-reach||std::min(ay,by)>y+reach) continue;
            if(line.frontsector&&line.backsector) {
                if(line.frontsector==line.backsector) continue;
                float bottom=std::max(floorZ(line.frontsector),floorZ(line.backsector)),top=std::min(ceilZ(line.frontsector),ceilZ(line.backsector));
                if(bottom<z-lamp-1&&top>z+lamp+1) continue;
            }
            LightBlocker segment={{ax,ay,bx,by},{0,0,0,0}};
            lamp=std::min(lamp,(doom_flash_segment_distance(x,y,&segment)-1)/0.6124f);
        }
        f.direction[3]=lamp>=1?-lamp:0;
    }
    BlockerKey key;
    memcpy(key.bits,f.position,sizeof(f.position));memcpy(key.bits+4,f.direction,sizeof(f.direction));
    auto found=blockerCache.find(key);
    if(found!=blockerCache.end()&&blockersCurrent(found->second)) {
        CachedBlockers &entry=found->second;
        lightBlockers.insert(lightBlockers.end(),entry.blockers.begin(),entry.blockers.end());
        f.count=(uint32_t)entry.blockers.size();
        entry.used=lightFrame;lightCache[flashes.count-1]=&entry;
        return;
    }
    std::vector<int> around;
    for(int i:linesAround(x,y,radius)) {
        const line_t &line=lines[i];
        float ax=units(line.v1->x),ay=units(line.v1->y),bx=units(line.v2->x),by=units(line.v2->y);
        if(std::max(ax,bx)<x-radius||std::min(ax,bx)>x+radius||
           std::max(ay,by)<y-radius||std::min(ay,by)>y+radius) continue;
        float bottom=1,top=-1;
        if(line.frontsector&&line.backsector) {
            bottom=std::max(floorZ(line.frontsector),floorZ(line.backsector));
            top=std::min(ceilZ(line.frontsector),ceilZ(line.backsector));
            if(line.frontsector->ceilingpic==skyflatnum&&line.backsector->ceilingpic==skyflatnum) top=32768;
            if(line.frontsector==line.backsector) continue;
            around.push_back((int)(line.frontsector-sectors));around.push_back((int)(line.backsector-sectors));
        }
        LightBlocker blocker={{ax,ay,bx,by},{bottom,top,0,0}};
        if(!doom_flash_blocker_relevant(&f,&blocker,!line.backsector)) continue;
        lightBlockers.push_back(blocker);
    }
    f.count=doom_flash_cull_hidden(&f,lightBlockers.data()+f.first,(uint32_t)lightBlockers.size()-f.first);
    lightBlockers.resize(f.first+f.count);
    if(CachedBlockers *entry=blockerSlot(key)) {
        std::sort(around.begin(),around.end());around.erase(std::unique(around.begin(),around.end()),around.end());
        entry->blockers.assign(lightBlockers.begin()+f.first,lightBlockers.end());
        entry->sectors=std::move(around);
        entry->built=entry->used=lightFrame;entry->wordsReady=false;
        lightCache[flashes.count-1]=entry;
    }
}
static void doorSpill(const Uniforms &camera);

// Exploding barrels: the light builds through the fireball and peaks on the
// blast tic (A_Explode), then fades; the blast itself also leaves sparks and
// a floor scorch, once per barrel.
static std::unordered_map<const mobj_t*,bool> burstBarrels;
static float barrelLight(const mobj_t &thing) {
    int frame=Game_BlastFrame(&thing);
    if(frame<0)return 0;
    static const float curve[GAME_BLAST_FRAMES]={0.7f,1.2f,1.8f,2.8f,1.5f};
    float strength=curve[frame];
    if(frame==GAME_BLAST_FRAMES-1)strength*=std::clamp(thing.tics/10.0f,0.0f,1.0f);
    return strength;
}
// Teleport fog (and the item respawn fog): a burst that peaks as it appears
// and fades over the rest of its frames, timed from the state chain so
// DEHACKED changes keep it in step. 0 for every other thing.
static float teleportLight(const mobj_t &thing) {
    if(Game_ThingRole(thing.type)!=GAME_THING_TELEPORT_FOG||!thing.state)return 0;
    int total=0,elapsed=-1,current=int(thing.state-states);
    for(int state=thing.info->spawnstate,n=0;state!=S_NULL&&n<32;state=states[state].nextstate,++n) {
        if(state==current)elapsed=total+std::max(0,(int)(states[state].tics-thing.tics));
        total+=std::max(0,(int)states[state].tics);
    }
    if(elapsed<0||total<=0)return 0;
    float t=elapsed/(float)total;
    return 1.8f*std::clamp((1-t)/0.7f,0.0f,1.0f);
}
// Decoration lights: things whose spawn frame is fullbright by design
// (torches, candles, lamps, burning barrels), recognized from thing data
// rather than names, and those the game names (Game_DecorationLight:
// Heretic's chandeliers and serpent torches). Monsters, shots, pickups and
// short-lived fogs and puffs are excluded. Animated flames flicker like DOOM's fire-flicker sectors:
// a stepped drop every 4 tics, from a hash rather than the game's RNG.
bool decorationLight(const mobj_t &thing) {
    if(thing.player||(thing.flags&(MF_MISSILE|MF_COUNTKILL|MF_SPECIAL|MF_NOBLOCKMAP)))return false;
    if(Game_DecorationLight(thing.sprite))return true;
    const state_t &spawn=states[thing.info->spawnstate];
    return (spawn.frame&FF_FULLBRIGHT)&&(thing.frame&FF_FULLBRIGHT);
}
// The flicker hashes finish with MurmurHash2's mixing step (Austin Appleby, public domain).
static float decorationFlicker(const mobj_t &thing) {
    const state_t &spawn=states[thing.info->spawnstate];
    if(spawn.tics<0||spawn.nextstate==thing.info->spawnstate)return 1;
    uint32_t hash=(uint32_t)(uintptr_t)&thing*2654435761u^(uint32_t)(leveltime/4)*40503u;
    hash^=hash>>13;hash*=0x5bd1e995u;hash^=hash>>15;
    return 1-0.12f*(hash&3);
}
// Bake-only lights flicker by group: each animated decoration falls in one
// of the groups switching lamps leave (up to 15) that step like
// decorationFlicker; group 0 stays steady. Lamp groups switch in every mode,
// decoration groups only with bake-only lights (their dynamic copies flicker
// them otherwise).
int flickerGroup(const mobj_t &thing) {
    const state_t &spawn=states[thing.info->spawnstate];
    int free=flickerGroups-1-lampGroupCount;
    if(spawn.tics<0||spawn.nextstate==thing.info->spawnstate||free<=0)return 0;
    uint32_t hash=(uint32_t)(uintptr_t)&thing*2654435761u;
    return 1+lampGroupCount+(int)((hash>>16)%free);
}
float groupFlicker(int group) {
    if(group<=0||group>=flickerGroups)return 1;
    if(group<=lampGroupCount)return lampGroupFlicker[group];
    if(!bakeOnlyActive)return 1;
    uint32_t hash=(uint32_t)group*2654435761u^(uint32_t)(leveltime/4)*40503u;
    hash^=hash>>13;hash*=0x5bd1e995u;hash^=hash>>15;
    return 1-0.12f*(hash&3);
}
// A decoration's unflickered light, shared by the dynamic set and the bake.
// Size follows the artwork: a candle glows small, a tall torch wide. It
// shines near the top, or, for art that isn't fullbright, from where its
// flames are painted (a chandelier's candles sit low under the chain).
BakeLight decorationSource(const mobj_t &thing) {
    const auto &frame=sprites[thing.sprite].spriteframes[thing.frame&FF_FRAMEMASK];
    const Image &image=lumpImage(firstspritelump+frame.lump[0],false);
    float height=(float)image.height,up=Game_DecorationLight(thing.sprite)?image.glowHeight:0.85f;
    auto color=frameGlow(thing.sprite,thing.frame);
    return {units(thing.x),units(thing.y),units(thing.z)+height*up,std::clamp(height*4.0f,96.0f,320.0f),
            std::clamp(height/60.0f,0.6f,1.2f),0.75f,{color[0],color[1],color[2]}};
}
std::vector<StaticFog> staticFog;
// The carried torch's color: the glow of the game's torch item sprite
// (Game_Sprite), paled halfway to white like firelight on stone, or a
// candle's warm white.
std::array<float,3> torchColor() {
    int sprite=Game_Sprite(GAME_SPRITE_TORCH);
    if(sprite<0)return {1.0f,0.75f,0.45f};
    auto glow=frameGlow(sprite,0);
    for(float &c:glow)c=0.5f+0.5f*c;
    return glow;
}
FlashlightBeam computeFlashlightBeam(const Uniforms &camera,float bob,float time,bool classic,bool simulation) {
    FlashlightBeam beam;
        float lamp[3],aim[3],length=0,forward[3],right[3],up[3];
        auto unit=[](const float *v,float *out){float l=std::sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);for(int c=0;c<3;++c)out[c]=l>0?v[c]/l:0;};
        unit(camera.forward,forward);unit(camera.right,right);unit(camera.up,up);
        for(int c=0;c<3;++c) lamp[c]=camera.eye[c]+right[c]*16-up[c]*6-forward[c]*20;
        // Like Doom 3's, the lamp backs out of walls: hugging a wall on the
        // right (the player's radius is the lamp's offset) or facing into a
        // corner put it on or behind the wall, which then blocked the beam.
        float dx=lamp[0]-camera.eye[0],dy=lamp[1]-camera.eye[1],reach=std::sqrt(dx*dx+dy*dy),clear=1;
        for(int i:reach>0?linesAround(camera.eye[0]+dx*0.5f,camera.eye[1]+dy*0.5f,reach*0.5f+5):std::vector<int>{}) {
            const line_t &line=lines[i];
            if(line.frontsector&&line.backsector&&line.frontsector!=line.backsector&&
               lamp[2]>std::max(simulation?units(line.frontsector->floorheight):floorZ(line.frontsector),simulation?units(line.backsector->floorheight):floorZ(line.backsector))&&
               lamp[2]<std::min(simulation?units(line.frontsector->ceilingheight):ceilZ(line.frontsector),simulation?units(line.backsector->ceilingheight):ceilZ(line.backsector))) continue;
            float ax=units(line.v1->x),ay=units(line.v1->y),ex=units(line.v2->x)-ax,ey=units(line.v2->y)-ay;
            float denominator=dx*ey-dy*ex;if(std::abs(denominator)<1e-6f)continue;
            float t=((ax-camera.eye[0])*ey-(ay-camera.eye[1])*ex)/denominator;
            float u=((ax-camera.eye[0])*dy-(ay-camera.eye[1])*dx)/denominator;
            if(t>=0&&u>=0&&u<=1)clear=std::min(clear,std::max(0.0f,t-4/reach));
        }
        lamp[0]=camera.eye[0]+dx*clear;lamp[1]=camera.eye[1]+dy*clear;
        // The beam sways with the gun as Doom bobs it (A_WeaponReady): side to
        // side over 64 tics and dipping twice a cycle, by how fast the player
        // moves, plus a slow drift at rest, so it feels carried rather than
        // glued to the view.
        bob=std::clamp(bob,0.f,1.f);
        float phase=time*(2*doomPi/64),seconds=time/TICRATE;
        float yaw=(std::cos(phase)*bob*1.2f+std::sin(seconds*(2*doomPi/4.3f))*0.2f)*(doomPi/180);
        float pitch=(-std::abs(std::sin(phase))*bob*0.8f+std::sin(seconds*(2*doomPi/3.1f))*0.15f)*(doomPi/180);
        for(int c=0;c<3;++c) {
            aim[c]=camera.eye[c]+(forward[c]+right[c]*std::tan(yaw)+up[c]*std::tan(pitch))*256-lamp[c];
            length+=aim[c]*aim[c];
        }
        length=std::sqrt(length);
    if(!(length>0.001f)||!std::isfinite(length))return beam;
    for(int c=0;c<3;++c){beam.origin[c]=lamp[c];beam.direction[c]=aim[c]/length;}
    beam.radius=classic?640.f:900.f;beam.strength=classic?1.1f:1.9f;beam.cone=.90f;beam.valid=true;
    return beam;
}
void collectFlashes(float fraction,const Uniforms &camera) {
    noteSectorChanges();
    flashes={};lightBlockers.clear();fogOnly={};lightKind=I_RENDER3D_LIGHT_FLASHLIGHT;
    // A carried torch (Game_Torch): a flame held low at the left hand,
    // lighting all around like the torches on the walls, flickering with
    // the game's own torch and a fine stepped flicker of its own. Things
    // throw shadows from it (decoration kind, not the flashlight's).
    float flame;
    if(Game_Torch(&flame)&&flame>0) {
        float at[3];
        for(int c=0;c<3;++c)at[c]=camera.eye[c]-camera.right[c]*12-camera.up[c]*10+camera.forward[c]*8;
        uint32_t hash=(uint32_t)(leveltime/3)*2654435761u;hash^=hash>>13;hash*=0x5bd1e995u;hash^=hash>>15;
        lightKind=I_RENDER3D_LIGHT_DECORATION;
        appendLight(at[0],at[1],at[2],640,1.0f*(0.7f+0.3f*flame)*(1-0.06f*(hash&3)),torchColor(),nullptr,0.75f);
        lightKind=I_RENDER3D_LIGHT_FLASHLIGHT;
    }
    // The flashlight rides at the right shoulder, a little below, right of and
    // behind the eye like Doom 3's, aimed to meet the view 256 units ahead.
    // High up it lights the floor ahead from above rather than skimming it,
    // and from behind the eye it reaches the floor just ahead; a light at the
    // eye would hide every shadow it casts behind its caster, and from the
    // side walls, door frames and things throw visible shadows (see
    // flashlightSilhouette).
    if(flashlightOn) {
        bool classic=settings.classicFlashlight;
        const auto beam=computeFlashlightBeam(camera,players[displayplayer].bob/16.0f/FRACUNIT,leveltime+fraction,classic);
        if(beam.valid)appendLight(beam.origin[0],beam.origin[1],beam.origin[2],beam.radius,beam.strength,
                    classic?std::array<float,3>{1.0f,0.86f,0.62f}:std::array<float,3>{1.0f,0.96f,0.86f},nullptr,0.85f,true,0,
                    {beam.direction[0],beam.direction[1],beam.direction[2],beam.cone});
        // Its bounce off what it lights (not for the classic beam, which
        // stays Doom's extralight); soft, and only the flashlight kind so
        // things cast no shadows from it (dynamicThingLight).
        float position[3],normal[2],strength;std::array<float,3> color;
        if(!classic&&flashlightBounce(position,normal,color,strength)) {
            appendLight(position[0],position[1],position[2],256,strength,color,nullptr,0.5f,true,0,{0,normal[0],normal[1],0});
        }
    }
    if(flashLevelSerial!=r_levelserial) {flashPulses.clear();flashLevelSerial=r_levelserial;}
    flashPulses.erase(std::remove_if(flashPulses.begin(),flashPulses.end(),[](const FlashPulse &p) {
        return leveltime<p.tic||leveltime-p.tic>=6;
    }),flashPulses.end());
    lightKind=I_RENDER3D_LIGHT_SHOT;
    for(const auto &p:flashPulses) {
        float strength=p.strength*doom_flash_fade(leveltime-p.tic,fraction);
        if(strength>0) appendLight(p.x,p.y,p.z,p.radius,strength,p.color,p.source,0.75f);
    }
    if(settings.emissive)collectHellfireLights(camera);
    // Live WAD missile objects supply continuous light, including their bright
    // impact frames. Keep the nearest eight so rapid fire has a bounded cost.
    struct Candidate {mobj_t *thing;float distance;};
    std::vector<Candidate> projectiles,barrels,decorations,teleports,keys;
    for(auto &entry:burstBarrels)entry.second=false;
    for(int i=0;i<numsectors;++i) for(mobj_t *thing=sectors[i].thinglist;thing;thing=thing->snext) {
        if(barrelLight(*thing)>0) {
            float x,y,z;interpolatedPosition(*thing,fraction,x,y,z);
            float dx=x-camera.eye[0],dy=y-camera.eye[1],dz=z-camera.eye[2];
            barrels.push_back({thing,dx*dx+dy*dy+dz*dz});
            auto seen=burstBarrels.find(thing);
            if(seen!=burstBarrels.end())seen->second=true;
            else if(Game_BlastFrame(thing)>=GAME_BLAST_BURST) {burstBarrels[thing]=true;barrelBlast(*thing,x,y,z);}
            continue;
        }
        if(teleportLight(*thing)>0) {
            float dx=units(thing->x)-camera.eye[0],dy=units(thing->y)-camera.eye[1];
            float distance=std::hypot(dx,dy);
            if(distance<1400)teleports.push_back({thing,distance});
            continue;
        }
        if((thing->flags&MF_SPECIAL)&&glowingPickup(thing->sprite)) {
            float distance=std::hypot(units(thing->x)-camera.eye[0],units(thing->y)-camera.eye[1]);
            if(distance<640)keys.push_back({thing,distance});
            continue;
        }
        if(decorationLight(*thing)) {
            float dx=units(thing->x)-camera.eye[0],dy=units(thing->y)-camera.eye[1];
            float distance=std::hypot(dx,dy);
            if(distance<1024)decorations.push_back({thing,distance});
            continue;
        }
        if(!(thing->info->flags&MF_MISSILE)||!(thing->frame&FF_FULLBRIGHT)) continue;
        if(liquidProjectileHidden(thing))continue;
        // Enhanced monster shots only get custom light near the player and
        // when their shooter is actually targeting a player (not infighting).
        if(monsterProjectileFxManaged(thing->type)&&!monsterProjectileFxEligible(*thing))continue;
        float x,y,z;interpolatedPosition(*thing,fraction,x,y,z);
        float dx=x-camera.eye[0],dy=y-camera.eye[1],dz=z-camera.eye[2];
        projectiles.push_back({thing,dx*dx+dy*dy+dz*dz});
    }
    for(auto it=burstBarrels.begin();it!=burstBarrels.end();) it=it->second?std::next(it):burstBarrels.erase(it);
    std::sort(barrels.begin(),barrels.end(),[](const Candidate &a,const Candidate &b) {return a.distance<b.distance;});
    lightKind=I_RENDER3D_LIGHT_BARREL;
    for(size_t n=0;n<std::min(size_t(4),barrels.size());++n) {
        const mobj_t &thing=*barrels[n].thing;
        float x,y,z;interpolatedPosition(thing,fraction,x,y,z);
        appendLight(x,y,z+24,448,barrelLight(thing),frameGlow(thing.sprite,thing.frame),&thing,0.75f);
    }
    std::sort(decorations.begin(),decorations.end(),[](const Candidate &a,const Candidate &b) {return a.distance<b.distance;});
    staticFog.clear();lightKind=I_RENDER3D_LIGHT_DECORATION;
    for(size_t n=0;n<std::min(size_t(10),decorations.size());++n) {
        const mobj_t &thing=*decorations[n].thing;
        BakeLight light=decorationSource(thing);
        // Bake-only: the bake lights everything; only the fog glow remains.
        if(bakeOnlyActive) {
            float strength=light.strength*groupFlicker(flickerGroup(thing));
            staticFog.push_back({light.x,light.y,light.z,light.radius,strength,{light.color[0],light.color[1],light.color[2]}});
            continue;
        }
        float flicker=decorationFlicker(thing);
        float strength=light.strength*flicker*std::clamp((1024-decorations[n].distance)/256,0.0f,1.0f);
        appendLight(light.x,light.y,light.z,light.radius,strength,{light.color[0],light.color[1],light.color[2]},&thing,0.75f,
                    true,bakedLightsActive?1/flicker:0);
    }
    // Pickups stay dynamic even in bake-only mode: no lingering light after collection.
    std::sort(keys.begin(),keys.end(),[](const Candidate&a,const Candidate&b){return a.distance<b.distance;});
    auto isKeyPickup=[](int sprite){
        return sprite==SPR_RKEY||sprite==SPR_RSKU||sprite==SPR_BKEY||sprite==SPR_BSKU||
               sprite==SPR_YKEY||sprite==SPR_YSKU;
    };
    lightKind=I_RENDER3D_LIGHT_DECORATION;
    for(size_t n=0;n<std::min(size_t(6),keys.size());++n){
        const auto& thing=*keys[n].thing;float x,y,z;interpolatedPosition(thing,fraction,x,y,z);
        std::array<float,3> color={1,.72f,.04f};
        switch(thing.sprite){
        case SPR_RKEY:case SPR_RSKU:case SPR_PSTR:color={1,.06f,.025f};break;
        case SPR_BKEY:case SPR_BSKU:case SPR_ARM2:case SPR_SOUL:color={.08f,.22f,1};break;
        case SPR_ARM1:case SPR_SUIT:case SPR_PVIS:color={.12f,1,.18f};break;
        case SPR_MEGA:color={1,.65f,.16f};break;
        case SPR_PINV:color={.45f,1,.2f};break;
        case SPR_PINS:color={.7f,.18f,1};break;
        case SPR_PMAP:color={.12f,.65f,1};break;
        default:break;
        }
        bool key=isKeyPickup(thing.sprite);
        // Keys mark their location without washing nearby walls or floors in
        // saturated color. Keep the same key hue and sprite treatment, but use
        // less light than the original 1.5-strength, 144-unit emitters.
        float strength=.65f;
        // Nearby keys also share a light budget; non-key pickups keep their
        // previous normalization against all other glowing pickups.
        int neighbors=0;
        for(const auto& other:keys)if(other.thing!=&thing&&(!key||isKeyPickup(other.thing->sprite))&&
            std::hypot(units(other.thing->x)-x,units(other.thing->y)-y)<160)++neighbors;
        strength/=std::sqrt(1.f+neighbors*.5f);
        // Raise the emitter into the pickup so the floor still catches a small,
        // local pool. Negative cone marker keeps the existing pickup surface tint.
        appendLight(x,y,z+(key?16:24),key?104:112,strength*std::clamp((640-keys[n].distance)/128,0.f,1.f),color,&thing,.35f,true,0,{0,0,0,-1});
    }
    std::sort(projectiles.begin(),projectiles.end(),[](const Candidate &a,const Candidate &b) {return a.distance<b.distance;});
    lightKind=I_RENDER3D_LIGHT_PROJECTILE;
    // Swarms (Heretic's teleport glitter, claw rippers, crossbow volleys,
    // flame trails) would stack a full light per spark and wash surfaces
    // out: shots within 96 units of a nearer one join its light, which
    // grows by a fifth per member up to half again as strong. Sparks
    // smaller than DOOM's plasma ball (13 pixels) shine by their area.
    struct Cluster {const mobj_t *thing;float x,y,z,radius,strength;int members;std::array<float,3> color;bool enhanced;};
    std::vector<Cluster> clusters;
    for(const Candidate &candidate:projectiles) {
        const mobj_t &thing=*candidate.thing;
        float x,y,z;interpolatedPosition(thing,fraction,x,y,z);z+=units(thing.height)*0.5f;
        float radius=256,strength=1.7f;
        game_thing_role role=Game_ThingRole(thing.type);
        if(role==GAME_THING_BIG_SHOT) {radius=448;strength=2.4f;}
        else if(role==GAME_THING_ROCKET) {radius=288;strength=1.5f;}
        auto color=frameGlow(thing.sprite,thing.frame);
        bool enhanced=monsterProjectileLight(thing,color,radius,strength);
        const spritedef_t &sprite=sprites[thing.sprite];
        if(!enhanced&&(thing.frame&FF_FRAMEMASK)<sprite.numframes) {
            const Image &image=lumpImage(firstspritelump+sprite.spriteframes[thing.frame&FF_FRAMEMASK].lump[0],false);
            strength*=std::clamp(image.width*image.height/169.0f,0.25f,1.0f);
        }
        auto joined=std::find_if(clusters.begin(),clusters.end(),[&](const Cluster &c) {
            return c.enhanced==enhanced&&(!enhanced||c.thing->type==thing.type)&&
                std::fabs(c.x-x)<96&&std::fabs(c.y-y)<96&&std::fabs(c.z-z)<96;});
        if(joined!=clusters.end()) {
            joined->radius=std::max(joined->radius,radius);joined->strength=std::max(joined->strength,strength);++joined->members;
        } else if(clusters.size()<8) clusters.push_back({&thing,x,y,z,radius,strength,1,color,enhanced});
    }
    for(const Cluster &c:clusters)
        appendLight(c.x,c.y,c.z,c.radius,c.strength*std::min(1.5f,1+0.2f*(c.members-1)),c.color,c.thing,0.75f);
    // Teleport flashes share the projectiles' light kind; the nearest four.
    std::sort(teleports.begin(),teleports.end(),[](const Candidate &a,const Candidate &b) {return a.distance<b.distance;});
    for(size_t n=0;n<std::min(size_t(4),teleports.size());++n) {
        const mobj_t &thing=*teleports[n].thing;
        appendLight(units(thing.x),units(thing.y),units(thing.z)+28,320,teleportLight(thing),frameGlow(thing.sprite,thing.frame),&thing,0.75f);
    }
    lightKind=I_RENDER3D_LIGHT_DOOR;
    doorSpill(camera);
    lightKind=I_RENDER3D_LIGHT_SHOT;
    collectBfgAftershockLights();
    collectGreenImpactFireLights(camera);
}
float flashAt(float x,float y,float z,const Flash &f) {
    return doom_flash_at(x,y,z,&f,lightBlockers.data());
}
// Door light spill: sectors closed at level start are doors. While one is
// open, its brighter side lights the darker side through the opening: a
// light just outside each face on the dark side, as strong as the light
// difference times how far the door is open (in eighths), tinted by the
// bright side's floor. The four nearest join the dynamic lights.
struct DoorPortal { int door;std::vector<std::pair<int,int>> faces;float fullOpen; }; // faces: line, neighbor sector
static std::vector<DoorPortal> doorPortals;
void findDoors() {
    doorPortals.clear();
    for(int i=0;i<numsectors;++i) {
        const sector_t &sector=sectors[i];
        if(sector.ceilingheight>sector.floorheight)continue;
        DoorPortal door={i,{},0};float lowest=1e9f;
        for(int j=0;j<sector.linecount;++j) {
            const line_t &line=*sector.lines[j];
            const sector_t *other=line.frontsector==&sector?line.backsector:line.frontsector;
            if(!other||other==&sector)continue;
            door.faces.push_back({int(&line-lines),int(other-sectors)});lowest=std::min(lowest,units(other->ceilingheight));
        }
        if(door.faces.size()<2)continue;
        door.fullOpen=std::max(8.0f,lowest-4-units(sector.floorheight));
        doorPortals.push_back(std::move(door));
    }
}
static void doorSpill(const Uniforms &camera) {
    if(!settings.doorLight)return;
    struct Spill { float x,y,z,radius,strength,distance;std::array<float,3> color; };
    std::vector<Spill> spills;
    for(const auto &door:doorPortals) {
        const sector_t &sector=sectors[door.door];
        float open=ceilZ(&sector)-floorZ(&sector);
        if(open<2)continue;
        float fraction=std::ceil(std::clamp(open/door.fullOpen,0.0f,1.0f)*8)/8;
        for(auto [index,dark]:door.faces) {
            int bright=-1;
            for(auto [other,neighbor]:door.faces)
                if(neighbor!=dark&&(bright<0||lighting(&sectors[neighbor])>lighting(&sectors[bright])))bright=neighbor;
            if(bright<0)continue;
            float difference=lighting(&sectors[bright])-lighting(&sectors[dark]);
            if(difference<0.06f)continue;
            const line_t &line=lines[index];
            float ax=units(line.v1->x),ay=units(line.v1->y),ex=units(line.v2->x)-ax,ey=units(line.v2->y)-ay,len=std::hypot(ex,ey);
            if(len<1)continue;
            // The front side's normal is the right of v1->v2.
            float side=line.frontsector==&sectors[dark]?1.0f:-1.0f,nx=ey/len*side,ny=-ex/len*side;
            float x=ax+ex*0.5f+nx*20,y=ay+ey*0.5f+ny*20,distance=std::hypot(x-camera.eye[0],y-camera.eye[1]);
            if(distance>1400)continue;
            int pic=sectors[bright].floorpic;
            std::array<float,3> color={1,1,1};
            if(pic!=skyflatnum) {
                const auto &average=averageColor(lumpImage(firstflat+flattranslation[pic],true));
                float peak=std::max({average[0],average[1],average[2],0.01f});
                for(int c=0;c<3;++c)color[c]=0.5f+0.5f*average[c]/peak;
            }
            spills.push_back({x,y,floorZ(&sector)+std::min(open,96.0f)*0.55f,std::clamp(len*2+96,128.0f,288.0f),difference*fraction*1.8f,distance,color});
        }
    }
    std::sort(spills.begin(),spills.end(),[](const Spill &a,const Spill &b){return a.distance<b.distance;});
    for(size_t n=0;n<std::min(size_t(4),spills.size());++n) {
        const Spill &s=spills[n];
        appendLight(s.x,s.y,s.z,s.radius,s.strength,s.color,nullptr,0.75f);
    }
}

void selectFogLights(const Uniforms &camera) {
    fogLights={};if(!settings.fog)return;
    std::vector<std::pair<float,unsigned>> candidates;
    for(unsigned i=0;i<flashes.count;++i) {
        const auto &light=flashes.lights[i];
        float distance=std::hypot(light.position[0]-camera.eye[0],light.position[1]-camera.eye[1]);
        if(distance>light.position[3]+512)continue;
        float score=light.direction[3]>0?1000:light.strength/(1+distance/128);
        candidates.emplace_back(score,i);
    }
    std::stable_sort(candidates.begin(),candidates.end(),[](auto a,auto b){return a.first>b.first;});
    const size_t budget=std::min<size_t>(settings.fogLightBudget,std::size(fogLights.indices));
    for(size_t i=0;i<std::min(budget,candidates.size());++i)
        fogLights.indices[fogLights.count++]=candidates[i].second;
}
// A sphere that misses a triangle's bounds cannot illuminate any fragment
// of it. The conservative mask preserves every light inside its radius.
void assignLightMasks() {
    struct Job {Vertex *vertices;size_t count;VertexSection* section;uint64_t stamp=0;};
    static std::vector<Job> jobs;jobs.clear();size_t triangles=0;
    for(auto *group:{&batches,&mirrorBatches})for(auto &batch:*group) {
        auto &vertices=batch.second;
        auto add=[&](size_t first,size_t end,VertexSection* section){
            for(size_t n=first;n+2<end;n+=768){size_t count=std::min(size_t(768),(end-n)/3*3);jobs.push_back({vertices.data()+n,count,section});triangles+=count/3;}
        };
        auto sections=vertexSections.find(&vertices);size_t cursor=0;
        if(sections!=vertexSections.end())for(auto& section:sections->second){
            if(section.first<cursor||section.first+section.count>vertices.size())continue;
            add(cursor,section.first,nullptr);section.maskStamp=1469598103934665603ULL;
            add(section.first,section.first+section.count,&section);cursor=section.first+section.count;
        }
        add(cursor,vertices.size(),nullptr);
    }
    auto assign=[&](int index) {
        auto &job=jobs[index];auto vertices=job.vertices;job.stamp=1469598103934665603ULL;
        for(size_t n=0;n<job.count;n+=3) {
            unsigned mask[2]={};
            if(!(vertices[n].mode&MODE_SPRITE)||(vertices[n].mode&MODE_RELIT)) {
                const auto &a=vertices[n],&b=vertices[n+1],&c=vertices[n+2];
                float low[3]={std::min({a.x,b.x,c.x}),std::min({a.y,b.y,c.y}),std::min({a.z,b.z,c.z})};
                float high[3]={std::max({a.x,b.x,c.x}),std::max({a.y,b.y,c.y}),std::max({a.z,b.z,c.z})};
                for(unsigned i=0;i<flashes.count;++i) {
                    const auto &light=flashes.lights[i];
                    if(!fogOnly[i]&&doom_flash_reaches_bounds(&light,low,high))mask[i/32]|=1u<<(i%32);
                }
            }
            job.stamp=(job.stamp^mask[0])*1099511628211ULL;job.stamp=(job.stamp^mask[1])*1099511628211ULL;
            for(size_t v=n;v<n+3;++v)std::copy(mask,mask+2,vertices[v].lightMask);
        }
    };
    if(triangles*flashes.count>=65536)frameParallelFor((int)jobs.size(),assign);
    else for(int n=0;n<(int)jobs.size();++n)assign(n);
    for(const auto& job:jobs)if(job.section)job.section->maskStamp=(job.section->maskStamp^job.stamp)*1099511628211ULL;
    if(M_CheckParm((char*)"-verifylightthreads"))for(int n=0;n<(int)jobs.size();++n) {
        const auto &job=jobs[n];std::vector<std::array<unsigned,2>> expected(job.count);
        for(size_t v=0;v<job.count;++v)std::copy(job.vertices[v].lightMask,job.vertices[v].lightMask+2,expected[v].begin());
        assign(n);
        for(size_t v=0;v<job.count;++v)if(!std::equal(expected[v].begin(),expected[v].end(),job.vertices[v].lightMask))
            I_Error((char*)"Parallel light masks differ from serial lighting");
    }
}
// Profiling: leaves out the light kinds named in profile.skip.
void skipProfiledLights() {
    unsigned kept=0;
    for(unsigned i=0;i<flashes.count;++i) {
        if(profile.skip&(1u<<lightKinds[i]))continue;
        flashes.lights[kept]=flashes.lights[i];lightSources[kept]=lightSources[i];
        fogOnly[kept]=fogOnly[i];lightKinds[kept]=lightKinds[i];++kept;
    }
    flashes.count=kept;
}
// The GPU's lights: each light's blockers in lightWords (two words each, the
// floats' bits), then for lights with more than a few the slice table and
// its 16-bit indices (doom_flash_slice_blockers), so a pixel only tests the
// walls in its direction. first is the light's word offset, with the top bit
// set when it has slices.
FlashSet gpuFlashes;
std::vector<std::array<uint32_t,4>> lightWords;
std::array<float,maxLights> lightSliceCost={}; // Profiling: blockers a pixel tests on average.
void buildLightWords() {
    // Each light owns its scratch/output. Keep packing order identical to the
    // serial path; only the independent slice calculations run concurrently.
    struct Packed {
        std::vector<std::array<uint32_t,4>> words;
        std::vector<LightBlocker> blockers;
        std::array<float,8> shape{};
        bool valid=false,sliced=false;float cost=0;
    };
    static std::array<Packed,maxLights> packed;
    auto pack=[&](int i,bool force=false) {
        thread_local std::vector<uint16_t> indices(65535);
        auto &result=packed[i];auto &words=result.words;
        const Flash &f=flashes.lights[i];
        const LightBlocker *own=lightBlockers.data()+f.first;
        // Slice construction depends only on light position/radius/cone and
        // the ordered blocker geometry, never color, flicker or GPU offsets.
        std::array<float,8> shape;
        std::copy(f.position,f.position+4,shape.begin());std::copy(f.direction,f.direction+4,shape.begin()+4);
        bool same=result.valid&&result.shape==shape&&result.blockers.size()==f.count&&
            (!f.count||!memcmp(result.blockers.data(),own,f.count*sizeof(LightBlocker)));
        if(same&&!force)return;
        result.shape=shape;result.blockers.assign(own,own+f.count);result.valid=true;
        words.clear();result.sliced=false;
        for(uint32_t n=0;n<f.count;++n) {
            std::array<uint32_t,4> line,opening;
            memcpy(line.data(),own[n].line,sizeof(line));memcpy(opening.data(),own[n].opening,sizeof(opening));
            words.push_back(line);words.push_back(opening);
        }
        result.cost=(float)f.count;
        if(f.count<=4)return;
        uint32_t table[DOOM_FLASH_SLICES];
        uint32_t listed=doom_flash_slice_blockers(&f,own,f.count,table,indices.data(),(uint32_t)indices.size());
        if(listed==UINT32_MAX)return;
        for(int n=0;n<DOOM_FLASH_SLICES;n+=4)words.push_back({table[n],table[n+1],table[n+2],table[n+3]});
        for(uint32_t n=0;n<listed;n+=8) {
            std::array<uint32_t,4> word={};
            for(uint32_t k=0;k<8&&n+k<listed;++k)word[k/2]|=(uint32_t)indices[n+k]<<(16*(k%2));
            words.push_back(word);
        }
        result.sliced=true;result.cost=(float)listed/DOOM_FLASH_SLICES;
    };
    size_t blockers=0;for(unsigned i=0;i<flashes.count;++i)blockers+=flashes.lights[i].count;
    if(blockers>=128&&flashes.count>=4)frameParallelFor((int)flashes.count,[&](int i){pack(i);});
    else for(unsigned i=0;i<flashes.count;++i)pack(i);
    if(M_CheckParm((char*)"-verifylightthreads"))for(unsigned i=0;i<flashes.count;++i) {
        auto expected=packed[i];pack(i,true);
        if(expected.words!=packed[i].words||expected.sliced!=packed[i].sliced||expected.cost!=packed[i].cost)
            I_Error((char*)"Parallel light packing differs from serial lighting");
    }
    gpuFlashes=flashes;lightWords.clear();
    for(unsigned i=0;i<flashes.count;++i) {
        const auto &result=packed[i];
        gpuFlashes.lights[i].first=(uint32_t)lightWords.size()|(result.sliced?0x80000000u:0);
        lightWords.insert(lightWords.end(),result.words.begin(),result.words.end());
        lightSliceCost[i]=result.cost;
    }
    if(lightWords.empty())lightWords.push_back({});
}
void clearFlashPulses() {flashPulses.clear();}
void lightsShutdown() {flashPulses.clear();glowColors.clear();burstBarrels.clear();staticFog.clear();doorPortals.clear();}
extern "C" void I_TestKeyLights() {
    mobj_t *player=players[displayplayer].mo;
    Uniforms camera={};camera.eye[0]=units(player->x);camera.eye[1]=units(player->y);camera.eye[2]=units(player->z)+41;
    bool savedBeam=flashlightOn,savedBake=bakeOnlyActive;flashlightOn=false;
    for(bool only:{false,true})for(int number:{5,6,13,38,39,40}) {
        int type=0;while(type<NUMMOBJTYPES&&mobjinfo[type].doomednum!=number)++type;
        if(type==NUMMOBJTYPES)I_Error((char*)"Missing key fixture type");
        mobj_t *key=P_SpawnMobj(player->x,player->y,player->floorz,(mobjtype_t)type);
        bakeOnlyActive=only;collectFlashes(1,camera);
        const Flash *found=nullptr;
        for(unsigned n=0;n<flashes.count;++n)if(lightSources[n]==key)found=&flashes.lights[n];
        // The subdued key light must still reach the nearby floor in both
        // dynamic and bake-only modes, without requiring the old bright pool.
        if(!found||found->baked!=0||flashAt(camera.eye[0]+24,camera.eye[1],units(player->floorz)+1,*found)<.25f)
            I_Error((char*)"Key %d missing useful dynamic floor light (bake-only %d)",number,only);
        int channel=(number==5||number==40)?2:0;
        if(found->color[channel]<.99f)I_Error((char*)"Wrong key light color");
        P_RemoveMobj(key);collectFlashes(1,camera);
        for(unsigned n=0;n<flashes.count;++n)if(lightSources[n]==key)I_Error((char*)"Collected key retained its light");
    }
    flashlightOn=savedBeam;bakeOnlyActive=savedBake;
    puts("All six key lights passed: floor illumination, colors, bake-only mode, and removal.");
}
} // namespace doom3d

/* Sprite shot intersections, optional human critical parts, and attached
   wound holes. The holes themselves remain cosmetic; critical parts is an
   explicit player-bullet rule, independent of wound drawing. */
#include "internal.h"
#include "gore_policy.h"
#include "gore_stock.h"
#include "../wound_attachment.h"
#include "../death_joints.h"
#include "../monster_regions.h"
#include <limits>
extern "C" {
#include "../../linuxdoom-1.10/p_local.h"
}

namespace doom3d {
// Capture the texel struck before damage switches the monster to its pain
// or death drawing. Other frames attach to that original patch of artwork,
// never to the previous frame (which would accumulate drift). The tunnel
// follows the shot in the monster's local coordinates, including side hits.
struct Wound {
    float height,across,radius,yaw;
    const Image *source;
    doom_wounds::Point point;
    bool flipped;
    uint32_t seed;
    std::unordered_map<const Image*,doom_wounds::Point> frames;
    int tic=0;
    enum class Kind { Bullet, Saw } kind=Kind::Bullet;
    unsigned char stage=0,contacts=0;
};
static std::unordered_map<const mobj_t*,std::vector<Wound>> wounds;
// Pending contacts are bounded by the SSG's pellet count and traversed in
// contact order, independent of actor allocation addresses.
struct GorePoint {float x=0,y=0,z=0;};
struct GoreShot {
    const mobj_t *target=nullptr;
    GoreDecision decision;
    GorePoint impact,legacyHead,surface,confirmedSurface;
    std::array<GorePoint,static_cast<unsigned>(GoreRegion::Count)> regionImpact{};
    std::array<bool,static_cast<unsigned>(GoreRegion::Count)> verifiedRegion{};
    GoreRegion region=GoreRegion::Unknown;
    float dx=1,dy=0;
    bool opaque=false,livingArt=false;
};
static std::array<GoreShot,20> goreShots;
static uint64_t goreDischarge;
static unsigned goreShotLevel;
static const mobj_t *goreShooter;
static std::unordered_map<const mobj_t*,GoreOutcome> goreCorpses;
static std::unordered_map<const Image*,const GoreFrame*> verifiedGoreFrames;
struct GoreLivingState { GoreRegion region=GoreRegion::Unknown; WoundSpot drip{}; };
static std::unordered_map<const mobj_t*,GoreLivingState> goreLiving;
static std::unordered_map<const Image*,const GoreLivingFrame*> verifiedLivingFrames;
// Outcome-dependent masks share storage across corpses of the same frame.
static std::unordered_map<const Image*,std::map<GoreOutcome,std::vector<byte>>> goreFrameMasks;

static std::unordered_map<int,float> standing;
struct WoundSurface {
    const mobj_t *viewer;
    const Image *image;
    float x,y,feet,top,bottom,rx,ry,yaw;
    BodyPose pose;
    int tic;
    bool flipped,posed;
    int sprite,frame,rotation;
};
static std::unordered_map<const mobj_t*,WoundSurface> surfaces;
struct SpriteImpact {
    enum Part { Body,Head } part=Body;
    int multiplier=1;
    const Image *image=nullptr;
    doom_wounds::Point point{};
    bool flipped=false;
    GorePoint world{};
    float viewYaw=0,sourceYaw=0,distance=0;
};
static std::unordered_map<const mobj_t*,SpriteImpact> bulletImpacts;
static std::unordered_map<const mobj_t*,critical_hit_t> bulletResults;
struct HeadProfile { bool found;doom_joints::Head head; };
static std::unordered_map<const Image*,HeadProfile> heads;
static std::unordered_map<const Image*,std::map<int,doom_regions::Mask>> regionMasks;
static const doom_regions::Mask &weakpointRegions(int type,const Image &image) {
    auto &types=regionMasks[&image];auto found=types.find(type);
    if(found==types.end())found=types.emplace(type,doom_regions::build(type,image.width,image.height,image.left,image.top,image.pixels.data())).first;
    return found->second;
}
const std::vector<unsigned char> &weakpointRegionMask(int type,const Image &image) {return weakpointRegions(type,image).pixels;}
static mobj_t *bulletShooter=nullptr;
static fixed_t bulletX,bulletY,bulletZ,bulletDx,bulletDy,bulletSlope,bulletRange;
// Geometry supplies current placement when the target has not been drawn recently.
void (*prepareCriticalSurface)(mobj_t*,mobj_t*)=nullptr;
bool (*criticalVoxelModel)(const mobj_t&)=nullptr;
// A lying drawing's body: its opaque columns, the middle row of each, and which end the head is.
struct Lying { int first=-1,last=-1;bool headRight=true;std::vector<float> middle; };
static std::unordered_map<const Image*,Lying> lyings;
constexpr size_t maxWounds=10;
constexpr float maxRadius=3.0f;

void woundView(const mobj_t *thing,const mobj_t *viewer,const Image &image,bool flipped,float x,float y,float feet,
               float top,float bottom,float rx,float ry,const BodyPose *pose,int rotation) {
    if((!settings.wounds&&!settings.criticalParts&&!settings.enhancedWeakPoints&&!settings.weaponVulnerabilities&&!settings.monsterMetalSparks&&!(settings.goreLevel&&settings.blood&&totalkills<200))||!thing||!thing->info||thing->health<=0)return;
    if(!(thing->info->flags&MF_COUNTKILL)&&thing->type!=MT_SKULL)return;
    surfaces[thing]={viewer,&image,x,y,feet,top,bottom,rx,ry,
        (float)(thing->angle*(2*doomPi/4294967296.0)),pose?*pose:BodyPose{},leveltime,flipped,pose!=nullptr,thing->sprite,thing->frame,rotation};
}
static bool opaqueAt(const Image &image,doom_wounds::Point point) {
    int c=(int)std::floor(point.u),r=(int)std::floor(point.v);
    return c>=0&&r>=0&&c<image.width&&r<image.height&&image.pixels[((size_t)r*image.width+c)*2+1];
}
// Recreate the uncut surface only when a shot arrives. Its triangles carry
// the same UVs as the drawn strips, so intersection also inverts flinching.
static bool surfaceHit(const WoundSurface &surface,const mobj_t &shooter,float x,float y,float z,
                       float dx,float dy,float slope,doom_wounds::Point &point,float *distance=nullptr) {
    const Image &image=*surface.image;
    float left=-(float)image.left,right=left+image.width;
    float u0=surface.flipped?(float)image.width:0,u1=surface.flipped?0:(float)image.width;
    std::vector<Vertex> vertices;
    if(surface.posed)
        bodyQuads(vertices,surface.pose,{},surface.x,surface.y,surface.feet,surface.rx,surface.ry,
                  left,right,image.top,image.height,u0,u1);
    else
        quad(vertices,{surface.x+surface.rx*left,surface.y+surface.ry*left,surface.top,u0,0},
                      {surface.x+surface.rx*right,surface.y+surface.ry*right,surface.top,u1,0},
                      {surface.x+surface.rx*right,surface.y+surface.ry*right,surface.bottom,u1,(float)image.height},
                      {surface.x+surface.rx*left,surface.y+surface.ry*left,surface.bottom,u0,(float)image.height});
    // Start at the shooter, rather than the puff: a flinching body can bend
    // toward the eye far enough to be in front of the game's blood point.
    float sx=units(shooter.x),sy=units(shooter.y),travel=(x-sx)*dx+(y-sy)*dy;
    using Vec=std::array<float,3>;
    Vec origin={sx,sy,z-travel*slope},direction={dx,dy,slope};
    auto cross=[](const Vec &a,const Vec &b)->Vec {return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};};
    auto dot=[](const Vec &a,const Vec &b) {return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];};
    float nearest=INFINITY;bool hit=false;
    for(size_t i=0;i+2<vertices.size();i+=3) {
        const Vertex &a=vertices[i],&b=vertices[i+1],&c=vertices[i+2];
        Vec e1={b.x-a.x,b.y-a.y,b.z-a.z},e2={c.x-a.x,c.y-a.y,c.z-a.z};
        Vec p=cross(direction,e2);float determinant=dot(e1,p);
        if(std::abs(determinant)<1e-6f)continue;
        Vec offset={origin[0]-a.x,origin[1]-a.y,origin[2]-a.z};
        float u=dot(offset,p)/determinant;
        if(u<0||u>1)continue;
        Vec q=cross(offset,e1);float v=dot(direction,q)/determinant;
        if(v<0||u+v>1)continue;
        float t=dot(e2,q)/determinant;
        if(t<0||t>=nearest)continue;
        doom_wounds::Point at={a.u+(b.u-a.u)*u+(c.u-a.u)*v,a.v+(b.v-a.v)*u+(c.v-a.v)*v};
        if(!opaqueAt(image,at))continue;
        nearest=t;point=at;hit=true;
    }
    if(hit&&distance)*distance=nearest;
    return hit;
}

static doom_wounds::Frame art(const Image &image) {
    return {image.width,image.height,image.left,image.top,image.pixels.data()};
}
static doom_wounds::Point sawTorso(const Image &image) {
    // Pinky and Spectre share art. The front-facing chest is broadest above
    // the mouth, while profile frames have more room down the flank.
    const auto &anatomy=weakpointRegions(MT_SERGEANT,image);
    if(!anatomy.stock)return {-1,-1};
    doom_wounds::Point point{-1,-1};
    float best=std::numeric_limits<float>::max();
    for(int y=image.height/5;y<image.height*3/4;++y)
        for(int x=image.width/4;x<image.width*3/4;++x) {
            if(anatomy.pixels[(size_t)y*image.width+x]!=WP_BODY)continue;
            const float across=x+.5f-image.width*.5f;
            const float down=y+.5f-image.height*.39f;
            int flesh=0;
            for(int r=std::max(0,y-8);r<=std::min(image.height-1,y+9);++r)
                for(int c=x-2;c<=x+2;++c)
                    flesh+=anatomy.pixels[(size_t)r*image.width+c]==WP_BODY;
            const float score=.065f*across*across+.043f*down*down-flesh;
            if(score<best){point={x+.5f,y+.5f};best=score;}
        }
    return point;
}
static doom_wounds::Point attached(Wound &wound,const Image &image) {
    if(wound.source==&image)return wound.point;
    auto found=wound.frames.find(&image);
    if(found!=wound.frames.end())return found->second;
    return wound.frames[&image]=wound.kind==Wound::Kind::Saw?sawTorso(image):
        doom_wounds::map(art(*wound.source),art(image),wound.point);
}
// Project an attached patch from its entry side to the current billboard.
static doom_wounds::Point projected(Wound &wound,const Image &image,bool flipped,float facing) {
    auto point=attached(wound,image);
    float left=-(float)image.left,right=left+image.width;
    float across=wound.flipped?right-point.u:left+point.u;
    point.u=flipped?right-across*facing:across*facing-left;
    return point;
}

static bool captureSpriteHit(mobj_t *target,fixed_t fx,fixed_t fy,fixed_t fz,fixed_t fdx,fixed_t fdy,fixed_t slope,const mobj_t *shooter,SpriteImpact &impact) {
    if(!target||!target->info)return false;
    if(!(target->info->flags&MF_COUNTKILL)&&target->type!=MT_SKULL)return false;
    if(target->sprite<0||target->sprite>=numsprites)return false;
    const auto &sprite=sprites[target->sprite];int frame=target->frame&FF_FRAMEMASK;
    if(frame>=sprite.numframes)return false;
    float dx=units(fdx),dy=units(fdy),length=std::hypot(dx,dy);
    if(length<1e-4f)return false;
    dx/=length;dy/=length;
    float viewYaw=std::atan2(dy,dx),sourceYaw;
    float a=(float)(target->angle*(2*doomPi/4294967296.0));
    const Image *source=nullptr;bool flipped=false;doom_wounds::Point point={};
    float alongFromPuff=0;
    auto drawn=surfaces.find(target);
    if(shooter&&drawn!=surfaces.end()&&drawn->second.viewer==shooter&&leveltime-drawn->second.tic<=2) {
        const WoundSurface &surface=drawn->second;
        if(!surfaceHit(surface,*shooter,units(fx),units(fy),units(fz),dx,dy,units(slope),point,&impact.distance))return false;
        alongFromPuff=impact.distance-((units(fx)-units(shooter->x))*dx+(units(fy)-units(shooter->y))*dy);
        source=surface.image;flipped=surface.flipped;sourceYaw=surface.yaw;
        viewYaw=std::atan2(surface.rx,-surface.ry);
    } else {
        // Off-screen hits use the attacker's view. Frame selection is based
        // on the bearing to the body's center, not an off-center pellet ray.
        float bearing=shooter?std::atan2(units(target->y)-units(shooter->y),units(target->x)-units(shooter->x)):viewYaw;
        angle_t angle=(angle_t)(int64_t)(bearing*(4294967296.0/(2*doomPi)));
        const auto &sf=sprite.spriteframes[frame];
        unsigned rotation=sf.rotate?(angle-target->angle+(unsigned)(ANG45/2)*9)>>29:0;
        source=&lumpImage(firstspritelump+sf.lump[rotation],false);flipped=sf.flip[rotation];sourceYaw=a;
        if(source->pixels.empty())return false;
        if(shooter&&shooter->player)viewYaw=(float)(shooter->angle*(2*doomPi/4294967296.0));
        float nx=std::cos(viewYaw),ny=std::sin(viewYaw),along=dx*nx+dy*ny;
        if(std::abs(along)<1e-4f)return false;
        float ox=units(fx)-units(target->x),oy=units(fy)-units(target->y),t=-(ox*nx+oy*ny)/along;
        float height=units(fz)-units(target->z)+t*units(slope);
        float across=(ox+dx*t)*ny-(oy+dy*t)*nx;
        float left=-(float)source->left,right=left+source->width;
        point={flipped?right-across:across-left,source->top-height};
        impact.distance=t;alongFromPuff=t;
    }
    const Image &image=*source;
    if(!opaqueAt(image,point))return false;
    impact.image=source;impact.point=point;impact.flipped=flipped;
    impact.viewYaw=viewYaw;impact.sourceYaw=sourceYaw;
    impact.world={units(fx)+dx*alongFromPuff,units(fy)+dy*alongFromPuff,units(fz)+units(slope)*alongFromPuff};
    return true;
}

// A death mask is only safe on the verified stock death frame. A replacement
// WAD, alternate frame family, or changed DEH actor state gets normal death art.
static bool goreFingerprintMatches(const GoreFingerprint &fp,const Image &image,uint32_t hash) {
    return fp.hash==hash&&fp.width==image.width&&fp.height==image.height&&fp.left==image.left&&fp.top==image.top;
}
static uint32_t goreHash(const Image &image) {
    uint32_t hash=2166136261u;
    for(byte pixel:image.pixels)hash=(hash^pixel)*16777619u;
    return hash;
}
static const GoreFrame *goreVerifyFrame(int type,int lump) {
    if(!lumpinfo||lump<firstspritelump||lump-firstspritelump>=numspritelumps)return nullptr;
    for(const auto &frame:goreFrames) {
        if(frame.type!=type||std::strncmp(frame.name,lumpinfo[lump].name,8))continue;
        const auto &image=lumpImage(lump,false);
        auto [it,inserted]=verifiedGoreFrames.try_emplace(&image,nullptr);
        if(inserted&&goreFingerprintMatches(frame.pixels,image,goreHash(image)))it->second=&frame;
        return it->second==&frame?&frame:nullptr;
    }
    return nullptr;
}
static bool goreStockDeath(const mobj_t &actor) {
    if(!actor.info||!goreHuman(actor.type)||!sprites)return false;
    const int state=actor.info->deathstate;
    if(state<=S_NULL||state>=NUMSTATES)return false;
    const auto &frame=states[state];
    if(frame.sprite<0||frame.sprite>=numsprites)return false;
    const auto &sprite=sprites[frame.sprite];
    const int index=frame.frame&FF_FRAMEMASK;
    if(index>=sprite.numframes||!sprite.spriteframes)return false;
    const int lump=firstspritelump+sprite.spriteframes[index].lump[0];
    const auto *frameArt=goreVerifyFrame(actor.type,lump);
    return frameArt&&!std::strcmp(frameArt->name,actor.type==MT_POSSESSED?"POSSH0":"SPOSH0");
}
static bool goreVerifySequence(const mobj_t &actor) {
    int start=0,count=0,spriteIndex=0,firstFrame=0;
    switch(actor.type) {
    case MT_POSSESSED:start=S_POSS_DIE1;count=5;spriteIndex=SPR_POSS;firstFrame=7;break;
    case MT_SHOTGUY:start=S_SPOS_DIE1;count=5;spriteIndex=SPR_SPOS;firstFrame=7;break;
    case MT_TROOP:start=S_TROO_DIE1;count=5;spriteIndex=SPR_TROO;firstFrame=8;break;
    case MT_UNDEAD:start=S_SKEL_DIE1;count=6;spriteIndex=SPR_SKEL;firstFrame=11;break;
    default:return false;
    }
    if(!actor.info||actor.info->deathstate!=start||!sprites||spriteIndex>=numsprites)return false;
    const auto &sprite=sprites[spriteIndex];
    if(!sprite.spriteframes||sprite.numframes<firstFrame+count)return false;
    bool current=false;
    for(int i=0;i<count;++i) {
        const auto &state=states[start+i];
        current|=actor.state==&state;
        if(state.sprite!=spriteIndex||(state.frame&FF_FRAMEMASK)!=firstFrame+i||
           state.nextstate!=(i+1<count?start+i+1:S_NULL))return false;
        const auto &frame=sprite.spriteframes[firstFrame+i];
        const int rotations=actor.type==MT_UNDEAD&&i==0?8:1;
        if(bool(frame.rotate)!=(rotations==8))return false;
        for(int rot=0;rot<rotations;++rot) {
            const auto *art=goreVerifyFrame(actor.type,firstspritelump+frame.lump[rot]);
            if(!art||art->name[4]!='A'+firstFrame+i||art->name[5]!=(rotations==8?'1'+rot:'0'))return false;
        }
    }
    return current;
}


static bool goreActive() {
    return settings.accelerated&&settings.goreLevel==1&&settings.blood&&totalkills<200&&
           levelSerial==r_levelserial&&!netgame&&!demoplayback&&!demorecording;
}
static bool goreDebug() {
    static const bool enabled=M_CheckParm((char*)"-goredebug")!=0;
    return enabled;
}
static bool goreSpriteActor(const mobj_t &target) {
    return !(target.flags&MF_NOBLOOD)&&(!criticalVoxelModel||!criticalVoxelModel(target));
}
static const GoreLivingFrame *goreLivingArt(int type,const Image &image) {
    if(!goreLivingCavity(type))return nullptr;
    auto [it,inserted]=verifiedLivingFrames.try_emplace(&image,nullptr);
    if(inserted) {
        const auto hash=goreHash(image);
        for(const auto &art:goreLivingFrames)
            if(art.type==type&&goreFingerprintMatches(art.pixels,image,hash)){it->second=&art;break;}
    }
    return it->second&&it->second->type==type?it->second:nullptr;
}
static std::array<float,2> goreCavityRadii(int type) {
    if(goreHuman(type))return {3.5f,2.8f};
    return type==MT_FATSO?std::array<float,2>{8.f,5.f}:std::array<float,2>{4.f,3.4f};
}
static GoreRegion goreRegion(int type,const SpriteImpact &impact,const GoreLivingFrame *art) {
    if(type==MT_HEAD||(goreHuman(type)&&!art))return GoreRegion::Unknown;
    const Image &image=*impact.image;
    const auto &anatomy=weakpointRegions(type,image);
    const int u=(int)impact.point.u,v=(int)impact.point.v;
    const auto region=anatomy.pixels[(size_t)v*image.width+u];
    if(art&&region==WP_BODY) {
        const auto &c=art->cavities;
        const auto radii=goreCavityRadii(type);
        const auto distance=[&](int x,int y) {
            const float dx=(impact.point.u-x)/radii[0],dy=(impact.point.v-y)/radii[1];
            return dx*dx+dy*dy;
        };
        const float chest=distance(c.chestX,c.chestY),belly=distance(c.bellyX,c.bellyY);
        if(std::min(chest,belly)<=2.25f)return chest<=belly?GoreRegion::Chest:GoreRegion::Belly;
        return GoreRegion::Unknown;
    }
    if(!anatomy.stock)return GoreRegion::Unknown;
    if(region==WP_HEAD)return GoreRegion::Head;
    if(region!=WP_BODY)return GoreRegion::Unknown;

    // Stock weakpoint art can support fragments in additional views. Only
    // the per-frame torso landmarks above can authorize a persistent mask.
    const float row=impact.point.v/image.height;
    const float across=std::abs(impact.point.u-image.left)/image.width;
    // The upper ridge and shoulders can be exposed above a frontal Pinky mouth.
    if(type==MT_SERGEANT)
        return row>=.16f&&row<=.70f&&across<.34f?GoreRegion::Chest:GoreRegion::Unknown;
    if(across>(type==MT_FATSO?.19f:.16f)||row<.27f||row>.64f)return GoreRegion::Unknown;
    return row<.45f?GoreRegion::Chest:GoreRegion::Belly;
}

static GoreShot *goreShotFor(const mobj_t *target) {
    for(auto &shot:goreShots)if(shot.target==target)return &shot;
    return nullptr;
}

// Later pellets continue through dead actors under the normal Doom rules.
void goreCapture(mobj_t *target,mobj_t *source,fixed_t x,fixed_t y,fixed_t z,
                 fixed_t dx,fixed_t dy,fixed_t slope) {
    if(!target||!source||!source->player||target->health<=0||!goreSupported(target->type)||
       P_WeakpointBulletKind()!=WP_SHOTGUN_PELLET)return;
    const uint64_t discharge=P_WeakpointDischarge(source);
    if(!discharge)return;
    const float distance=std::hypot(units(target->x)-units(source->x),units(target->y)-units(source->y));
    const bool eligible=goreActive()&&goreSpriteActor(*target)&&distance<=goreRange;
    if(!eligible&&!goreDebug())return;
    if(goreDischarge!=discharge||goreShotLevel!=r_levelserial||goreShooter!=source) {
        goreShots={};goreDischarge=discharge;goreShotLevel=r_levelserial;goreShooter=source;
    }
    auto *shot=goreShotFor(target);
    if(!shot) {
        shot=goreShotFor(nullptr);
        if(!shot)return;
        shot->target=target;
        auto &d=shot->decision;
        d.type=target->type;d.distance=distance;d.preHP=d.postHP=target->health;
        // Monsters expose spawnhealth as their health reference.
        d.maxHP=target->info?target->info->spawnhealth:0;
    }
    auto &d=shot->decision;
    ++d.contacts;
    shot->impact={units(x),units(y),units(z)};
    const float length=std::hypot(units(dx),units(dy));
    if(length>1e-4f){shot->dx=units(dx)/length;shot->dy=units(dy)/length;}
    shot->opaque=false;shot->region=GoreRegion::Unknown;shot->livingArt=false;
    SpriteImpact impact;
    if(!eligible||!captureSpriteHit(target,x,y,z,dx,dy,slope,source,impact))return;
    shot->opaque=true;shot->surface=impact.world;
    const auto *art=goreLivingArt(target->type,*impact.image);
    shot->region=goreRegion(target->type,impact,art);
    shot->livingArt=art!=nullptr;
    if(goreHuman(target->type)) {
        const auto &img=*impact.image;
        auto [it,inserted]=heads.try_emplace(&img);
        auto &profile=it->second;
        if(inserted)profile.found=doom_joints::findHead(img.pixels.data(),img.width,img.height,img.left,img.top,profile.head);
        const auto &head=profile.head;
        if(profile.found&&img.top-impact.point.v>=head.neck&&
           std::abs(impact.point.u-img.left-head.middle)<=head.width*.5f) {
            ++d.legacyHeads;shot->legacyHead=shot->impact;
        }
    }
}

static const char *goreOutcomeName(GoreOutcome outcome) {
    switch(outcome) {
    case GoreOutcome::HumanFlesh:return "human_flesh";
    case GoreOutcome::HeadBurst:return "head_burst";
    case GoreOutcome::SkullBurst:return "skull_burst";
    case GoreOutcome::ChestCavity:return "live_chest";
    case GoreOutcome::BellyCavity:return "live_belly";
    case GoreOutcome::ImpTissue:return "imp_tissue";
    case GoreOutcome::RevenantBone:return "revenant_bone";
    case GoreOutcome::MancubusHeavy:return "mancubus_heavy";
    case GoreOutcome::PinkyViscera:return "pinky_viscera";
    case GoreOutcome::CacoViscera:return "caco_blue_viscera";
    default:return "none";
    }
}

static void goreResolve(const mobj_t &target,GoreShot &shot,uint64_t discharge,bool legacyOnly) {
    auto &d=shot.decision;
    const char *reason="feature_off",*fallback="none",*art="uncut";
    auto outcome=GoreOutcome::None;
    bool maskApplied=false;
    GoreRegion region=GoreRegion::Unknown;
    if(target.type==d.type&&goreActive()&&goreSpriteActor(target)&&(!legacyOnly||goreHuman(d.type))) {
        if(d.fatal()!=(target.health<=0)) {
            // End-shot synthetic damage can change the actor's life state.
            reason="health_gate";
        } else {
            d.headEnabled=settings.goreHeads&&!settings.deathRagdoll;
            d.bodyEnabled=settings.goreBodyDamage&&!legacyOnly;
            d.legacyStock=d.fatal()&&d.distance<=goreRange&&goreStockDeath(target);
            d.already=goreLiving.count(&target)!=0;
            const auto result=goreCandidate(d);
            outcome=result.outcome;
            static constexpr const char *reasons[]={
                "emitted","health_gate","damage_gate","opaque_gate","range_gate","feature_off","already_emitted"
            };
            reason=reasons[(unsigned)result.reason];
        }
    }
    const bool head=outcome==GoreOutcome::HeadBurst||outcome==GoreOutcome::SkullBurst;
    if(head) {
        region=GoreRegion::Head;
        if(goreCorpses.size()<199&&goreVerifySequence(target)) {
            goreCorpses[&target]=outcome;
            art="stock_death";
            maskApplied=true;
        } else {
            fallback="art_unverified";
            outcome=goreHuman(d.type)?GoreOutcome::HumanFlesh:
                    outcome==GoreOutcome::SkullBurst?GoreOutcome::RevenantBone:GoreOutcome::ImpTissue;
        }
    }
    const bool cavity=outcome==GoreOutcome::ChestCavity||outcome==GoreOutcome::BellyCavity;
    if(cavity)region=outcome==GoreOutcome::ChestCavity?GoreRegion::Chest:GoreRegion::Belly;
    else if(outcome==GoreOutcome::PinkyViscera||outcome==GoreOutcome::RevenantBone) {
        if(region==GoreRegion::Unknown) {
            region=d.count(GoreRegion::Chest)>=d.count(GoreRegion::Belly)?GoreRegion::Chest:GoreRegion::Belly;
            if(!d.count(region))region=GoreRegion::Head;
        }
    }
    const auto index=(unsigned)region;
    const auto &point=head&&goreHuman(d.type)?shot.legacyHead:
        d.count(region)?shot.regionImpact[index]:d.spriteContacts?shot.confirmedSurface:shot.impact;
    if(!d.fatal()&&outcome!=GoreOutcome::None&&d.type!=MT_UNDEAD) {
        const bool verified=cavity&&shot.verifiedRegion[index];
        if(cavity&&!verified) {
            fallback="art_unverified";
            outcome=goreHuman(d.type)?GoreOutcome::HumanFlesh:
                    d.type==MT_TROOP?GoreOutcome::ImpTissue:GoreOutcome::MancubusHeavy;
        }
        if(!verified&&!settings.goreFragments&&!settings.goreWallBlood) {
            outcome=GoreOutcome::None;reason="feature_off";
        } else if(goreLiving.size()<199) {
            auto &live=goreLiving[&target];
            if(verified) {
                art="stock_living";
                maskApplied=true;
                live.region=region;
                const float yaw=std::atan2(shot.dy,shot.dx)-float(target.angle*(2*doomPi/4294967296.0));
                live.drip={point.z-units(target.z),0,yaw,3.f,leveltime};
            }
        } else {
            outcome=GoreOutcome::None;reason="feature_off";fallback="actor_budget";
        }
    }
    const bool burstEnabled=settings.goreFragments||(settings.goreWallBlood&&d.type!=MT_UNDEAD);
    if(outcome!=GoreOutcome::None&&!maskApplied&&!burstEnabled) {
        outcome=GoreOutcome::None;reason="feature_off";
    }
    if(goreDebug()) {
        const auto &h=d.hits;
        std::fprintf(stderr,"GORE M2.1: type=%d discharge=%llu preHP=%d postHP=%d maxHP=%d damage=%lld contacts=%d opaque=%d head=%d chest=%d belly=%d distance=%.1f fatal=%d outcome=%s style=%s reason=%s art=%s fallback=%s\n",
            d.type,(unsigned long long)discharge,d.preHP,d.postHP,d.maxHP,(long long)d.effectiveDamage,
            d.contacts,d.spriteContacts,goreHuman(d.type)?d.legacyHeads:h[1],h[2],h[3],
            d.distance,int(d.fatal()),goreOutcomeName(outcome),
            outcome==GoreOutcome::None?"none":goreStyleName(d.type),reason,art,fallback);
    }
    if(outcome!=GoreOutcome::None)
        goreBurst(target,point.x,point.y,point.z,shot.dx,shot.dy,outcome);
}

void goreAfterHit(mobj_t *target,mobj_t *source,int kind,int healthAfter,int extreme,int healthLoss) {
    if(kind!=WP_SHOTGUN_PELLET||goreShooter!=source||goreShotLevel!=r_levelserial||
       goreDischarge!=P_WeakpointDischarge(source))return;
    auto *shot=goreShotFor(target);
    if(!shot||shot->decision.fatal())return;
    auto &d=shot->decision;
    d.postHP=healthAfter;d.extreme=extreme;
    d.effectiveDamage+=healthLoss;
    if(healthLoss>0&&shot->opaque) {
        ++d.spriteContacts;shot->confirmedSurface=shot->surface;
        const auto region=(unsigned)shot->region;
        ++d.hits[region];shot->regionImpact[region]=shot->surface;
        shot->verifiedRegion[region]|=shot->livingArt;
    }
    if(d.fatal()&&!R_GoreShotEnd&&goreActive())goreResolve(*target,*shot,goreDischarge,true);
}

void goreFinishShot(mobj_t *shooter,uint64_t discharge) {
    if(discharge!=goreDischarge||shooter!=goreShooter)return;
    if(goreShotLevel==r_levelserial)
        for(auto &shot:goreShots)
            if(shot.target)goreResolve(*shot.target,shot,discharge,false);
    goreShots={};goreDischarge=0;goreShooter=nullptr;
}

void forgetGore(const mobj_t *actor,int death) {
    if(!death&&goreDischarge) {
        if(actor==goreShooter){goreShots={};goreDischarge=0;goreShooter=nullptr;}
        else if(auto *shot=goreShotFor(actor))*shot={};
    }
    goreCorpses.erase(actor);
    goreLiving.erase(actor);
}

int cosmeticBulletLocate(mobj_t *target,mobj_t *shooter,fixed_t x,fixed_t y,fixed_t z,fixed_t dx,fixed_t dy,fixed_t slope,wp_impact_t *out) {
    if(!settings.accelerated||!settings.monsterMetalSparks||!target||!shooter||
        (target->type!=MT_BABY&&target->type!=MT_SPIDER)||!P_WeakpointActive()||
        (criticalVoxelModel&&criticalVoxelModel(*target)))return 0;
    if(prepareCriticalSurface)prepareCriticalSurface(target,shooter);
    auto found=surfaces.find(target);if(found==surfaces.end())return 0;
    mobj_t origin=*shooter;origin.x=x;origin.y=y;
    float length=std::hypot(units(dx),units(dy)),distance=0;
    if(length<=0)return 0;
    doom_wounds::Point point{};const auto &s=found->second;
    if(!surfaceHit(s,origin,units(x),units(y),units(z),units(dx)/length,units(dy)/length,units(slope),point,&distance))return 0;
    const auto &image=*s.image;const auto &mask=weakpointRegionMask(target->type,image);
    *out={};out->region=(wp_region_t)mask[(size_t)(int)point.v*image.width+(int)point.u];out->confidence=100;
    out->x=units(x)+units(dx)/length*distance;out->y=units(y)+units(dy)/length*distance;out->z=units(z)+units(slope)*distance;
    out->u=point.u;out->v=point.v;out->distance=distance;
    out->sprite=s.sprite;out->frame=s.frame;out->rotation=s.rotation;out->flipped=s.flipped;
    return 1;
}

int weakpointLocate(mobj_t *projectile,mobj_t *target,wp_impact_t *out) {
    *out={};
    if(!settings.accelerated||(!settings.weaponVulnerabilities&&!settings.monsterMetalSparks)||!projectile||!projectile->target||!target||
        (criticalVoxelModel&&criticalVoxelModel(*target)))return 0;
    float start[3],end[3];if(!P_WeakpointTrajectory(projectile,start,end))return 0;
    float dx=end[0]-start[0],dy=end[1]-start[1],length=std::hypot(dx,dy);
    if(length<1e-4f)return 0;
    float slope=(end[2]-start[2])/length;dx/=length;dy/=length;
    auto found=surfaces.find(target);
    if((found==surfaces.end()||found->second.viewer!=projectile->target||leveltime-found->second.tic>2)&&prepareCriticalSurface)
        prepareCriticalSurface(target,projectile->target);
    found=surfaces.find(target);if(found==surfaces.end()||found->second.viewer!=projectile->target)return 0;
    const auto &surface=found->second;const auto &image=*surface.image;
    mobj_t origin=*projectile;origin.x=(fixed_t)std::lround(start[0]*FRACUNIT);origin.y=(fixed_t)std::lround(start[1]*FRACUNIT);
    doom_wounds::Point point{};float distance=0;
    float centerZ=start[2]+units(projectile->height)*0.5f;
    if(!surfaceHit(surface,origin,start[0],start[1],centerZ,dx,dy,slope,point,&distance))return 0;
    // Actor collision happens on the near side of its broad cylinder. Only
    // reconstruct through that admitted contact volume; never cast an
    // unrestricted ray to a body part beyond this actual actor collision.
    if(distance<0||distance>length+units(target->radius)+units(projectile->radius)+1)return 0;
    const auto &mask=weakpointRegionMask(target->type,image);
    auto sample=[&](float u,float v) {
        int x=(int)std::floor(u),y=(int)std::floor(v);
        return x<0||y<0||x>=image.width||y>=image.height?WP_UNKNOWN:(wp_region_t)mask[(size_t)y*image.width+x];
    };
    wp_region_t region=sample(point.u,point.v);
    if(region==WP_UNKNOWN)return 0;
    int opaque=0;bool competing=false;float footprint=std::max(0.5f,units(projectile->radius)*0.5f);
    for(auto offset:{doom_wounds::Point{0,0},{-footprint,0},{footprint,0},{0,-footprint},{0,footprint}}) {
        auto part=sample(point.u+offset.u,point.v+offset.v);
        if(part!=WP_UNKNOWN)++opaque;
        if(region>=WP_HEAD&&region<=WP_LEGS&&part>=WP_HEAD&&part<=WP_LEGS&&part!=region)competing=true;
    }
    // Ambiguous face/chassis/leg overlaps cannot execute or immobilize.
    // The center determines the contact location; a rocket can cover a
    // small eye and surrounding ordinary face pixels. Reject sparse alpha
    // support or overlap with a *different* critical part (brain vs legs),
    // rather than requiring the entire projectile to fit inside an eye.
    if(opaque<2||competing)return 0;
    out->region=region;out->confidence=100;out->u=point.u;out->v=point.v;out->distance=distance;
    out->x=start[0]+dx*distance;out->y=start[1]+dy*distance;out->z=centerZ+slope*distance;
    out->sprite=surface.sprite;out->frame=surface.frame;out->rotation=surface.rotation;out->flipped=surface.flipped;
    return 1;
}

int criticalBegin(mobj_t *shooter,fixed_t x,fixed_t y,fixed_t z,fixed_t dx,fixed_t dy,fixed_t slope,fixed_t range) {
    bulletImpacts.clear();bulletResults.clear();bulletShooter=nullptr;
    if(!settings.accelerated||(!settings.criticalParts&&!settings.enhancedWeakPoints)||!shooter||range<=0)return 0;
    bulletShooter=shooter;bulletX=x;bulletY=y;bulletZ=z;
    bulletDx=dx;bulletDy=dy;bulletSlope=slope;bulletRange=range;
    return 1;
}
int criticalThing(mobj_t *target,critical_hit_t *result) {
    if(!bulletShooter||!target||target==bulletShooter||target->player||target->health<=0||!(target->flags&MF_SHOOTABLE))return 0;
    bool human=P_WeakpointHuman(target->type)!=0;
    if(!(human?settings.criticalParts:settings.enhancedWeakPoints&&P_WeakpointProfile(target->type)) ||
       (criticalVoxelModel&&criticalVoxelModel(*target)))return 0;
    auto cached=bulletResults.find(target);
    if(cached!=bulletResults.end()){*result=cached->second;return 1;}
    *result={};result->fraction=-1;result->multiplier=1;result->percent=100;
    auto drawn=surfaces.find(target);
    if((drawn==surfaces.end()||drawn->second.viewer!=bulletShooter||leveltime-drawn->second.tic>2)&&prepareCriticalSurface)
        prepareCriticalSurface(target,bulletShooter);
    SpriteImpact impact;
    // surfaceHit reconstructs its origin from shooter XY. Use the actual
    // collision-checked peek origin while retaining the real viewer identity.
    mobj_t origin=*bulletShooter;origin.x=bulletX;origin.y=bulletY;
    auto surface=surfaces.find(target);
    bool hit=false;
    if(surface!=surfaces.end()&&surface->second.viewer==bulletShooter&&leveltime-surface->second.tic<=2) {
        float length=std::hypot(units(bulletDx),units(bulletDy));
        if(length>0) {
            impact.image=surface->second.image;impact.flipped=surface->second.flipped;
            impact.sourceYaw=surface->second.yaw;impact.viewYaw=std::atan2(surface->second.rx,-surface->second.ry);
            const auto &s=surface->second;
            float dx=units(bulletDx)/length,dy=units(bulletDy)/length;
            float ox=s.x-units(bulletX),oy=s.y-units(bulletY),along=ox*dx+oy*dy;
            float reach=std::max(std::abs((float)s.image->left),std::abs((float)(s.image->width-s.image->left)));
            if(s.posed)reach+=s.image->height*(std::abs(s.pose.lean[0])+std::abs(s.pose.lean[1])+std::abs(s.pose.whip[0])+std::abs(s.pose.whip[1]));
            if(std::abs(ox*dy-oy*dx)<=reach&&along>=-reach&&along<=units(bulletRange)+reach)
                hit=surfaceHit(s,origin,units(bulletX),units(bulletY),units(bulletZ),
                    dx,dy,units(bulletSlope),impact.point,&impact.distance);
        }
    }
    if(hit&&impact.distance>=0&&impact.distance<=units(bulletRange)) {
        result->fraction=(fixed_t)std::lround(impact.distance/units(bulletRange)*FRACUNIT);
        const auto &im=*impact.image;
        if(human) {
            auto head=heads.find(impact.image);
            if(head==heads.end()) {
                HeadProfile h{};
                h.found=doom_joints::findHead(im.pixels.data(),im.width,im.height,im.left,im.top,h.head);
                head=heads.emplace(impact.image,h).first;
            }
            const auto &h=head->second;
            float across=impact.point.u-im.left,height=im.top-impact.point.v;
            if(h.found&&height>=h.head.neck&&std::abs(across-h.head.middle)<=h.head.width/2) {
                impact.part=SpriteImpact::Head;impact.multiplier=2;
            }
        }
        result->impact.region=impact.part==SpriteImpact::Head?WP_HEAD:WP_BODY;
        if(!human) {
            const auto &mask=weakpointRegionMask(target->type,im);
            int x=(int)std::floor(impact.point.u),y=(int)std::floor(impact.point.v);
            result->impact.region=(wp_region_t)mask[(size_t)y*im.width+x];
            result->impact.eyeContact=weakpointRegions(target->type,im).eyes[(size_t)y*im.width+x]!=0;
        }
        result->impact.confidence=100;result->impact.u=impact.point.u;result->impact.v=impact.point.v;
        result->impact.sprite=surface->second.sprite;result->impact.frame=surface->second.frame;result->impact.flipped=impact.flipped;
        result->impact.rotation=surface->second.rotation;
        result->impact.x=units(bulletX)+units(bulletDx)/std::hypot(units(bulletDx),units(bulletDy))*impact.distance;
        result->impact.y=units(bulletY)+units(bulletDy)/std::hypot(units(bulletDx),units(bulletDy))*impact.distance;
        result->impact.z=units(bulletZ)+units(bulletSlope)*impact.distance;
        result->impact.distance=impact.distance;
        result->percent=human?impact.multiplier*100:P_WeakpointRegionPercent(target->type,P_WeakpointBulletKind(),result->impact.region,100);
        result->multiplier=human?impact.multiplier:result->percent/100;
        bulletImpacts[target]=impact;
    }
    bulletResults[target]=*result;return 1;
}

void woundHit(mobj_t *target,fixed_t fx,fixed_t fy,fixed_t fz,fixed_t fdx,fixed_t fdy,fixed_t slope,int damage,const mobj_t *shooter) {
    if(!settings.wounds)return;
    SpriteImpact impact;
    auto captured=bulletImpacts.find(target);
    if(p_critical_trace&&shooter==bulletShooter&&captured!=bulletImpacts.end())impact=captured->second;
    else if(!captureSpriteHit(target,fx,fy,fz,fdx,fdy,slope,shooter,impact))return;
    const Image &image=*impact.image;auto point=impact.point;
    bool flipped=impact.flipped;float viewYaw=impact.viewYaw,sourceYaw=impact.sourceYaw;
    float height=image.top-point.v,across=flipped?image.width-image.left-point.u:point.u-image.left;
    uint32_t seed=hash32((uint32_t)std::lround(point.u*16)*73856093u^(uint32_t)std::lround(point.v*16)*19349663u^
                         (uint32_t)leveltime*83492791u^(uint32_t)target->type*2654435761u);
    Wound wound={height,across,std::clamp(0.7f+damage/14.0f,0.8f,1.8f),viewYaw-sourceYaw,&image,point,flipped,seed,{},leveltime};
    auto &list=wounds[target];
    // Compare holes on this drawing, so hits on a moving limb still merge.
    auto distance=[&](Wound &w) {
        if(w.kind!=Wound::Kind::Bullet)return 1e9f;
        float facing=std::cos(w.yaw-wound.yaw);
        if(facing<0.8f)return 1e9f;
        auto at=projected(w,image,flipped,facing);
        return std::hypot(at.u-point.u,at.v-point.v);
    };
    for(Wound &w:list) {
        if(distance(w)<(w.radius+wound.radius)*0.9f) {
            auto at=projected(w,image,flipped,std::cos(w.yaw-wound.yaw));
            doom_wounds::Point middle={(at.u+point.u)*0.5f,(at.v+point.v)*0.5f};
            if(!opaqueAt(image,middle))continue;
            wound.radius=std::min(maxRadius,std::sqrt(w.radius*w.radius+wound.radius*wound.radius));
            wound.point=middle;wound.height=image.top-middle.v;
            wound.across=flipped?image.width-image.left-middle.u:middle.u-image.left;
            wound.seed=w.seed;
            w=std::move(wound);
            return;
        }
    }
    if(list.size()>=maxWounds) {
        auto nearest=std::min_element(list.begin(),list.end(),[&](Wound &p,Wound &q) {return distance(p)<distance(q);});
        if(nearest->kind!=Wound::Kind::Bullet)return;
        nearest->radius=std::min(maxRadius,nearest->radius+0.3f);nearest->tic=leveltime;
        return;
    }
    list.push_back(wound);
}
bool sawWoundHit(mobj_t *target,mobj_t *shooter,float &z) {
    if(!settings.blood||!settings.accelerated||!target||target->health<=0||!shooter||!shooter->player||
       !P_WeakpointActive()||!p_wp_vulnerabilities||
       (target->type!=MT_SERGEANT&&target->type!=MT_SHADOWS)||
       (criticalVoxelModel&&criticalVoxelModel(*target)))return false;
    if(prepareCriticalSurface)prepareCriticalSurface(target,shooter);
    auto surface=surfaces.find(target);
    if(surface==surfaces.end()||surface->second.viewer!=shooter||surface->second.tic!=leveltime)return false;
    // The saw has already made a gameplay contact. Its ray can pass through
    // transparent texels of a pain frame, so take the torso from the sprite.
    const auto &body=surface->second;
    const Image &image=*body.image;
    const auto point=sawTorso(image);
    if(point.u<0)return false;
    z=body.top-point.v;
    bool flipped=body.flipped;
    float height=image.top-point.v;
    float across=flipped?image.width-image.left-point.u:point.u-image.left;
    auto &list=wounds[target];
    auto existing=std::find_if(list.begin(),list.end(),[](const Wound &w){return w.kind==Wound::Kind::Saw;});
    if(existing!=list.end()) {
        existing->contacts=std::min<unsigned char>(5,existing->contacts+1);
        existing->stage=existing->contacts>=5?3:existing->contacts>=3?2:1;
        existing->yaw=std::atan2(body.rx,-body.ry)-body.yaw;
        existing->tic=leveltime;
        return true;
    }
    if(list.size()>=maxWounds)list.erase(std::min_element(list.begin(),list.end(),[](const Wound &a,const Wound &b){return a.tic<b.tic;}));
    Wound wound={height,across,0.8f,std::atan2(body.rx,-body.ry)-body.yaw,&image,point,flipped,
        hash32(uint32_t(target->x)^hash32(uint32_t(target->y))^uint32_t(leveltime)),{},leveltime};
    wound.kind=Wound::Kind::Saw;wound.stage=1;wound.contacts=1;
    list.push_back(std::move(wound));
    return true;
}
void forgetWounds(const mobj_t *thing) {
    wounds.erase(thing);surfaces.erase(thing);bulletImpacts.erase(thing);bulletResults.erase(thing);
    forgetGore(thing,0);
}
int woundSpots(const mobj_t *thing,WoundSpot *out,int most,bool includeCritical) {
    int count=0;
    if(includeCritical&&most>0&&thing->health>0&&goreLivingCavity(thing->type)&&
       goreActive()&&settings.goreBodyDamage) {
        const auto wound=goreLiving.find(thing);
        if(wound!=goreLiving.end()&&wound->second.region!=GoreRegion::Unknown&&goreSpriteActor(*thing)) {
            out[count]=wound->second.drip;out[count++].critical=true;
        }
    }
    auto found=wounds.find(thing);
    if(found==wounds.end())return count;
    for(const Wound &w:found->second) {
        if(count>=most)break;
        out[count++]={w.height,w.across,w.yaw,w.radius,w.tic};
    }
    return count;
}
void woundsShutdown() {
    wounds.clear();surfaces.clear();lyings.clear();standing.clear();bulletImpacts.clear();bulletResults.clear();
    goreShots={};goreDischarge=0;goreShooter=nullptr;goreCorpses.clear();
    goreFrameMasks.clear();verifiedGoreFrames.clear();goreLiving.clear();verifiedLivingFrames.clear();
    heads.clear();regionMasks.clear();bulletShooter=nullptr;
}

// How tall a monster stands, by its first drawing, to tell lying death drawings.
static float standingTop(const mobj_t &thing) {
    auto found=standing.find(thing.type);
    if(found!=standing.end())return found->second;
    float top=0;
    int state=thing.info->spawnstate;
    if(state>0&&states[state].sprite>=0&&states[state].sprite<numsprites) {
        const auto &sprite=sprites[states[state].sprite];int frame=states[state].frame&FF_FRAMEMASK;
        if(frame<sprite.numframes)top=(float)lumpImage(firstspritelump+sprite.spriteframes[frame].lump[0],false).top;
    }
    return standing[thing.type]=top;
}

static const Lying &lyingOf(const Image &image) {
    auto found=lyings.find(&image);
    if(found!=lyings.end())return found->second;
    Lying &l=lyings[&image];
    int w=image.width,h=image.height;
    l.middle.assign(w,-1);
    double upper=0,lower=0;int upperCount=0,lowerCount=0;
    for(int c=0;c<w;++c) {
        int top=-1,bottom=-1;
        for(int r=0;r<h;++r) if(image.pixels[((size_t)r*w+c)*2+1]) {
            if(top<0)top=r;bottom=r;
            if(r<h/2){upper+=c;++upperCount;}else{lower+=c;++lowerCount;}
        }
        if(top<0)continue;
        if(l.first<0)l.first=c;l.last=c;
        l.middle[c]=(top+bottom+1)*0.5f;
    }
    l.headRight=!upperCount||!lowerCount||upper/upperCount>=lower/lowerCount;
    return l;
}

// Wound-local marks, held for the wound's lifetime: a narrow dark torn
// edge, ragged red flesh, a few brighter lip texels, and a mottled stain.
// The short trail runs down the source drawing and bends with the body.
static byte woundMark(float x,float y,float radius,uint32_t seed) {
    float d=std::hypot(x,y);
    if(d<=radius)return 1;
    if(d<=radius+0.55f)return 2;
    float angle=std::atan2(y,x),phase=(seed&1023u)*(2*doomPi/1024);
    float edge=radius+1.15f+0.35f*std::sin(3*angle+phase)+0.2f*std::sin(7*angle-phase);
    uint32_t cell=hash32(seed^(uint32_t)(int)std::lround(x)*374761393u^(uint32_t)(int)std::lround(y)*668265263u);
    float grain=(cell&255u)/255.0f;
    if(d<=edge) {
        // A small broken highlight along the upper lip, never a shiny ring.
        return y<-0.25f&&x<0.4f&&d>edge-0.7f&&grain>0.35f?4:3;
    }
    byte stain=d<=edge+0.8f&&grain>0.3f?5:0;
    float length=radius+2.5f+(seed%3u)*0.5f;
    if(y>radius*0.5f&&y<length) {
        float progress=(y-radius*0.5f)/(length-radius*0.5f);
        float middle=((int)((seed>>10)&3u)-1.5f)*0.25f+std::sin(y*0.8f+phase)*0.2f;
        if(std::abs(x-middle)<0.35f*(1-progress)+0.55f)return progress<0.75f?3:5;
    }
    return stain;
}
static byte sawMark(float x,float y,unsigned char stage,uint32_t seed) {
    float length=stage==1?9.0f:stage==2?15.0f:23.0f;
    float phase=(seed&1023u)*(2*doomPi/1024);
    float t=y+length*0.5f;
    if(t<0||t>length)return 0;
    float center=std::sin(t*0.72f+phase)*1.0f+std::sin(t*1.73f+phase*0.7f)*0.4f;
    uint32_t grain=hash32(seed^(uint32_t)(int)std::lround(x)*374761393u^(uint32_t)(int)std::lround(y)*668265263u);
    float width=(stage==1?1.25f:stage==2?1.7f:2.15f)*(0.52f+0.48f*std::sin(t*float(doomPi)/length));
    float edge=std::abs(x-center);
    if(edge<width)return edge>width*0.68f?2:((grain&7u)==0?4:3);
    if(t>length*0.48f&&edge<width+1.55f&&(grain&3u)!=0)return 5;
    if(t>length*0.60f&&std::abs(x-center-2.1f)<0.65f)return 3;
    return 0;
}

// Apply after ordinary wound masks, so an intact head cannot reappear on
// later death frames. No texture rewriting and no extra GPU resources.
static bool goreCorpseMask(const mobj_t &actor,const Image &image,std::vector<byte> &mask) {
    if(!goreActive()||settings.deathRagdoll||actor.health>0||
       (actor.flags&MF_NOBLOOD)||image.pixels.empty())return false;
    const auto corpse=goreCorpses.find(&actor);
    if(corpse==goreCorpses.end())return false;
    const auto verified=verifiedGoreFrames.find(&image);
    if(verified==verifiedGoreFrames.end()||!verified->second||verified->second->type!=actor.type)return false;
    const auto outcome=corpse->second;
    if(!settings.goreHeads)return false;
    const auto &art=*verified->second;
    const int w=image.width,h=image.height;
    if(w<=0||h<=0)return false;
    auto &masks=goreFrameMasks[&image];
    auto cached=masks.find(outcome);
    if(cached==masks.end()&&outcome==GoreOutcome::HeadBurst&&goreHuman(actor.type)) {
        std::vector<byte> cut((size_t)w*h,0);
        const bool lying=image.top<standingTop(actor)*.85f;
        const Lying *body=lying?&lyingOf(image):nullptr;
        if(body&&body->first<0)return false;
        auto profile=heads.find(&image);
        doom_joints::Head head{};
        bool found=profile!=heads.end()&&profile->second.found;
        if(found)head=profile->second.head;
        else if(!lying) {
            profile=heads.emplace(&image,HeadProfile{}).first;
            profile->second.found=doom_joints::findHead(image.pixels.data(),w,h,image.left,image.top,profile->second.head);
            if(profile->second.found){head=profile->second.head;found=true;}
        }
        if(!found&&!lying) {
            // Transitional fall frames sometimes lack visible shoulders. This
            // conservative estimate stays confined to the upper silhouette.
            int first=h;
            for(int r=0;r<h;++r)for(int c=0;c<w;++c)
                if(image.pixels[((size_t)r*w+c)*2+1])first=std::min(first,r);
            if(first>=h)return false;
            head={float(image.top-first-std::clamp(h/5,6,12)),
                  float(w/2-image.left),float(std::clamp(w/3,8,17))};
            found=true;
        }
        for(int r=0;r<h;++r)for(int c=0;c<w;++c) {
            size_t i=(size_t)r*w+c;
            if(!image.pixels[i*2+1])continue;
            bool remove=false,rim=false;
            if(lying) {
                const int span=body->last+1-body->first;
                if(span<12)continue;
                const int width=std::max(3,(int)std::lround(span*.19f));
                const int dist=body->headRight?body->last-c:c-body->first;
                remove=dist<width-1;
                rim=dist>=width-1&&dist<=width+1;
            } else if(found) {
                const float above=image.top-r-.5f;
                const float across=c-image.left+.5f-head.middle;
                const float width=std::max(3.f,head.width*.62f);
                if(std::abs(across)>width+1.f)continue;
                remove=above>head.neck+1.f&&std::abs(across)<width;
                rim=above>=head.neck-1.5f&&above<=head.neck+2.f;
            }
            if(remove)cut[i]=1;
            else if(rim)cut[i]=((c+2*r)%5==0)?4:3;
        }
        cached=masks.emplace(outcome,std::move(cut)).first;
    } else if(cached==masks.end()) {
        const auto &box=art.head;
        if(!box.right)return false;
        std::vector<byte> cut((size_t)w*h,0);
        for(int r=box.top;r<box.bottom;++r)for(int c=box.left;c<box.right;++c) {
            const size_t i=(size_t)r*w+c;
            if(image.pixels[i*2+1])cut[i]=1;
        }
        cached=masks.emplace(outcome,std::move(cut)).first;
    }
    if(mask.size()!=(size_t)w*h)mask.assign((size_t)w*h,0);
    bool any=false;
    const auto &cut=cached->second;
    for(size_t i=0;i<cut.size();++i) {
        if(!cut[i])continue;
        any=true;
        if(cut[i]==1)mask[i]=1;
        else if(mask[i]!=1)mask[i]=cut[i];
    }
    return any;
}

static bool goreLiveMask(const mobj_t &actor,const Image &image,std::vector<byte> &mask) {
    if(!goreActive()||!settings.goreBodyDamage||actor.health<=0||(actor.flags&(MF_CORPSE|MF_NOBLOOD)))return false;
    const auto wound=goreLiving.find(&actor);
    if(wound==goreLiving.end()||wound->second.region==GoreRegion::Unknown)return false;
    const auto *art=goreLivingArt(actor.type,image);
    if(!art)return false;
    const bool belly=wound->second.region==GoreRegion::Belly;
    const auto outcome=belly?GoreOutcome::BellyCavity:GoreOutcome::ChestCavity;
    auto &masks=goreFrameMasks[&image];
    auto cached=masks.find(outcome);
    const int w=image.width,h=image.height;
    if(cached==masks.end()) {
        std::vector<byte> cut((size_t)w*h,0);
        const int cx=belly?art->cavities.bellyX:art->cavities.chestX;
        const int cy=belly?art->cavities.bellyY:art->cavities.chestY;
        const auto radii=goreCavityRadii(actor.type);
        const auto &anatomy=weakpointRegions(actor.type,image);
        const auto torso=[&](int x,int y) {
            if(x<0||x>=w||y<0||y>=h)return false;
            const size_t i=(size_t)y*w+x;
            return image.pixels[i*2+1]&&anatomy.pixels[i]==WP_BODY;
        };
        for(int r=0;r<h;++r)for(int c=0;c<w;++c) {
            const size_t i=(size_t)r*w+c;
            if(!torso(c,r))continue;
            const float dx=(c+.5f-cx)/radii[0],dy=(r+.5f-cy)/radii[1];
            const uint32_t grain=hash32((uint32_t)i^art->pixels.hash);
            const float distance=dx*dx+dy*dy+(grain&7u)*.032f;
            // Keep the opening inside the torso and paint a dark recess around it.
            if(distance<.26f&&torso(c-2,r)&&torso(c+2,r)&&torso(c,r-2)&&torso(c,r+2))cut[i]=1;
            else if(distance<.69f)cut[i]=2;
            else if(distance<1.f)cut[i]=(grain%5u==0)?4:3;
            else if(distance<1.25f&&(grain&1u))cut[i]=5;
        }
        cached=masks.emplace(outcome,std::move(cut)).first;
    }
    if(mask.size()!=(size_t)w*h)mask.assign((size_t)w*h,0);
    bool any=false;
    const auto &cut=cached->second;
    for(size_t i=0;i<cut.size();++i)
        if(cut[i]&&mask[i]!=1){mask[i]=cut[i];any=true;}
    return any;
}

static bool goreMask(const mobj_t &actor,const Image &image,std::vector<byte> &mask) {
    if(image.pixels.empty())return false;
    return actor.health>0?goreLiveMask(actor,image,mask):goreCorpseMask(actor,image,mask);
}

bool woundMask(const mobj_t &thing,const Image &image,bool flipped,float rx,float ry,std::vector<byte> &mask) {
    auto found=wounds.find(&thing);
    if((!settings.wounds&&!settings.blood)||found==wounds.end()||found->second.empty()||image.pixels.empty())
        return goreMask(thing,image,mask);
    bool dead=thing.health<=0;
    // Death drawings face the viewer; lying ones carry the holes along the body.
    float tall=standingTop(thing);
    bool lying=dead&&image.top<0.85f*tall;
    const Lying *body=lying?&lyingOf(image):nullptr;
    if(lying&&body->first<0)return goreMask(thing,image,mask);
    float a=(float)(thing.angle*(2*doomPi/4294967296.0));
    int w=image.width,h=image.height;
    mask.assign((size_t)w*h,0);
    bool any=false;
    for(Wound &wound:found->second) {
        const bool saw=wound.kind==Wound::Kind::Saw;
        if(saw?(!settings.blood||!p_wp_vulnerabilities||!P_WeakpointActive()):!settings.wounds)continue;
        float facing=std::sin(a+wound.yaw)*rx-std::cos(a+wound.yaw)*ry;
        if(!dead&&(saw?facing<0.18f:std::abs(facing)<0.4f))continue;
        // Procedural bends then carry these source texels through the same
        // bodyQuads/fallQuads deformation as the rest of the drawing.
        auto point=(dead||saw)?attached(wound,image):projected(wound,image,flipped,facing);
        if(point.u<0||point.v<0)continue;
        float cu=point.u,cv=point.v;
        if(lying&&!saw) {
            float along=std::clamp(wound.height/std::max(1.0f,tall),0.05f,0.95f)*(body->last+1-body->first);
            cu=body->headRight?body->first+along:body->last+1-along;
            int column=std::clamp((int)cu,body->first,body->last);
            if(body->middle[column]<0)continue;
            cv=body->middle[column]-wound.across*0.3f;
        }
        int c0=(int)std::floor(cu),r0=(int)std::floor(cv);
        if(c0<0||r0<0||c0>=w||r0>=h||!image.pixels[((size_t)r0*w+c0)*2+1])continue;   // missed the body here
        const doom_regions::Mask *sawAnatomy=nullptr;
        if(saw) {
            sawAnatomy=&weakpointRegions(thing.type,image);
            if(!sawAnatomy->stock)continue;
        }
        int reach=saw?5:(int)std::ceil(wound.radius+2.5f);
        int up=saw?12:reach;
        int down=saw?12:(int)std::ceil(wound.radius+4);
        float direction=dead?1.0f:((wound.flipped!=flipped)?-1.0f:1.0f)*(facing<0?-1.0f:1.0f);
        for(int r=std::max(0,r0-up);r<=std::min(h-1,r0+down);++r)
            for(int c=std::max(0,c0-reach);c<=std::min(w-1,c0+reach);++c) {
                size_t i=(size_t)r*w+c;
                if(!image.pixels[i*2+1]||mask[i]==1)continue;
                if(sawAnatomy&&(sawAnatomy->pixels[i]!=WP_BODY||c<w/5||c>w*4/5||r<h/10||r>h*5/6))continue;
                byte mark=saw?
                    sawMark((c+0.5f-cu)*direction,r+0.5f-cv,wound.stage,wound.seed):
                    woundMark((c+0.5f-cu)*direction,r+0.5f-cv,wound.radius,wound.seed);
                // Nearby stains combine, but never paint over a hole or its
                // recessed edge. Highlights win over the red lip and stain.
                static const byte priority[6]={0,5,4,2,3,1};
                if(priority[mark]>priority[mask[i]])mask[i]=mark;
                any=any||mark!=0;
            }
    }
    return goreMask(thing,image,mask)||any;
}

void woundVertex(Vertex &vertex,byte state) {
    if(state<2||state>5)return;
    const auto &shades=bloodPalette();
    if(shades.empty())return;
    static const float shade[6]={0,0,0.02f,0.45f,0.82f,0.22f};
    byte color=shades[(size_t)std::lround(shade[state]*(shades.size()-1))];
    vertex.mode&=((1u<<MODE_WOUND_PALETTE_SHIFT)-1u)&~(MODE_GLOWING_EYES|MODE_WET_SPRITE);
    vertex.mode|=MODE_WOUND|((unsigned)color<<MODE_WOUND_PALETTE_SHIFT);
    if(state==5)vertex.mode|=MODE_WOUND_STAIN;
    else if(settings.bloodShine&&(state==3||state==4))vertex.mode|=MODE_WET_SPRITE;
}

// One texel row cut by the mask: runs of body texels, holes skipped, rim runs
// painted with the wound material. corner(edge, bottom) gives the vertex at the
// texture column edge (0..width) on the row's top or bottom; flip says the
// texture runs right to left on screen.
void maskedRow(std::vector<Vertex> &out,const std::vector<byte> &mask,int width,int r,bool flip,
               const std::function<Vertex(int,bool)> &corner) {
    int c=0;
    while(c<width) {
        byte state=mask[(size_t)r*width+c];
        int end=c+1;
        while(end<width&&mask[(size_t)r*width+end]==state)++end;
        if(state!=1) {
            int a=flip?end:c,b=flip?c:end;    // left to right on screen
            Vertex v[4]={corner(a,false),corner(b,false),corner(b,true),corner(a,true)};
            if(state>=2)for(Vertex &vertex:v)woundVertex(vertex,state);
            quad(out,v[0],v[1],v[2],v[3]);
        }
        c=end;
    }
}
static bool rowWounded(const std::vector<byte> &mask,int width,int r) {
    for(int c=0;c<width;++c)if(mask[(size_t)r*width+c])return true;
    return false;
}

void woundedQuads(std::vector<Vertex> &out,const Vertex &base,const Image &image,const std::vector<byte> &mask,
                  float x,float y,float top,float rx,float ry,bool flip,float rowHeight) {
    int w=image.width,h=image.height;
    float left=-(float)image.left,right=left+w;
    auto at=[&](int edge,float z,float v) {
        float across=flip?right-edge:left+edge;
        Vertex vertex=base;vertex.x=x+rx*across;vertex.y=y+ry*across;vertex.z=z;vertex.u=(float)edge;vertex.v=v;
        return vertex;
    };
    // Unwounded rows go out as whole blocks, wounded ones as runs.
    int block=-1;
    auto flush=[&](int end) {
        if(block<0)return;
        int a=flip?w:0,b=flip?0:w;
        quad(out,at(a,top-block*rowHeight,(float)block),at(b,top-block*rowHeight,(float)block),
             at(b,top-end*rowHeight,(float)end),at(a,top-end*rowHeight,(float)end));
        block=-1;
    };
    for(int r=0;r<h;++r) {
        if(!rowWounded(mask,w,r)) {if(block<0)block=r;continue;}
        flush(r);
        maskedRow(out,mask,w,r,flip,[&](int edge,bool bottom){return at(edge,top-(r+(bottom?1:0))*rowHeight,(float)(r+(bottom?1:0)));});
    }
    flush(h);
}
} // namespace doom3d

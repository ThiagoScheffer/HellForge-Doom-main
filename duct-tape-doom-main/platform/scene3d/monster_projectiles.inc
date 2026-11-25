// Render-only profiles, using the same masked alpha/additive batches and
// particle representation as rocket fire. No actors, gameplay RNG or shaders.
enum class MonsterProjectileStyle { Fire, Energy, Molten, Plasma, SquareFire };
struct MonsterProjectileProfile {
    int type;MonsterProjectileStyle style;
    std::array<unsigned,5> palette; // edge, outer, middle, inner, core
    float radius,stretch,tailBodies,lightRadius,lightStrength;
};
// FX radii scale visible body/trails/impact particles; actor collision is unchanged.
static constexpr MonsterProjectileProfile monsterProjectileProfiles[]={
    {MT_TROOPSHOT,MonsterProjectileStyle::Fire,{0xA92313,0xF04A17,0xFF641D,0xFF9A28,0xFFF2BA},12,1.1f,1.25f,88,.65f},
    {MT_HEADSHOT,MonsterProjectileStyle::Energy,{0x9E1C38,0xC52A65,0x7135B8,0x3457D5,0xA8C7FF},14,1,1,96,.6f},
    {MT_FATSHOT,MonsterProjectileStyle::Molten,{0x561B14,0xB72815,0xFF641D,0xFFB536,0xFFF3C4},14,1.9f,2.5f,136,.85f},
    {MT_ARACHPLAZ,MonsterProjectileStyle::Plasma,{0x087C45,0x19DF5B,0x19DF5B,0x79FF57,0xE7FFD0},10,1,1.5f,72,.55f},
    // Baron of Hell and Hell Knight share this Doom projectile actor.
    {MT_BRUISERSHOT,MonsterProjectileStyle::SquareFire,{0x064B22,0x0C9E3C,0x20E44E,0x80FF5B,0xE3FFC2},11.5f,1,1.4f,100,.70f}
};
static std::array<float,3> projectileColor(unsigned rgb){
    return {float((rgb>>16)&255)/255,float((rgb>>8)&255)/255,float(rgb&255)/255};
}
static bool monsterProjectileEnabled(){
    // Projectile types select the effect, so supported Doom II/compatible IWADs
    // should not be excluded by a Doom I-only WAD check.
    return settings.accelerated&&settings.fluidMonsters&&
           !netgame&&!demoplayback&&!demorecording;
}
static const MonsterProjectileProfile *findMonsterProjectileProfile(int type){
    for(const auto &p:monsterProjectileProfiles)if(p.type==type)return &p;
    return nullptr;
}
static const MonsterProjectileProfile *monsterProjectileProfile(int type){
    return monsterProjectileEnabled()?findMonsterProjectileProfile(type):nullptr;
}
// Enhanced monster-projectile FX are intentionally local to the player.
// 512 world units is eight classic 64-unit map blocks.
/*
Distance	Value
8 blocks (current)	512.0f
9 blocks	576.0f
10 blocks	640.0f
12 blocks	768.0f
*/
static constexpr float monsterProjectileFxRange=640.0f;
static bool monsterProjectileNearPlayer(float x,float y,float z){
    if(displayplayer<0||displayplayer>=MAXPLAYERS)return false;
    const mobj_t *player=players[displayplayer].mo;
    if(!player)return false;
    float dx=x-units(player->x),dy=y-units(player->y);
    float dz=z-(units(player->z)+units(player->height)*.5f);
    return dx*dx+dy*dy+dz*dz<=monsterProjectileFxRange*monsterProjectileFxRange;
}
static bool monsterProjectileTargetsPlayer(const mobj_t &mo){
    // A missile's target is its shooter; the shooter's target is its victim.
    // Monster-vs-monster infighting therefore keeps the stock projectile only.
    const mobj_t *shooter=mo.target;
    const mobj_t *victim=shooter?shooter->target:nullptr;
    return victim&&victim->player;
}
static bool monsterProjectileFxAllowed(const mobj_t &mo,const MonsterProjectileProfile *p,float x,float y,float z){
    return p&&!voxelMonster(mo)&&monsterProjectileTargetsPlayer(mo)&&monsterProjectileNearPlayer(x,y,z);
}
static bool monsterProjectileFxAllowed(const mobj_t &mo,const MonsterProjectileProfile *p){
    return monsterProjectileFxAllowed(mo,p,units(mo.x),units(mo.y),units(mo.z)+units(mo.height)*.5f);
}
bool monsterProjectileFxManaged(int type){return monsterProjectileProfile(type)!=nullptr;}
bool monsterProjectileFxEligible(const mobj_t &mo){
    const auto *p=monsterProjectileProfile(mo.type);
    return monsterProjectileFxAllowed(mo,p);
}
struct MonsterProjectileDraw {const MonsterProjectileProfile *profile;lab::Vec center,velocity;float phase,distance;};
struct MoltenGlass {float x,y,z,radius,born,life;int sector;uint32_t seed;unsigned supportRevision;};
struct MonsterImpactStamp {int type,tic;lab::Vec point;};
struct ThermalWallMelt {
    int line,side,section,projectileType,bornTic;
    float u,relativeZ,nx,ny,radius,born,life;
    uint32_t seed;
    unsigned supportRevision=~0u;
    float supportLow=0.f,supportHigh=0.f;
};
static std::vector<MonsterProjectileDraw> monsterProjectileDraws;
static std::vector<FluidParticle> monsterProjectileMotes;
static std::vector<MoltenGlass> moltenGlass;
static std::vector<MonsterImpactStamp> monsterImpactStamps;
static std::vector<ThermalWallMelt> thermalWallMelts;
static constexpr size_t maxThermalWallMelts=48;
static constexpr float thermalWallLife=15.f;
static unsigned monsterProjectileLevel=~0u;
static int monsterEmissionTic=-1,monsterEmissionCount=0;
static bool thermalWallMeltEnabled(){
    return settings.accelerated&&settings.fireballWallMelt&&settings.fireballWallMeltIntensity>0.f&&
           gamestate==GS_LEVEL&&!netgame&&!demoplayback&&!demorecording;
}
static float thermalWallIntensity(){
    return std::clamp(settings.fireballWallMeltIntensity/100.f,0.f,1.f);
}
static void clearFluidMonsterProjectiles(){
    monsterProjectileDraws.clear();monsterProjectileMotes.clear();moltenGlass.clear();monsterImpactStamps.clear();
    monsterEmissionTic=-1;monsterEmissionCount=0;
}
void clearMonsterProjectileEffects(){
    clearFluidMonsterProjectiles();
    clearThermalWallMelts();
    monsterProjectileLevel=r_levelserial;
}
void clearThermalWallMelts(){thermalWallMelts.clear();}
static void resetMonsterProjectileEffects(){
    if(monsterProjectileLevel!=r_levelserial)clearMonsterProjectileEffects();
    if(!monsterProjectileEnabled())clearFluidMonsterProjectiles();
    if(!thermalWallMeltEnabled())clearThermalWallMelts();
}
static constexpr size_t maxMonsterProjectileDraws=96;
void beginMonsterProjectileFrame(){
    // Allocate only when an eligible projectile is actually close enough to
    // need the enhanced effect; distant fights keep this queue empty.
    resetMonsterProjectileEffects();monsterProjectileDraws.clear();
}
// Cached image is recreated with the normal image cache, never held by pointer.
// The decal shader already supports smooth coverage and tinted emissive quads.
static int monsterGlowImage(){
    constexpr int lump=100000220,key=-1-lump,size=64;
    if(!images.count(key)){
        std::array<byte,size*size*2> pixels{};
        for(int y=0;y<size;++y)for(int x=0;x<size;++x){
            float r=std::hypot((x+.5f-size*.5f)/(size*.5f),(y+.5f-size*.5f)/(size*.5f));
            float a=std::clamp(1-r*r,0.f,1.f);pixels[(y*size+x)*2+1]=byte(255*a*a);
        }
        images.emplace(key,upload(size,size,pixels.data()));
    }
    return lump;
}
// Square, irregularly feathered fire coverage, separate from the round plasma
// mask so the Baron/Hell Knight body never becomes a spherical glow sprite.
// Cached once alongside the existing projectile image; no external assets.
static int monsterSquareFireImage(){
    constexpr int lump=100000221,key=-1-lump,size=64;
    if(!images.count(key)){
        std::array<byte,size*size*2> pixels{};
        for(int y=0;y<size;++y)for(int x=0;x<size;++x){
            float ax=std::abs((x+.5f-size*.5f)/(size*.5f));
            float ay=std::abs((y+.5f-size*.5f)/(size*.5f));
            // A fourth-order superellipse rounds the corners while retaining
            // four broad flat sides; never use the circular glow mask here.
            // Restrained deterministic ripples keep the green-fire silhouette alive.
            float edge=std::pow(std::pow(ax,4.f)+std::pow(ay,4.f),.25f);
            float ripple=.020f*std::sin(ay*13.f+ax*3.5f)+.016f*std::sin(ax*19.f-ay*4.f);
            float a=std::clamp((.935f+ripple-edge)/.17f,0.f,1.f);
            a=a*a*(3.f-2.f*a);
            pixels[(y*size+x)*2+1]=byte(255*a);
        }
        images.emplace(key,upload(size,size,pixels.data()));
    }
    return lump;
}
static void projectileQuad(const Uniforms &camera,lab::Vec at,float width,float height,
                           float roll,const std::array<float,3> &color,float alpha,bool additive,
                           bool squareFire=false,bool floorAligned=false){
    if(width<=0||height<=0||alpha<=0)return;
    float depth=(at.x-camera.eye[0])*camera.forward[0]+(at.y-camera.eye[1])*camera.forward[1]+(at.z-camera.eye[2])*camera.forward[2];
    if(depth<.5f)return;
    float c=std::cos(roll),s=std::sin(roll);
    auto v=[&](float x,float y,float u,float t){
        float rx=c*x-s*y,ry=s*x+c*y;
        Vertex out=floorAligned?Vertex{at.x+rx,at.y+ry,at.z,u,t,alpha,0}:
            Vertex{at.x+camera.right[0]*rx+camera.up[0]*ry,at.y+camera.right[1]*rx+camera.up[1]*ry,
                   at.z+camera.right[2]*rx+camera.up[2]*ry,u,t,alpha,0};
        out.red=color[0];out.green=color[1];out.blue=color[2];return out;
    };
    int image=squareFire?monsterSquareFireImage():monsterGlowImage();
    auto &out=additive?effectAddBatches[image]:effectBatches[image];
    quad(out,v(-width,height,0,0),v(width,height,64,0),v(width,-height,64,64),v(-width,-height,0,64));
}
void drawHellfireGlow(const Uniforms &camera,lab::Vec at,float width,float height,
                      float roll,const std::array<float,3> &color,float alpha,bool additive,bool floorAligned) {
    projectileQuad(camera,at,width,height,roll,color,alpha,additive,false,floorAligned);
}
bool monsterProjectileVisual(const mobj_t &mo,const Uniforms &camera,float fraction){
    const auto *p=monsterProjectileProfile(mo.type);
    if(!p||!mo.info||!(mo.info->flags&MF_MISSILE)||camera.effects[2]!=0||voxelMonster(mo))return false;
    // Impact is rendered by the confirmed explosion hook, not by a second sprite.
    if(!(mo.flags&MF_MISSILE))return true;
    float x,y,z;interpolatedPosition(mo,fraction,x,y,z);z+=units(mo.height)*.5f;
    if(!monsterProjectileFxAllowed(mo,p,x,y,z))return false; // stock sprite remains as fallback
    if(monsterProjectileDraws.size()>=maxMonsterProjectileDraws)return false;
    float distance=std::hypot(std::hypot(x-camera.eye[0],y-camera.eye[1]),z-camera.eye[2]);
    monsterProjectileDraws.push_back({p,{x,y,z},{units(mo.momx),units(mo.momy),units(mo.momz)},
        x*.037f+y*.023f+float(mo.angle)*(2.f*doomPi/4294967296.f),distance});
    return true;
}
bool monsterProjectileLight(const mobj_t &mo,std::array<float,3> &color,float &radius,float &strength){
    const auto *p=monsterProjectileProfile(mo.type);if(!p||!monsterProjectileFxAllowed(mo,p))return false;
    color=projectileColor(p->palette[2]);radius=p->lightRadius;strength=p->lightStrength;return true;
}
static void monsterMote(lab::Vec at,lab::Vec velocity,const std::array<float,3> &color,float size,float life,float gravity=0){
    if(monsterEmissionTic!=leveltime){monsterEmissionTic=leveltime;monsterEmissionCount=0;}
    if(monsterEmissionCount>=48||monsterProjectileMotes.size()>=256)return;
    // Let the mote vector grow only when a nearby eligible projectile emits.
    monsterProjectileMotes.push_back({at.x,at.y,at.z,velocity.x,velocity.y,velocity.z,gravity,size,-size/life,.8f,0,life,15,true,color});
    ++monsterEmissionCount;
}
bool monsterProjectileTrail(const mobj_t &mo){
    const auto *p=monsterProjectileProfile(mo.type);
    if(!p||!(mo.flags&MF_MISSILE)||!monsterProjectileFxAllowed(mo,p))return false;
    resetMonsterProjectileEffects();
    // Only small embers/droplets persist; the smooth main tail follows the
    // interpolated actor directly, with no actor pointers to outlive removal.
    int interval=p->style==MonsterProjectileStyle::Molten?2:3;
    if(leveltime%interval==0){
        int count=p->style==MonsterProjectileStyle::Molten?2:1;
        for(int i=0;i<count;++i)monsterMote({units(mo.x)+randomRange(-2,2),units(mo.y)+randomRange(-2,2),units(mo.z)+units(mo.height)*.5f},
            {-units(mo.momx)*.08f+randomRange(-.2f,.2f),-units(mo.momy)*.08f+randomRange(-.2f,.2f),randomRange(-.1f,.3f)},
            projectileColor(p->palette[p->style==MonsterProjectileStyle::Energy?2:3]),p->style==MonsterProjectileStyle::Molten?2.8f:1.4f,8,p->style==MonsterProjectileStyle::Molten?-.08f:0);
    }
    return true;
}
static bool moltenFloorSupported(float x,float y,float floor,float radius,const sector_t *sector){
    // A small flat footprint must be supported throughout. No hovering across
    // ledges, liquid borders, slopes, destroyed terrain or actor-only contacts.
    for(int i=0;i<9;++i){
        float angle=i*doomPi*.25f,px=x+(i?std::cos(angle)*radius:0),py=y+(i?std::sin(angle)*radius:0);
        auto *under=R_PointInSubsector(fixed_t(px*FRACUNIT),fixed_t(py*FRACUNIT))->sector;
        if(under!=sector||SV_Edited(px,py,floor)||SV_Edited(px,py,floor-2))return false;
    }
    return true;
}
static void addMoltenGlass(float x,float y,float z,float nz){
    if(nz<.85f||sectorMist.size()!=size_t(numsectors))return;
    auto *sector=R_PointInSubsector(fixed_t(x*FRACUNIT),fixed_t(y*FRACUNIT))->sector;
    int index=int(sector-sectors);float floor=units(sector->floorheight),radius=11;
    if(index<0||index>=numsectors||sectorMist[index].liquid||sector->floorpic==skyflatnum||std::abs(z-floor)>2)return;
    if(!moltenFloorSupported(x,y,floor,radius,sector))return;
    for(const auto &old:moltenGlass)if(old.sector==index&&gameSeconds()-old.born<.1f&&std::hypot(old.x-x,old.y-y)<12)return;
    if(moltenGlass.capacity()<24)moltenGlass.reserve(24);
    if(moltenGlass.size()>=24)moltenGlass.erase(moltenGlass.begin());
    const auto *world=SV_MapWorld();
    moltenGlass.push_back({x,y,floor,radius,gameSeconds(),2.4f,index,uint32_t(moltenGlass.size()*977+leveltime),world?world->revision():0});
}
bool monsterProjectileSurfaceImpact(int type,float x,float y,float z,float nx,float ny,float nz,const mobj_t *projectile){
    const auto *p=monsterProjectileProfile(type);if(!p)return false;
    resetMonsterProjectileEffects();
    // rubbleImpact has no actor/source pointer. Consume that duplicate callback;
    // the owner-aware monsterProjectileImpact hook creates the effect if eligible.
    if(!projectile||!monsterProjectileFxAllowed(*projectile,p,x,y,z))return true;
    if(p->style==MonsterProjectileStyle::Molten)addMoltenGlass(x,y,z,nz);
    lab::Vec at{x,y,z};
    monsterImpactStamps.erase(std::remove_if(monsterImpactStamps.begin(),monsterImpactStamps.end(),[](const auto &s){return s.tic!=leveltime;}),monsterImpactStamps.end());
    for(const auto &s:monsterImpactStamps)if(s.type==type&&lab::length(s.point-at)<32)return true;
    if(monsterImpactStamps.size()>=64)return true;
    if(monsterImpactStamps.capacity()<64)monsterImpactStamps.reserve(64);
    monsterImpactStamps.push_back({type,leveltime,at});
    environmentBlast(x+nx*3,y+ny*3,z+nz*3,type==MT_ROCKET||type==MT_BFG?1.f:.25f);
    lab::Vec normal{nx,ny,nz};at=at+normal*1.5f;
    auto color=projectileColor(p->palette[3]);
    if(settings.emissive)sparkFlash(at.x,at.y,at.z,p->lightRadius*.7f,.65f,color);
    monsterMote(at,normal*.1f,projectileColor(p->palette[4]),p->radius*3,5);
    monsterMote(at,normal*.08f,projectileColor(p->palette[2]),p->radius*4,10);
    for(int i=0;i<(p->style==MonsterProjectileStyle::Molten?10:6);++i){
        lab::Vec v{randomRange(-1.2f,1.2f),randomRange(-1.2f,1.2f),randomRange(-1.2f,1.2f)};
        float dot=lab::dot(v,normal);if(dot<0)v=v-normal*(2*dot);
        auto fragment=p->style==MonsterProjectileStyle::Energy?projectileColor(p->palette[i%2?1:2]):color;
        monsterMote(at,v+normal*.6f,fragment,p->style==MonsterProjectileStyle::Molten?3:1.6f,12,p->style==MonsterProjectileStyle::Molten?-.12f:0);
    }
    return true;
}
bool monsterProjectileImpact(mobj_t *mo){
    if(!mo)return false;
    const auto *p=monsterProjectileProfile(mo->type);if(!p)return false;
    if(!monsterProjectileFxAllowed(*mo,p))return true; // recognized, deliberately no custom impact FX
    float x=units(mo->x),y=units(mo->y),z=units(mo->z)+units(mo->height)*.5f;
    lab::Vec v{units(mo->momx),units(mo->momy),units(mo->momz)},n=lab::normalized(v)*-1;
    // P_ExplodeMissile calls us before clearing momentum. A grounded contact
    // has already clamped z/momz in P_ZMovement; unknown actor hits get no pool.
    float previousX,previousY,previousZ;interpolatedPosition(*mo,0,previousX,previousY,previousZ);
    if(mo->subsector&&mo->momz==0&&mo->z==mo->floorz&&previousZ>units(mo->floorz)+.5f){
        z=units(mo->floorz);n={0,0,1};
    }
    return monsterProjectileSurfaceImpact(mo->type,x,y,z,n.x,n.y,n.z,mo);
}
static float thermalBaseRadius(int type){return type==MT_FATSHOT?20.f:13.f;}

static float addThermalWallMelt(const WallHit &hit,float u,float relativeZ,float nx,float ny,int type){
    for(const auto &mark:thermalWallMelts)
        if(mark.bornTic==leveltime&&mark.line==hit.line&&mark.side==hit.side&&mark.section==hit.section&&
           std::abs(mark.u-u)<1.f&&std::abs(mark.relativeZ-relativeZ)<1.f)return 0.f;

    float radius=thermalBaseRadius(type)*(.6f+.4f*thermalWallIntensity());
    uint32_t seed=hash32(uint32_t(type)*2654435761u^uint32_t(hit.line));
    seed=hash32(seed^uint32_t(leveltime)^uint32_t(hit.side*4+hit.section)*2246822519u);
    seed=hash32(seed^uint32_t(std::lround(u*4))*73856093u^uint32_t(std::lround(relativeZ*4))*19349663u);
    if(thermalWallMelts.capacity()<maxThermalWallMelts)
        thermalWallMelts.reserve(maxThermalWallMelts);
    if(thermalWallMelts.size()==maxThermalWallMelts)thermalWallMelts.erase(thermalWallMelts.begin());
    thermalWallMelts.push_back({hit.line,hit.side,hit.section,type,leveltime,u,relativeZ,nx,ny,radius,gameSeconds(),thermalWallLife,seed});
    return radius;
}

void monsterProjectileWallMelt(mobj_t *mo){
    if(!mo||(mo->type!=MT_TROOPSHOT&&mo->type!=MT_FATSHOT)||!(mo->flags&MF_MISSILE)||
       (mo->flags&MF_NOCLIP)||!thermalWallMeltEnabled()||!mo->subsector||!monsterProjectileTargetsPlayer(*mo))return;
    if(!(mo->target->flags&MF_COUNTKILL))return;
    if((tmthing==mo&&p_missile_hit_actor)||mo->z<=mo->floorz||mo->z+mo->height>=mo->ceilingz)return;

    float x=units(mo->x),y=units(mo->y),z=units(mo->z);
    float center=z+units(mo->height)*.5f;
    if(!monsterProjectileNearPlayer(x,y,center))return;
    float vx=units(mo->momx),vy=units(mo->momy),vz=units(mo->momz),speed=std::hypot(vx,vy);
    if(speed<.01f)return;
    float dx=vx/speed,dy=vy/speed;

    // Failed classic moves leave the attempted position and collision box in p_map.
    // A spawn check has already advanced the missile by half a step.
    bool atSpawnStep=tmx==mo->x&&tmy==mo->y;
    auto matchesHalfStep=[](fixed_t step,fixed_t momentum){
        return step==momentum/2||step==(momentum>>1);
    };
    bool failedMove=tmthing==mo&&
        ((tmx-mo->x==mo->momx&&tmy-mo->y==mo->momy)||
         (matchesHalfStep(tmx-mo->x,mo->momx)&&matchesHalfStep(tmy-mo->y,mo->momy))||atSpawnStep);
    float reach,traceZ=center,slope=0.f;
    float solidHit[6];
    if(failedMove){
        if(atSpawnStep){x-=units(mo->momx>>1);y-=units(mo->momy>>1);}
        reach=std::hypot(units(tmx)-x,units(tmy)-y)+units(mo->radius)*(std::abs(dx)+std::abs(dy))+.01f;
    }else{
        // Solid collision stops the missile about .2 units outside its contact.
        // Confirm that nearby contact with the same trace used by SV_Missile.
        if(SV_Trace(x,y,z,vx,vy,vz,solidHit,nullptr)>=1.f||std::abs(solidHit[5])>=.5f)return;
        float separation=(x-solidHit[0])*solidHit[3]+(y-solidHit[1])*solidHit[4]+(z-solidHit[2])*solidHit[5];
        if(std::abs(separation-.2f)>.06f)return;
        reach=std::hypot(solidHit[0]-x,solidHit[1]-y)+1.f;
        traceZ=z;
        slope=vz/speed;
    }

    WallHit hit;
    if(!firstSolidWall(x,y,traceZ,dx,dy,slope,reach,hit,true))return;
    line_t &line=lines[hit.line];
    if(failedMove&&((!SV_Active()&&line.validcount!=validcount)||
       tmbbox[BOXRIGHT]<=line.bbox[BOXLEFT]||tmbbox[BOXLEFT]>=line.bbox[BOXRIGHT]||
       tmbbox[BOXTOP]<=line.bbox[BOXBOTTOM]||tmbbox[BOXBOTTOM]>=line.bbox[BOXTOP]||
       P_BoxOnLineSide(tmbbox,&line)!=-1))return;

    WallSpan span;
    if(!wallSpan(line,hit.side,hit.section,span,true)||span.texture<=0||span.texture==skytexture)return;
    float hx=x+dx*hit.t,hy=y+dy*hit.t,hz=traceZ+slope*hit.t;
    float ex=span.b.x-span.a.x,ey=span.b.y-span.a.y,len=std::hypot(ex,ey);
    if(len<.01f||hz<=span.bottom||hz>=span.top||!monsterProjectileNearPlayer(hx,hy,hz))return;
    float nx=ey/len,ny=-ex/len;
    if(!failedMove&&(std::hypot(hx-solidHit[0],hy-solidHit[1])>1.f||nx*solidHit[3]+ny*solidHit[4]<.8f))return;
    if(SV_Edited(hx,hy,hz))return;

    resetMonsterProjectileEffects();
    float u=std::clamp(((hx-span.a.x)*ex+(hy-span.a.y)*ey)/len,0.f,len);
    float radius=addThermalWallMelt(hit,u,hz-span.anchor,nx,ny,mo->type);
    if(radius>0.f&&settings.emissive){
        const auto &palette=findMonsterProjectileProfile(mo->type)->palette;
        sparkFlash(hx+nx,hy+ny,hz,radius*2.5f,.5f*thermalWallIntensity(),projectileColor(palette[3]));
    }
}

static float thermalSmooth(float t){
    t=std::clamp(t,0.f,1.f);
    return t*t*(3.f-2.f*t);
}

// Returns true when the original wall face has lost support under the mark.
static bool drawThermalWallMelt(ThermalWallMelt &mark,float age,const lab::MapWorld *world){
    WallSpan span;
    if(mark.line<0||mark.line>=numlines)return true;
    if(!wallSpan(lines[mark.line],mark.side,mark.section,span))return false; // Wait for a moving wall to expose this span again.
    if(span.texture<=0||span.texture==skytexture)return true;
    float dx=span.b.x-span.a.x,dy=span.b.y-span.a.y,len=std::hypot(dx,dy);
    if(len<.01f||mark.u>len)return true;
    float tx=dx/len,ty=dy/len,zc=span.anchor+mark.relativeZ;
    float x=span.a.x+tx*mark.u,y=span.a.y+ty*mark.u;
    float radius=mark.radius,scale=radius/thermalBaseRadius(mark.projectileType);
    bool mancubus=mark.projectileType==MT_FATSHOT;
    float maxDrip=(mancubus?42.f:26.f)*scale;
    float left=std::max(0.f,mark.u-radius*1.45f),right=std::min(len,mark.u+radius*1.45f);
    float low=std::max(span.bottom,zc-maxDrip-radius*.55f),high=std::min(span.top,zc+radius*1.2f);
    if(left>=right||low>=high)return false;

    float excavation[4];
    if(P_GlobalDigBounds(mark.line,mark.side,excavation)&&
       left<excavation[1]&&right>excavation[0]&&low<excavation[3]&&high>excavation[2])return true;
    if(world&&(mark.supportRevision!=world->revision()||low-zc<mark.supportLow||high-zc>mark.supportHigh)){
        // Sample the entire clipped footprint, including the potential drip paths.
        if(zc>=span.bottom&&zc<=span.top&&world->removal({x,y,zc})>0.f)return true;
        for(int i=0;i<=8;++i){
            float u=left+(right-left)*i/8.f;
            for(int j=0;j<=8;++j){
                float z=low+(high-low)*j/8.f;
                if(world->removal({span.a.x+tx*u,span.a.y+ty*u,z})>0.f)return true;
            }
        }
        mark.supportRevision=world->revision();
        mark.supportLow=low-zc;
        mark.supportHigh=high-zc;
    }
    if(!monsterProjectileNearPlayer(x,y,zc))return false;

    int mask=monsterSquareFireImage(),glow=monsterGlowImage();
    auto rect=[&](int image,bool additive,float u,float z,float halfWidth,float halfHeight,
                  const std::array<float,3> &color,float alpha,bool flip=false){
        if(alpha<=0.f||halfWidth<=0.f||halfHeight<=0.f)return;
        float u0=std::max(0.f,u-halfWidth),u1=std::min(len,u+halfWidth);
        float z0=std::max(span.bottom,z-halfHeight),z1=std::min(span.top,z+halfHeight);
        if(u0>=u1||z0>=z1)return;
        auto vertex=[&](float across,float height){
            float s=32.f+32.f*(across-u)/halfWidth,t=32.f-32.f*(height-z)/halfHeight;
            Vertex v={span.a.x+tx*across+mark.nx*.25f,span.a.y+ty*across+mark.ny*.25f,
                      height,flip?64.f-s:s,t,alpha,0};
            v.red=color[0];v.green=color[1];v.blue=color[2];
            return v;
        };
        auto &batch=additive?effectAddBatches[image]:decalBatches[image];
        quad(batch,vertex(u0,z1),vertex(u1,z1),vertex(u1,z0),vertex(u0,z0));
    };

    const auto &palette=findMonsterProjectileProfile(mark.projectileType)->palette;
    float fade=1.f-thermalSmooth((age-8.f)/(mark.life-8.f));
    float flash=1.f-thermalSmooth(age/.4f);
    float heat=(1.f-thermalSmooth((age-.4f)/4.4f))*thermalWallIntensity();
    float spread=.75f+.25f*thermalSmooth(age/.65f);
    auto charred=projectileColor(0x211617),edge=projectileColor(palette[0]);
    for(size_t c=0;c<charred.size();++c)charred[c]+=(edge[c]*.55f-charred[c])*heat;

    rect(mask,false,mark.u,zc,radius*1.16f*spread,radius*.95f*spread,charred,.85f*fade,mark.seed&1u);
    rect(glow,true,mark.u,zc,radius*1.45f*spread,radius*1.2f*spread,projectileColor(palette[1]),heat*.85f);
    rect(mask,true,mark.u,zc,radius*.83f*spread,radius*.61f*spread,projectileColor(palette[2]),heat*.75f,mark.seed&1u);
    rect(glow,true,mark.u,zc,radius*.62f,radius*.49f,projectileColor(palette[3]),heat*(.65f+.30f*flash));
    rect(glow,true,mark.u,zc,radius*.35f,radius*.28f,projectileColor(palette[4]),heat*(.22f+.78f*flash));

    int drips=(mancubus?2:1)+int((mark.seed>>1)&1u);
    for(int i=0;i<drips;++i){
        float offset=((i+.5f)/drips-.5f)*1.5f+(hashUnit(i,1,mark.seed)-.5f)*.28f;
        float delay=.10f+.45f*hashUnit(i,2,mark.seed);
        float length=maxDrip*(.62f+.38f*hashUnit(i,3,mark.seed))*thermalSmooth((age-delay)/2.3f);
        if(length<=0.f)continue;
        float width=radius*(.08f+.07f*hashUnit(i,4,mark.seed));
        float u=mark.u+radius*offset,top=zc-radius*.30f,tip=top-length;
        float dripHeat=heat*(.70f+.30f*hashUnit(i,5,mark.seed));
        rect(mask,false,u,top-length*.5f,width*1.2f,length*.5f,charred,.72f*fade,i%2!=0);
        rect(glow,true,u,top-length*.5f,width,length*.5f,projectileColor(palette[2]),dripHeat);
        rect(glow,true,u,tip+width,width*.85f,width*1.2f,projectileColor(palette[3]),dripHeat*.85f);
    }
    return false;
}

void tickMonsterProjectileEffects(){
    tickGreenImpactFires();
    resetMonsterProjectileEffects();
    float now=gameSeconds();
    thermalWallMelts.erase(std::remove_if(thermalWallMelts.begin(),thermalWallMelts.end(),[&](const auto &mark){
        return now-mark.born>=mark.life;
    }),thermalWallMelts.end());
    for(auto &p:monsterProjectileMotes){++p.age;p.x+=p.vx;p.y+=p.vy;p.z+=p.vz;p.vz+=p.az;p.size+=p.growth;}
    monsterProjectileMotes.erase(std::remove_if(monsterProjectileMotes.begin(),monsterProjectileMotes.end(),[](const auto &p){
        return p.age>=p.life||p.size<=0||!monsterProjectileNearPlayer(p.x,p.y,p.z);
    }),monsterProjectileMotes.end());
}
void drawMonsterProjectileEffects(const Uniforms &camera){
    drawGreenImpactFires(camera);
    resetMonsterProjectileEffects();float now=gameSeconds();
    // Written after drawFluidEffects clears the shared batches, not from the
    // actor loop. Per-frame requests contain values, never actor/image pointers.
    std::sort(monsterProjectileDraws.begin(),monsterProjectileDraws.end(),[](const auto &a,const auto &b){return a.distance>b.distance;});
    for(const auto &d:monsterProjectileDraws){
        if(!monsterProjectileNearPlayer(d.center.x,d.center.y,d.center.z))continue;
        const auto &p=*d.profile;float t=now*6+d.phase;
        bool squareFire=p.style==MonsterProjectileStyle::SquareFire;
        // Modest distance compensation: capped at 1.35x, without changing hitboxes.
        // Keep the bright body readable, not only its additive glow.
        float distanceScale=1.f+.35f*std::clamp((d.distance-900.f)/1700.f,0.f,1.f);
        float r=p.radius*distanceScale*(1+.035f*std::sin(t));
        lab::Vec velocity=lab::normalized(d.velocity);
        float dx=lab::dot(velocity,{camera.right[0],camera.right[1],camera.right[2]}),dy=lab::dot(velocity,{camera.up[0],camera.up[1],camera.up[2]});
        float roll=std::atan2(-dx,dy),stretch=1+(p.stretch-1)*std::hypot(dx,dy);
        float tail=2*r*p.stretch*p.tailBodies;int steps=d.distance>1000?3:7;
        for(int i=steps;i>0;--i){float f=float(i)/steps;
            lab::Vec center=d.center-velocity*(tail*f);
            float wave=.35f*std::sin(t-f*5)*f;center.x+=camera.right[0]*wave;center.y+=camera.right[1]*wave;center.z+=camera.right[2]*wave;
            projectileQuad(camera,center,r*(1-.75f*f),r*(1-.65f*f),roll,projectileColor(p.palette[i%2?1:2]),.22f*(1-f*.8f),true,squareFire);
        }
        // The halo stays soft; larger masked layers form the actual visible body.
        projectileQuad(camera,d.center,r*1.8f,r*1.8f*stretch,roll,projectileColor(p.palette[0]),.2f,true);
        static constexpr std::array<float,5> bodyScale={1.25f,1.05f,.85f,.70f,.50f};
        static constexpr std::array<float,5> bodyAlpha={.48f,.72f,.78f,.85f,.95f};
        for(int layer=0;layer<5;++layer){
            // At long range keep the main color, intermediate energy, and bright core.
            if(d.distance>1400&&layer!=1&&layer!=2&&layer!=4)continue;
            float scale=bodyScale[layer];
            lab::Vec center=d.center;
            if(p.style==MonsterProjectileStyle::Energy&&layer>=2){float swirl=now*1.6f+d.phase+layer*.7f;
                center=center+lab::Vec{camera.right[0],camera.right[1],camera.right[2]}*(std::cos(swirl)*r*.07f)+
                    lab::Vec{camera.up[0],camera.up[1],camera.up[2]}*(std::sin(swirl)*r*.07f);}
            if(p.style==MonsterProjectileStyle::Plasma)scale*=1+.025f*std::sin(t*2.3f+layer*1.7f);
            if(squareFire)scale*=1+.025f*std::sin(t*2.1f+layer*1.2f); // restrained living flame
            projectileQuad(camera,center,r*scale,r*scale*stretch,roll,projectileColor(p.palette[layer]),bodyAlpha[layer],false,squareFire);
        }
    }
    for(const auto &p:monsterProjectileMotes){float f=renderFraction-1;
        float x=p.x+p.vx*f,y=p.y+p.vy*f,z=p.z+p.vz*f;
        if(!monsterProjectileNearPlayer(x,y,z))continue;
        projectileQuad(camera,{x,y,z},p.size,p.size,0,p.color,p.alpha*std::clamp(1-(p.age+f)/p.life,0.f,1.f),true);
    }
    const auto *world=SV_MapWorld();
    for(auto mark=thermalWallMelts.begin();mark!=thermalWallMelts.end();){
        float age=now-mark->born;
        if(age>=mark->life||drawThermalWallMelt(*mark,std::max(0.f,age),world))mark=thermalWallMelts.erase(mark);
        else ++mark;
    }
    moltenGlass.erase(std::remove_if(moltenGlass.begin(),moltenGlass.end(),[&](auto &p){
        if(now<p.born||now-p.born>=p.life||p.sector<0||p.sector>=numsectors||units(sectors[p.sector].floorheight)!=p.z||
           !monsterProjectileNearPlayer(p.x,p.y,p.z))return true;
        if(world&&p.supportRevision!=world->revision()){
            p.supportRevision=world->revision();
            if(!moltenFloorSupported(p.x,p.y,p.z,p.radius,&sectors[p.sector]))return true;
        }
        return false;
    }),moltenGlass.end());
    for(const auto &p:moltenGlass){
        float age=(now-p.born)/p.life,fade=std::clamp((1-age)*5,0.f,1.f),z=floorZ(&sectors[p.sector])+.15f;
        auto hot=projectileColor(0xFF9A28),red=projectileColor(0x682019),cold=projectileColor(0x211B20),color=hot;
        for(int c=0;c<3;++c)color[c]=age<.45f?hot[c]+(red[c]-hot[c])*(age/.45f):red[c]+(cold[c]-red[c])*((age-.45f)/.55f);
        auto vertex=[&](float x,float y,const auto &tint,float alpha){Vertex v={p.x+x,p.y+y,z,32+x/p.radius*32,32-y/p.radius*32,alpha,0};v.red=tint[0];v.green=tint[1];v.blue=tint[2];return v;};
        int image=monsterGlowImage();auto &base=decalBatches[image];auto &glow=effectAddBatches[image];
        for(int i=0;i<12;++i){
            auto rim=[&](int j){float a=j*(2*doomPi/12),radius=p.radius*(.82f+.12f*std::sin(j*2.3f+p.seed));return Point{std::cos(a)*radius,std::sin(a)*radius};};
            auto a=rim(i),b=rim((i+1)%12);
            base.push_back(vertex(0,0,color,.92f*fade));base.push_back(vertex(a.x,a.y,color,.92f*fade));base.push_back(vertex(b.x,b.y,color,.92f*fade));
            // Narrow broken radial seams glow briefly, then cool with the pool.
            if(i%2==0&&age<.8f){float a0=i*(2*doomPi/12),a1=a0+.18f;float x=std::cos(a0)*p.radius*.65f,y=std::sin(a0)*p.radius*.65f;
                quad(glow,vertex(std::cos(a1)*2-.2f,std::sin(a1)*2,hot,(1-age)*.65f),vertex(x-.2f,y,hot,(1-age)*.65f),vertex(x+.2f,y,hot,(1-age)*.65f),vertex(std::cos(a1)*2+.2f,std::sin(a1)*2,hot,(1-age)*.65f));}
        }
    }
}

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
static const MonsterProjectileProfile *monsterProjectileProfile(int type){
    if(monsterProjectileEnabled())for(const auto &p:monsterProjectileProfiles)if(p.type==type)return &p;
    return nullptr;
}
// Enhanced monster-projectile FX are intentionally local to the player.
// 512 world units is eight classic 64-unit map blocks.
static constexpr float monsterProjectileFxRange=512.0f;
static bool monsterProjectileNearPlayer(float x,float y,float z){
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
static std::vector<MonsterProjectileDraw> monsterProjectileDraws;
static std::vector<FluidParticle> monsterProjectileMotes;
static std::vector<MoltenGlass> moltenGlass;
static std::vector<MonsterImpactStamp> monsterImpactStamps;
static unsigned monsterProjectileLevel=~0u;
static int monsterEmissionTic=-1,monsterEmissionCount=0;
void clearMonsterProjectileEffects(){
    monsterProjectileDraws.clear();monsterProjectileMotes.clear();moltenGlass.clear();monsterImpactStamps.clear();
    monsterEmissionTic=-1;monsterEmissionCount=0;monsterProjectileLevel=r_levelserial;
}
static void resetMonsterProjectileEffects(){
    if(monsterProjectileLevel!=r_levelserial||!monsterProjectileEnabled())clearMonsterProjectileEffects();
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
                           float roll,const std::array<float,3> &color,float alpha,bool additive,bool squareFire=false){
    if(width<=0||height<=0||alpha<=0)return;
    float depth=(at.x-camera.eye[0])*camera.forward[0]+(at.y-camera.eye[1])*camera.forward[1]+(at.z-camera.eye[2])*camera.forward[2];
    if(depth<.5f)return;
    float c=std::cos(roll),s=std::sin(roll);
    auto v=[&](float x,float y,float u,float t){
        float rx=c*x-s*y,ry=s*x+c*y;
        Vertex out={at.x+camera.right[0]*rx+camera.up[0]*ry,at.y+camera.right[1]*rx+camera.up[1]*ry,
                    at.z+camera.right[2]*rx+camera.up[2]*ry,u,t,alpha,0};
        out.red=color[0];out.green=color[1];out.blue=color[2];return out;
    };
    int image=squareFire?monsterSquareFireImage():monsterGlowImage();
    auto &out=additive?effectAddBatches[image]:effectBatches[image];
    quad(out,v(-width,height,0,0),v(width,height,64,0),v(width,-height,64,64),v(-width,-height,0,64));
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
void tickMonsterProjectileEffects(){
    resetMonsterProjectileEffects();
    for(auto &p:monsterProjectileMotes){++p.age;p.x+=p.vx;p.y+=p.vy;p.z+=p.vz;p.vz+=p.az;p.size+=p.growth;}
    monsterProjectileMotes.erase(std::remove_if(monsterProjectileMotes.begin(),monsterProjectileMotes.end(),[](const auto &p){
        return p.age>=p.life||p.size<=0||!monsterProjectileNearPlayer(p.x,p.y,p.z);
    }),monsterProjectileMotes.end());
}
void drawMonsterProjectileEffects(const Uniforms &camera){
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
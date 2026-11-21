// Uses the shared monster fire mask and render batches. Sprite drawing is
// independent, including DEHACKED frames and replacement FIRE/VILE patches.
namespace {
constexpr std::array<float,3> vileRed{.92f,.12f,.025f};
constexpr std::array<float,3> vileOrange{1.f,.37f,.053f};
constexpr std::array<float,3> vileGold{1.f,.82f,.32f};
struct VileBurst {
    lab::Vec at{};
    float angle=0;
    int tic=-1;
    uint32_t seed=0;
    bool impact=false;
};
std::array<VileBurst,12> vileBursts;
unsigned vileLevel=~0u;

int vileBurstLife(bool impact) {return impact?17:14;}
int vileCastStage(const mobj_t &vile) {
    return vile.state>=&states[S_VILE_ATK1]&&vile.state<=&states[S_VILE_ATK10]?
        int(vile.state-&states[S_VILE_ATK1])+1:0;
}
bool vileReach(lab::Vec p,const Uniforms &camera) {
    const float x=p.x-camera.eye[0],y=p.y-camera.eye[1],z=p.z-camera.eye[2];
    return x*x+y*y+z*z<=600.f*600.f;
}
void emitVileBurst(lab::Vec at,float angle,bool impact) {
    if(!vileEffectsEnabled()||gamestate!=GS_LEVEL)return;
    if(vileLevel!=r_levelserial)resetVileEffects();
    VileBurst *slot=nullptr;
    VileBurst *oldest=nullptr;
    for(auto &burst:vileBursts) {
        const int age=leveltime-burst.tic;
        if(burst.tic<0||age<0||age>=vileBurstLife(burst.impact)) {
            if(!slot)slot=&burst;
            continue;
        }
        // Nearby shotgun pellets and successive hits share one short flare.
        const auto d=burst.at-at;
        if(!impact&&!burst.impact&&lab::dot(d,d)<32.f*32.f)return;
        if(!oldest||burst.tic<oldest->tic)oldest=&burst;
    }
    if(!slot)slot=oldest;
    const uint32_t seed=hash32(uint32_t(int(at.x))*0x9e3779b9u^
                               uint32_t(int(at.y))*0x85157af5u^uint32_t(leveltime));
    *slot={at,angle,leveltime,seed,impact};
}
void vileSpark(const Uniforms &camera,lab::Vec at,float size,float alpha,bool hot) {
    pixelBlock(particleVertices,camera,at.x,at.y,at.z,size,1.45f,
               hot?vileGold:vileOrange,false,nullptr,false,nullptr,alpha);
}
}

bool vileEffectsEnabled() {return settings.accelerated&&settings.fluidMonsters;}
bool vileEffectNear(const mobj_t &actor,const Uniforms &camera) {
    return vileReach({units(actor.x),units(actor.y),units(actor.z)},camera);
}
void resetVileEffects() {
    vileBursts={};
    vileLevel=r_levelserial;
}
void vileHurt(const mobj_t *vile) {
    const float facing=float(double(vile->angle)*(2.0*doomPi/4294967296.0));
    emitVileBurst({units(vile->x),units(vile->y),units(vile->z)+units(vile->height)*.91f},facing,false);
}
void vileImpact(fixed_t x,fixed_t y,fixed_t z) {
    emitVileBurst({units(x),units(y),units(z)+9.f},0.f,true);
}

void drawVileFire(const mobj_t &fire,const mobj_t &vile,const mobj_t &victim,
                  const Uniforms &camera,float fraction) {
    if(vile.type!=MT_VILE||vile.health<=0||victim.health<=0||
       vile.tracer!=&fire||vile.target!=&victim)return;
    const int stage=vileCastStage(vile);
    if(stage<3)return;
    // The original actor moves through A_Fire. Never create a second flame at
    // the victim position when the game stopped moving or replaced that actor.
    if(std::hypot(units(fire.x)-units(victim.x),units(fire.y)-units(victim.y))>72.f||
       std::abs(units(fire.z)-units(victim.z))>40.f)return;
    lab::Vec base;
    interpolatedPosition(fire,fraction,base.x,base.y,base.z);
    const float t=(leveltime+fraction)/TICRATE;
    const float charge=std::clamp((stage-3+(8.f-vile.tics)/8.f)/7.f,0.f,1.f);
    const float range=lab::length(base-lab::Vec{camera.eye[0],camera.eye[1],camera.eye[2]});
    const float opacity=(.26f+.34f*charge)*std::clamp(range/62.f,.28f,1.f);
    for(int n=0;n<5;++n) {
        const float phase=t*(10.f+.88f*n)+n*2.41f;
        const float sway=std::sin(phase),flicker=.76f+.24f*std::cos(phase*1.23f);
        lab::Vec flame{base.x+7.f*std::cos(phase),base.y+7.f*sway,base.z+10.f+9.f*n};
        projectileQuad(camera,flame,5.4f+1.5f*charge,(8.f+7.f*charge)*flicker,
                       .38f*sway,n%2?vileOrange:vileRed,opacity*flicker,false);
        if(n<3)projectileQuad(camera,flame,2.6f,5.5f+4.f*charge,.28f*sway,
                             vileGold,opacity*.68f,true);
    }
    for(int n=0;n<8;++n) {
        const float age=std::fmod(t*(1.65f+.055f*n)+n*.381f,1.f);
        const float phase=n*2.41f+age*3.7f;
        const float spread=4.f+age*(6.f+5.f*charge);
        vileSpark(camera,{base.x+spread*std::cos(phase),base.y+spread*std::sin(phase),
            base.z+10.f+age*(44.f+11.f*charge)},.84f,opacity*(1.f-age),
            n%3==0);
    }
}

void drawVileHands(mobj_t &vile,const mobj_t *victim,const Uniforms &camera,float fraction) {
    if(!victim||victim->health<=0||vile.health<=0||!vile.info)return;
    if(vile.state>=&states[S_VILE_HEAL1]&&vile.state<=&states[S_VILE_HEAL3])return;
    const int cast=vileCastStage(vile);
    const bool frenzy=P_WeakpointFrenzied(&vile)!=0;
    const bool wounded=vile.info->spawnhealth>0&&int64_t(vile.health)*3<=vile.info->spawnhealth;
    if(!cast&&!frenzy&&!wounded)return;
    lab::Vec base;
    interpolatedPosition(vile,fraction,base.x,base.y,base.z);
    const float height=units(vile.height),radius=units(vile.radius);
    const float facing=float(double(vile.angle)*(2.0*doomPi/4294967296.0));
    const float forwardX=std::cos(facing),forwardY=std::sin(facing);
    const float t=(leveltime+fraction)/TICRATE;
    const float charge=(cast?.46f+.054f*cast:.23f)+(frenzy?.23f:wounded?.13f:0.f);
    // The stock windup raises the hands above the actor's collision height.
    // Follow that silhouette down and forward as the spell is released.
    const float extension=std::clamp((cast-3)/5.f,0.f,1.f);
    const float handZ=height*(cast?1.42f-.49f*extension:.91f);
    const float handForward=radius*(cast?.23f+1.16f*extension:.32f);
    for(int side:{-1,1}) {
        const float phase=t*(cast?13.f:8.f)+side*2.3f;
        const float flicker=.78f+.22f*std::sin(phase*1.33f);
        const float spread=side*(radius*.82f+1.7f*std::sin(phase));
        const lab::Vec hand{base.x+forwardX*handForward-forwardY*spread,
            base.y+forwardY*handForward+forwardX*spread,
            base.z+handZ+2.5f*std::sin(phase)};
        projectileQuad(camera,hand,4.4f,6.f+3.f*charge,.36f*std::cos(phase),
                       vileOrange,charge*.66f*flicker,false);
        projectileQuad(camera,hand,2.1f,3.6f,.1f,vileGold,charge*.55f*flicker,true);
        if(cast||frenzy) {
            const float age=std::fmod(t*2.2f+(side>0?.42f:0.f),1.f);
            vileSpark(camera,{hand.x-forwardY*side*age*6.f,hand.y+forwardX*side*age*6.f,
                hand.z+age*13.f},.91f,charge*(1.f-age),true);
        }
    }
}

void drawVileBursts(const Uniforms &camera,float fraction) {
    if(vileLevel!=r_levelserial)return;
    // Events are world-space snapshots. Sparks can finish their short arcs
    // after a removal, but no emitter stays attached to a removed object.
    for(const auto &burst:vileBursts) {
        const float age=leveltime-1-burst.tic+fraction;
        const int life=vileBurstLife(burst.impact);
        if(burst.tic<0||age>=life||age<-1.f||!vileReach(burst.at,camera))continue;
        const float t=std::max(0.f,age)/TICRATE;
        const float fade=1.f-std::max(0.f,age)/life;
        const float near=std::clamp(lab::length(burst.at-
            lab::Vec{camera.eye[0],camera.eye[1],camera.eye[2]})/72.f,.28f,1.f);
        if(burst.impact) {
            const float pulse=std::max(0.f,1.f-8.f*t)*near;
            projectileQuad(camera,burst.at+lab::Vec{0,0,11.f},19.f+20.f*t,
                           29.f+10.f*t,.25f,vileRed,pulse*.63f,true);
            projectileQuad(camera,burst.at+lab::Vec{0,0,9.f},9.f+9.f*t,
                           22.f,-.14f,vileGold,pulse*.84f,true);
            for(int side:{-1,1}) {
                const lab::Vec jet=burst.at+lab::Vec{side*(7.f+t*27.f),side*4.f,18.f+t*40.f};
                projectileQuad(camera,jet,6.f,21.f,side*.46f,vileOrange,pulse*.69f,true);
            }
        }
        for(int n=0;n<(burst.impact?14:8);++n) {
            const float phase=burst.angle+float(doomPi)*2.f*hashUnit(n,0,burst.seed);
            const float speed=(burst.impact?41.f:24.f)+38.f*hashUnit(n,1,burst.seed);
            const float radius=(burst.impact?3.f:9.f)+speed*t;
            const lab::Vec at{burst.at.x+radius*std::cos(phase),burst.at.y+radius*std::sin(phase),
                burst.at.z+t*(26.f+48.f*hashUnit(n,2,burst.seed)-58.f*t)};
            if(n<4)projectileQuad(camera,at,4.f+3.f*t,8.f+7.f*t,
                .42f*std::sin(phase),n%2?vileRed:vileOrange,fade*.70f*near,false);
            vileSpark(camera,at,1.1f,fade*.88f*near,n%3!=0);
        }
    }
}

#ifdef DOOM_TEST_DRIVER
extern "C" int I_TestVileHurtBursts() {
    if(vileLevel!=r_levelserial)return 0;
    return int(std::count_if(vileBursts.begin(),vileBursts.end(),[](const VileBurst &burst) {
        const int age=leveltime-burst.tic;
        return !burst.impact&&burst.tic>=0&&age>=0&&age<vileBurstLife(false);
    }));
}
#endif

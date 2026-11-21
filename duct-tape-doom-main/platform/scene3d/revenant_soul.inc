namespace {
constexpr int soulCapacity=24;
constexpr float soulLife=1.65f;
constexpr int soulImageKey=100000033,soulWidth=64,soulHeight=80;
struct RevenantSoul {
    float x=0,y=0,z=0;
    int born=-1,sector=-1;
    std::uint32_t seed=0;
};
std::array<RevenantSoul,soulCapacity> revenantSouls;
unsigned soulLevel=~0u;
int soulClockTic=-1;
float soulFraction=0.f;

float soulEase(float u) {
    u=std::clamp(u,0.f,1.f);
    return u*u*(3.f-2.f*u);
}
bool soulAlive(const RevenantSoul &s,float age) {
    return s.born>=0&&age>=0.f&&age<soulLife;
}
bool cacheSoulImage() {
    const int key=-1-soulImageKey;
    if(images.count(key))return true;
    if(SPR_SKEL>=numsprites||sprites[SPR_SKEL].numframes<=0)return false;
    const int patch=sprites[SPR_SKEL].spriteframes[0].lump[0];
    if(patch<0)return false;
    const auto &skel=lumpImage(firstspritelump+patch,false);
    std::array<byte,soulWidth*soulHeight*2> pixels{};
    auto cover=[&](int x,int y) {
        const float u=(x+.5f)/soulWidth,v=(y+.5f)/soulHeight;
        const float sway=.026f*std::sin(v*19.f)+.014f*std::sin(v*36.f);
        const int sx=int(std::floor((.5f+(u-.5f-sway)*1.12f)*skel.width));
        const int sy=int(std::floor((v*.94f)*skel.height));
        return sx>=0&&sx<skel.width&&sy>=0&&sy<skel.height?
            skel.pixels[2*(size_t(sy)*skel.width+sx)+1]/255.f:0.f;
    };
    for(int y=1;y<soulHeight-1;++y)for(int x=1;x<soulWidth-1;++x) {
        const float u=(x+.5f)/soulWidth,v=(y+.5f)/soulHeight;
        float torso=.28f*cover(x,y)+.12f*(cover(x-1,y)+cover(x+1,y)+cover(x,y-1)+cover(x,y+1))+
            .06f*(cover(x-2,y-1)+cover(x+2,y-1)+cover(x-2,y+1)+cover(x+2,y+1));
        const float haze=.80f+.11f*std::sin(u*28.f+v*13.f)+.08f*std::sin(u*61.f-v*22.f);
        torso*=haze*(1.f-soulEase((v-.47f)/.30f));
        // Retain the skull and shoulders. The lower body frays into a
        // tapering wake instead of retaining the sprite's legs and shading.
        const float lean=.09f*std::sin(v*14.f)+.035f*std::sin(v*34.f);
        const float width=.23f*(1.f-v)+.035f;
        const float tail=.65f*soulEase((v-.38f)/.19f)*soulEase((.99f-v)/.28f)*
            soulEase((width-std::abs(u-.5f-lean))/.09f);
        pixels[2*(y*soulWidth+x)+1]=byte(255.f*std::max(torso,tail));
    }
    images.emplace(key,upload(soulWidth,soulHeight,pixels.data()));
    return true;
}

void drawSoul(const Uniforms &camera,const RevenantSoul &s,float age) {
    const float phase=6.2831853f*hashUnit(0,0,s.seed);
    const float emerge=soulEase(age/.25f);
    const float resist=std::clamp((age-.25f)/.30f,0.f,1.f);
    const float held=std::sin(resist*float(doomPi));
    const float strain=held*(.71f*std::sin(resist*12.f)+.29f*std::sin(resist*31.f));
    const float lift=soulEase((age-.55f)/.70f);
    const float dissolve=soulEase((age-1.25f)/.40f);
    const float rise=8.f*emerge+3.f*resist+7.f*strain+48.f*lift+5.f*dissolve;
    const float drift=3.f*emerge+8.f*lift;
    const float x=s.x+drift*std::cos(phase+.62f*lift);
    const float y=s.y+drift*std::sin(phase+.62f*lift);
    float height=37.f+10.f*emerge+6.f*strain+4.f*lift;
    const float roof=sectors[s.sector].ceilingpic==skyflatnum?
        32768.f:ceilZ(&sectors[s.sector])-2.f;
    height=std::min(height,roof-s.z);
    if(height<8.f)return;
    const float base=std::min(s.z+rise,roof-height);
    const float width=35.f+9.f*emerge-3.f*strain;
    const float opacity=(.09f+.33f*emerge)*(.96f+.04f*std::sin(age*27.f+phase));
    constexpr std::array<float,3> color{.56f,.73f,.75f};
    auto &out=effectBatches[soulImageKey];
    auto ribbon=[&](float from,float to,int bands,float alpha,float spread,float climb,bool torn) {
        auto vertex=[&](int row,float edge) {
            const float u=float(row)/bands,v=from+(to-from)*u;
            const float flutter=std::sin(age*13.f+phase+v*13.f);
            const float trail=v*v;
            const float tremble=held*std::sin(age*91.f+v*4.f)*1.10f;
            const float across=edge*width*.5f*(1.f+.035f*flutter)+
                flutter*(1.f+trail*2.8f)+tremble+spread;
            const float z=std::min(roof,base+(1.f-v)*height-trail*lift*11.f+climb);
            const float edgeFade=torn?std::sin(float(doomPi)*u):1.f;
            Vertex p={x+camera.right[0]*across,y+camera.right[1]*across,z,
                      edge<0?.5f:soulWidth-.5f,.5f+v*(soulHeight-1),
                      alpha*std::max(0.f,edgeFade),0u};
            p.red=color[0];p.green=color[1];p.blue=color[2];return p;
        };
        auto left=vertex(0,-1.f),right=vertex(0,1.f);
        for(int i=1;i<=bands;++i) {
            const auto nextLeft=vertex(i,-1.f),nextRight=vertex(i,1.f);
            quad(out,left,right,nextRight,nextLeft);
            left=nextLeft;right=nextRight;
        }
    };
    const float split=soulEase((age-1.25f)/.12f);
    if(split<1.f)ribbon(0.f,1.f,8,opacity*(1.f-split),0.f,0.f,false);
    if(split>0.f)for(int i=0;i<3;++i) {
        const float scatter=hashUnit(i,1,s.seed);
        const float fade=1.f-soulEase((age-1.29f)/(.27f+.08f*scatter));
        const float from=i/3.f,to=(i+1)/3.f;
        if(fade>0.f)ribbon(from,to,2,opacity*split*fade*.74f,
            dissolve*(i-1)*(7.f+6.f*scatter),dissolve*(3.f+9.f*scatter),true);
    }
}
}

void resetRevenantSouls() {
    revenantSouls={};
    soulLevel=r_levelserial;
    soulClockTic=-1;soulFraction=0.f;
}

void revenantDeath(const mobj_t *revenant) {
    if(!settings.accelerated||!settings.revenantSoulRelease||gamestate!=GS_LEVEL||
       !revenant->subsector)return;
    if(soulLevel!=r_levelserial)resetRevenantSouls();
    for(auto &s:revenantSouls) {
        if(soulAlive(s,(leveltime-s.born)/float(TICRATE)))continue;
        s={units(revenant->x),units(revenant->y),units(revenant->z)+units(revenant->height)*.28f,
           leveltime,int(revenant->subsector->sector-sectors),
           hash32(std::uint32_t(revenant->x)^std::uint32_t(revenant->y)*0x9e3779b9u^
                  std::uint32_t(leveltime))};
        break;
    }
}

void drawRevenantSouls(const Uniforms &camera,float fraction) {
    if(soulLevel!=r_levelserial||camera.effects[2]!=0||gamestate!=GS_LEVEL)return;
    if(soulClockTic!=leveltime){soulClockTic=leveltime;soulFraction=0.f;}
    if(!paused&&!menuactive)soulFraction=std::max(soulFraction,fraction);
    struct Visible {const RevenantSoul *soul;float age,depth;};
    std::array<Visible,soulCapacity> visible;
    int count=0;
    for(const auto &s:revenantSouls) {
        const float age=(leveltime-1-s.born+soulFraction)/TICRATE;
        if(!soulAlive(s,age)||s.sector<0||s.sector>=numsectors)continue;
        const float dx=s.x-camera.eye[0],dy=s.y-camera.eye[1],dz=s.z-camera.eye[2];
        if(dx*dx+dy*dy+dz*dz>768.f*768.f)continue;
        const float depth=dx*camera.forward[0]+dy*camera.forward[1]+dz*camera.forward[2];
        if(depth>-64.f)visible[count++]={&s,age,depth};
    }
    if(!count||!cacheSoulImage())return;
    std::sort(visible.begin(),visible.begin()+count,[](const Visible &a,const Visible &b) {
        return a.depth>b.depth;
    });
    for(int i=0;i<count;++i)drawSoul(camera,*visible[i].soul,visible[i].age);
}

#ifdef DOOM_TEST_DRIVER
extern "C" int I_TestRevenantSouls() {
    if(soulLevel!=r_levelserial)return 0;
    return int(std::count_if(revenantSouls.begin(),revenantSouls.end(),[](const RevenantSoul &s) {
        return soulAlive(s,(leveltime-s.born)/float(TICRATE));
    }));
}
#endif
